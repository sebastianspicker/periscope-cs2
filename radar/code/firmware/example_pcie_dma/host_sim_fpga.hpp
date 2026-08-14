#pragma once

#include "host_sim_memory.hpp"
#include "fpga_dma.h"
#include "obf.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

void host_sim_log(const char* tag, const char* fmt, ...);
bool host_sim_quiet();
void host_sim_set_quiet(bool q);

inline double host_sim_now_sec() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

struct FpgaDevice {
    uint32_t version   = DMA_FW_VERSION;
    uint32_t status    = STATUS_READY;
    uint32_t ctrl      = 0;
    uint32_t iommu     = 0;
    uint32_t completed = 0;
    uint32_t dma_st    = DMA_STATUS_IDLE;
    uint32_t dma_err   = DMA_ERR_NONE;
    uint64_t desc_addr = 0;
    uint32_t desc_count = 0;
    uint64_t total_us   = 0;
    uint64_t total_bytes = 0;
    uint64_t tlp_count  = 0;
    std::array<uint8_t, DATA_BUFFER_SIZE> db{};

    bool iommu_enabled = false;
    const Cs2MemoryImage* mem = nullptr;   // bound target memory
    static FpgaDevice* s_inst;

    // C-callable DMA path (PhysReadFn) — routes through the active FPGA.
    static bool phys_cb(uint64_t pa, void* buf, size_t sz) {
        return s_inst ? s_inst->hw_read(pa, buf, sz) : false;
    }

    // Register write (host → FPGA)
    void wr(uint64_t off, uint32_t v) {
        switch (off) {
        case REG_SCRATCH: break;
        case REG_CTRL: ctrl = v; break;
        case REG_DESC_ADDR_LO: desc_addr = (desc_addr & ~0xFFFFFFFFULL) | v; break;
        case REG_DESC_ADDR_HI: desc_addr = (desc_addr & 0xFFFFFFFFULL) | (static_cast<uint64_t>(v) << 32); break;
        case REG_DESC_COUNT: desc_count = v; break;
        case REG_DMA_CTRL: if (v == 1) run_dma(); break;
        case REG_DMA_ABORT:
            dma_st = DMA_STATUS_IDLE;
            status &= ~(static_cast<uint32_t>(STATUS_DMA_BUSY |
                        STATUS_DMA_DONE | STATUS_DMA_ERROR));
            break;
        case REG_ACS_BYPASS: break;
        default: break;
        }
    }

    // Register read (host ← FPGA)
    uint32_t rd(uint64_t off) {
        switch (off) {
        case REG_VERSION: return version;
        case REG_STATUS: return status;
        case REG_CTRL: return ctrl;
        case REG_DMA_STATUS: return dma_st;
        case REG_DMA_ERROR: return dma_err;
        case REG_IOMMU_STATE: return iommu;
        case REG_DMA_LATENCY: return static_cast<uint32_t>(total_us);
        case REG_BYTES_TRANSFERRED: return static_cast<uint32_t>(total_bytes);
        case REG_DESC_COMPLETED: return completed;
        case REG_DESC_COUNT: return desc_count;
        case REG_TIMESTAMP: return static_cast<uint32_t>(host_sim_now_sec() * 1e6);
        default:
            if (off >= REG_DATA_BUFFER && off < REG_DATA_BUFFER + DATA_BUFFER_SIZE)
                return db[off - REG_DATA_BUFFER];
            return 0;
        }
    }

    // Boot sequence: link training + IOMMU detection
    void boot() {
        host_sim_log("FW",   "Hardened FPGA DMA initializing (fw 0x%08x)...", DMA_FW_VERSION);
        host_sim_log("FPGA", "Custom bitstream, vid=0x%04x did=0x%04x, PCIe gen2 x4",
            DMA_VENDOR_ID, DMA_DEVICE_ID);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        host_sim_log("PCIe", "Link up: width x4, speed 5.0 GT/s");
        if (iommu_enabled) {
            iommu = STATUS_IOMMU;
            host_sim_log("IOMMU", "ACTIVE - DMA blocked to unmapped pages");
        } else {
            host_sim_log("IOMMU", "DISABLED - all physical memory accessible");
        }
    }

    // The DMA read path — models a PCIe Memory Read TLP sequence.
    // IOMMU, if enabled, blocks every request.
    bool hw_read(uint64_t pa, void* buf, size_t sz) {
        if (iommu_enabled) {
            dma_err = DMA_ERR_IOMMU;
            status |= STATUS_DMA_ERROR;
            return false;
        }
        const uint64_t lat = static_cast<uint64_t>(
            DMA_READ_4KB_US * (static_cast<float>(sz) / 4096.0f + 0.05f));
        total_us += lat;
        tlp_count += (sz + DMA_MAX_PAYLOAD - 1) / DMA_MAX_PAYLOAD;
        total_bytes += sz;
        return mem ? mem->phys_read(pa, buf, sz) : false;
    }

    // Descriptor-ring bulk DMA. `ring` points at host-side descriptors.
    void run_dma(const DmaDescriptor* ring = nullptr, uint32_t count = 0) {
        host_sim_log("FPGA", "DOORBELL - starting DMA (%u descriptors)", count ? count : desc_count);
        if (iommu_enabled) {
            host_sim_log("IOMMU", "BLOCKED: IOMMU remapping active");
            dma_err = DMA_ERR_IOMMU;
            dma_st = DMA_STATUS_ERROR;
            status |= STATUS_DMA_ERROR;
            return;
        }
        if (!ring) ring = reinterpret_cast<const DmaDescriptor*>(desc_addr);
        const uint32_t n = count ? count : desc_count;
        if (!ring || n == 0) {
            dma_err = DMA_ERR_BAD_DESC;
            dma_st = DMA_STATUS_ERROR;
            status |= STATUS_DMA_ERROR;
            return;
        }

        status |= STATUS_DMA_BUSY;
        dma_st = DMA_STATUS_BUSY;
        completed = 0;
        dma_err = DMA_ERR_NONE;

        const auto start = std::chrono::steady_clock::now();
        for (uint32_t i = 0; i < n; ++i) {
            const auto& d = ring[i];
            if (d.flags & DMAF_NULL) continue;
            host_sim_log("PCIe", "TLP read PA=0x%016" PRIx64 " sz=%u fl=0x%x",
                d.src_addr, d.size, d.flags);

            if ((d.flags & DMAF_READ) && d.size && mem) {
                if (d.size > DATA_BUFFER_SIZE) { dma_err = DMA_ERR_BAD_DESC; break; }
                std::vector<uint8_t> tmp(d.size);
                if (!mem->phys_read(d.src_addr, tmp.data(), d.size)) {
                    dma_err = DMA_ERR_MASTER_ABORT;
                    break;
                }
                // Copy into FPGA data buffer at dst offset.
                const uint64_t dst = d.dst_addr < DATA_BUFFER_SIZE ? d.dst_addr : 0;
                std::memcpy(db.data() + dst, tmp.data(), d.size);
                total_bytes += d.size;
                tlp_count += (d.size + DMA_MAX_PAYLOAD - 1) / DMA_MAX_PAYLOAD;
            }

            // Latency model: fetch + transfer
            const uint64_t lat = static_cast<uint64_t>(
                DMA_DESC_FETCH_US + DMA_READ_4KB_US *
                (static_cast<float>(d.size) / 4096.0f));
            total_us += lat;
            std::this_thread::sleep_for(std::chrono::microseconds(lat));
            completed++;
            if (d.flags & DMAF_LAST) break;
        }

        const auto end = std::chrono::steady_clock::now();
        total_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        dma_st = DMA_STATUS_DONE;
        status = (status & ~static_cast<uint32_t>(STATUS_DMA_BUSY)) | STATUS_DMA_DONE;
        host_sim_log("FPGA", "DMA done: %u/%u desc, %" PRIu64 " us, %" PRIu64 " bytes",
            completed, n, total_us, total_bytes);
    }
};
