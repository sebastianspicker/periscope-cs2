# DMA scatter-read policy integration (ledger R10)

Status: portable policies implemented and tested. The live adapter wiring is
not implemented in this pass; the scope boundary below is authoritative.

## What is already portable (tested, no hardware)

| Policy | Where | Test |
|---|---|---|
| Scatter batching ≤ in-flight capacity | `src/domain/radar/scatter_batch.hpp` (`plan_scatter_batches`) | `tests/radar_scatter_test.cpp` |
| Pointer-cache revalidation classes (unchanged / moved / generation gap / read failed) | `classify_revalidation` | same |
| Latency model incl. stall penalty for oversized batches | `DeviceLatencyModel` | same |
| Bulk-recovery window semantics | `within_bulk_recovery_window` | same |
| Dual-worker publication (seqlock ring of 8) | `src/domain/radar/snapshot_ring.hpp` | `tests/radar_ring_test.cpp` |
| Cadence constants (camera 300 Hz / data 60 Hz) | `src/domain/radar/intervals.hpp` | `tests/radar_geometry_test.cpp` |

## Required live-adapter rules (upstream evidence, ledger R10)

1. Batch sizing (oasis): never issue more scatter reads than the FPGA can
   keep in flight; reads beyond that stall the pipe for several milliseconds.
   The adapter must chunk with `plan_scatter_batches` using the device's
   measured in-flight capacity, not a constant guess.
2. Pointer-cache revalidation (oasis): keep each entity-slot's resolved
   pointer between frames; every frame re-reads pointer + payload in the same
   batch and only consumes the payload when `classify_revalidation` returns
   `Unchanged`. `Moved` entries are re-resolved next frame; `GenerationGap`
   is treated as respawn; `ReadFailed` drops the slot for the frame.
3. Dual-device split (oasis): action/entity reads and camera reads use
   separate FPGA device indexes to avoid USB contention; the camera worker
   runs at `kCameraHz`, the data worker at `kDataHz`.
4. Recovery windows (cs2-dma): after a bulk eviction (scene change,
   device reset) consumers hold the last good frame for
   `within_bulk_recovery_window` instead of flapping; recovery state lives in
   exactly one place (the data worker), never in a second cache.
5. Scene watermark: every batch is stamped with the current
   `SceneWatermark` serial; results from an older serial are discarded
   wholesale.

## Deliberate scope boundary

The adapter wiring itself is not implemented in this pass: the real DMA
adapter area is excluded from ordinary review (audit posture), and wiring new
hardware-facing code without that review would expand the unreviewed surface.
When the lab review window opens, the adapter should consume the policies
above and add a real-flag-gated integration test; until then the portable
policies are the reviewed, tested half of R10.
