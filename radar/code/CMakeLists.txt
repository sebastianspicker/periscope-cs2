cmake_minimum_required(VERSION 3.20)
project(ac_sim LANGUAGES C CXX)
# MASM only on MSVC (Windows real-stack .asm helpers); not available on Linux CI.
if(MSVC)
  enable_language(ASM_MASM)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

string(RANDOM LENGTH 15 ALPHABET "0123456789ABCDEF" _XOR_HEX)
math(EXPR XOR_KEY_SEED "0x${_XOR_HEX}")
string(RANDOM LENGTH 12 ALPHABET "0123456789ABCDEF" _TIMING_HEX)
math(EXPR TIMING_SALT "0x${_TIMING_HEX}")
string(RANDOM LENGTH 8 ALPHABET "0123456789ABCDEF" _CLASS_HEX)
math(EXPR CLASS_SEED "0x${_CLASS_HEX}")
string(RANDOM LENGTH 10 ALPHABET "0123456789ABCDEF" _API_HEX)
math(EXPR API_SALT "0x${_API_HEX}")
# Keep hex length <= 15 so CMake math(EXPR) stays within signed 64-bit.
string(RANDOM LENGTH 15 ALPHABET "0123456789ABCDEF" _COMPILE_HEX)
math(EXPR COMPILE_SALT "0x${_COMPILE_HEX}")
string(RANDOM LENGTH 6 ALPHABET "0123456789ABCDEF" _SECTION_HEX)
math(EXPR SECTION_SEED "0x${_SECTION_HEX}")
string(RANDOM LENGTH 8 ALPHABET "0123456789ABCDEF" _GUID_HEX)
math(EXPR GUID_SEED "0x${_GUID_HEX}")
string(RANDOM LENGTH 6 ALPHABET "0123456789ABCDEF" _SURFACE_HEX)
math(EXPR SURFACE_SEED "0x${_SURFACE_HEX}")
option(LR_BUILD_ID "Build identifier for watermarking" 0)
if(LR_BUILD_ID)
    add_definitions(-DBUILD_ID=${LR_BUILD_ID})
endif()
message(STATUS "Per-build XOR seed: ${XOR_KEY_SEED} CLASS:${CLASS_SEED} API:${API_SALT} SECTION:${SECTION_SEED} GUID:${GUID_SEED} SURFACE:${SURFACE_SEED} BUILD_ID:${LR_BUILD_ID}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_keys.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/build_keys.hpp" @ONLY)

option(LAB_ALLOW_HIJACK_PROXY "Simulate hijack/proxy acquisition" OFF)
option(LAB_ALLOW_KERNEL "Simulate kernel driver (T2)" OFF)
option(LAB_ALLOW_HV "Simulate hypervisor (T3)" OFF)
option(LAB_ALLOW_DMA "Simulate DMA (T4)" OFF)
option(LAB_ALLOW_BAND4 "Simulate band-4 overlay" OFF)
option(LAB_ALLOW_SEDEBUG "Simulate SeDebugPrivilege use" OFF)
foreach(opt LAB_ALLOW_HIJACK_PROXY LAB_ALLOW_KERNEL LAB_ALLOW_HV LAB_ALLOW_DMA LAB_ALLOW_BAND4 LAB_ALLOW_SEDEBUG LR_ENABLE_VEH)
  if(${opt})
    add_compile_definitions(${opt}=1)
  endif()
endforeach()

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  # /DYNAMICBASE and /guard:cf are LINKER flags — putting them in CXX flags makes
  # cl.exe treat /DYNAMICBASE as /D YNAMICBASE (bogus macro).
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG /GS /GR-")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DYNAMICBASE /guard:cf /HIGHENTROPYVA /NXCOMPAT /PDBSTRIPPED /PDBALTPATH:NUL /CETCOMPAT /MANIFEST:NO")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections")
  if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
  else()
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
  endif()
endif()
option(LR_BUILD_TESTS "Build tests" ON)
option(LR_ENABLE_VEH "Enable VEH anti-debug handler (kernel-visible)" OFF)
option(LR_BUILD_PROTOS "Build per-tier proto demos" ON)
option(LR_BUILD_DUELS "Build per-tier duel demos" ON)
option(LR_BUILD_STRATEGY_LAB "Build strategy_lab catalog runner" ON)

set(LR_PUBLIC_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/lib" "${CMAKE_CURRENT_SOURCE_DIR}/teams" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
function(lr_library target)
  if(NOT ARGN)
    message(FATAL_ERROR "${target} has no sources in the new layout")
  endif()
  add_library(${target} ${ARGN})
  target_include_directories(${target} PUBLIC ${LR_PUBLIC_INCLUDE_DIRS})
endfunction()
function(lr_exe target source)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    message(FATAL_ERROR "${target} source is missing: ${source}")
  endif()
  add_executable(${target} "${source}")
  target_include_directories(${target} PRIVATE ${LR_PUBLIC_INCLUDE_DIRS})
  target_link_libraries(${target} PRIVATE ${ARGN})
endfunction()
function(lr_glob_library target pattern)
  file(GLOB sources CONFIGURE_DEPENDS "${pattern}")
  lr_library(${target} ${sources})
endfunction()

lr_glob_library(ac_common "${CMAKE_CURRENT_SOURCE_DIR}/lib/ac/*.cpp")
file(GLOB AC_SIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/sim/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/ac_sim/*.cpp")
lr_library(ac_sim ${AC_SIM_SOURCES})
target_link_libraries(ac_sim PUBLIC ac_common)
# Strategy support core: scar sensors, multi-reason, pair_util, catalog_util.
file(GLOB AC_STRATEGY_CORE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/strategies/*.cpp")
lr_library(ac_strategy_core ${AC_STRATEGY_CORE})
target_link_libraries(ac_strategy_core PUBLIC ac_common ac_sim)
# Radar pipeline: integrates ALL ac_sim + periscope features into a coherent frame loop
lr_library(ac_pipeline
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/radar_pipeline.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/radar_pipeline_lifecycle.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/radar_pipeline_collect.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/radar_pipeline_frame.cpp"
)
target_link_libraries(ac_pipeline PUBLIC ac_sim ac_common)
# ac_real is defined later; link is completed after ac_real is created.
lr_glob_library(ac_blue "${CMAKE_CURRENT_SOURCE_DIR}/lib/blue/*.cpp")
target_link_libraries(ac_blue PUBLIC ac_common ac_sim)
lr_glob_library(ac_server "${CMAKE_CURRENT_SOURCE_DIR}/lib/server/*.cpp")
target_link_libraries(ac_server PUBLIC ac_common)
lr_glob_library(ac_depth "${CMAKE_CURRENT_SOURCE_DIR}/lib/depth/*.cpp")
target_link_libraries(ac_depth PUBLIC ac_sim ac_server ac_common)
lr_glob_library(ac_fps "${CMAKE_CURRENT_SOURCE_DIR}/lib/fps/*.cpp")
target_link_libraries(ac_fps PUBLIC ac_common ac_sim)
lr_library(ac_cs2_signatures
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures_data.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures_scan.cpp")
target_link_libraries(ac_cs2_signatures PUBLIC ac_common)
file(GLOB AC_CS2 CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/*.cpp")
# Signature corpus TUs live only on ac_cs2_signatures (not ac_cs2).
list(REMOVE_ITEM AC_CS2
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures_data.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/cs2/signatures_scan.cpp")
lr_library(ac_cs2 ${AC_CS2})
target_link_libraries(ac_cs2 PUBLIC ac_sim ac_common ac_cs2_signatures)
# AOB scanner is a light dependency (CS2 signatures + sim). Scan detector and the
# rest of lab/ live in ac_lab so backend stats can correlate without cycles.
set(AC_LAB_SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/lib/lab/aob_scanner.cpp")
lr_library(ac_lab_scanner ${AC_LAB_SCANNER})
target_link_libraries(ac_lab_scanner PUBLIC ac_cs2_signatures ac_sim)
file(GLOB AC_LAB CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lib/lab/*.cpp")
list(REMOVE_ITEM AC_LAB ${AC_LAB_SCANNER})
lr_library(ac_lab ${AC_LAB})
target_link_libraries(ac_lab PUBLIC ac_common ac_sim ac_lab_scanner ac_cs2_signatures)

function(lr_team target name)
  lr_glob_library(${target} "${CMAKE_CURRENT_SOURCE_DIR}/teams/${name}/*.cpp")
endfunction()
lr_team(ac_t0_red t0_red)
target_link_libraries(ac_t0_red PUBLIC ac_common ac_lab ac_sim)
# ac_real is defined later; PUBLIC link is completed after ac_real is created.
lr_team(ac_t0_blue t0_blue)
target_link_libraries(ac_t0_blue PUBLIC ac_common ac_sim)
lr_team(ac_t1_red t1_red)
target_link_libraries(ac_t1_red PUBLIC ac_t0_red)
lr_team(ac_t1_blue t1_blue)
target_link_libraries(ac_t1_blue PUBLIC ac_t0_blue ac_sim)
lr_team(ac_t2_red t2_red)
target_link_libraries(ac_t2_red PUBLIC ac_t0_red)
lr_team(ac_t2_blue t2_blue)
target_link_libraries(ac_t2_blue PUBLIC ac_common ac_sim ac_t2_red)
lr_team(ac_t3_red t3_red)
target_link_libraries(ac_t3_red PUBLIC ac_t2_red ac_t1_red ac_t0_red)
lr_team(ac_t3_blue t3_blue)
target_link_libraries(ac_t3_blue PUBLIC ac_t2_blue ac_common ac_sim ac_depth)
lr_team(ac_t4_red t4_red)
target_link_libraries(ac_t4_red PUBLIC ac_common ac_sim)
lr_team(ac_t4_blue t4_blue)
target_link_libraries(ac_t4_blue PUBLIC ac_common ac_sim ac_depth ac_server)

set(LR_REAL_DEFAULT OFF)
option(LR_ENABLE_REAL_RPM "Build real Windows ReadProcessMemory backend" ${LR_REAL_DEFAULT})
option(LR_ENABLE_REAL_SYSCALL "Build real direct syscall backend" ${LR_REAL_DEFAULT})
option(LR_ENABLE_REAL_KERNEL "Build real kernel/BYOVD backend" OFF)
option(LR_ENABLE_REAL_VMX "Build real Intel VT-x hypervisor backend" OFF)
option(LR_ENABLE_REAL_DMA "Build real DMA/PCIe backend" OFF)
option(LR_ENABLE_REAL_SMM "Build real SMM/firmware backend" OFF)
option(LR_ENABLE_REAL_GPU "Build real GPU rendering/compute backends" OFF)
option(LR_ENABLE_REAL_NET "Build real networking/C2 backends" ${LR_REAL_DEFAULT})
option(LR_ENABLE_REAL_UEFI "Build UEFI firmware backend" ${LR_REAL_DEFAULT})
option(LR_ENABLE_REAL_LINUX "Build Linux real backend and host stubs" ${LR_REAL_DEFAULT})
option(LR_ENABLE_REAL_ALL "Enable ALL real backends (use with care)" OFF)
if(LR_ENABLE_REAL_ALL)
  foreach(opt RPM SYSCALL KERNEL VMX DMA SMM GPU NET UEFI LINUX)
    set(LR_ENABLE_REAL_${opt} ON)
  endforeach()
endif()
# ── Extended stealth features (gated by LR_ENABLE_* pattern) ──
option(LR_ENABLE_SHELLCODE_INJECTION "Enable shellcode injection support" OFF)
option(LR_ENABLE_XORSTR_OBFUSCATION  "Enable XOR string obfuscation at build time" ON)
option(LR_ENABLE_PEB_EAT_RESOLUTION "Enable PEB+EAT function resolution" ON)
option(LR_ENABLE_HIJACK_READER      "Enable donor hijack memory reader" ${LR_REAL_DEFAULT})
option(LR_ENABLE_PERISCOPE_ENTITY   "Enable Periscope entity collector" ${LR_REAL_DEFAULT})
option(LR_ENABLE_PERISCOPE_HUD      "Enable Periscope HUD radar reader" ${LR_REAL_DEFAULT})
option(LR_ENABLE_PERISCOPE_RADAR    "Enable Periscope coordinate math" ${LR_REAL_DEFAULT})
option(LR_ENABLE_PERISCOPE_OVERLAY  "Enable Periscope stealth overlay" ${LR_REAL_DEFAULT})
option(LR_ENABLE_BATCH_READ_JITTER  "Enable batch read with temporal jitter" ${LR_REAL_DEFAULT})
option(LR_ENABLE_MEMORY_HIDE        "Enable PEB unlinking / memory hide" OFF)
option(LR_ENABLE_DECOY_RENDER       "Enable decoy overlay window rendering" OFF)
option(LR_ENABLE_TEMPORAL_PHASE     "Enable 4-phase temporal jitter engine" ${LR_REAL_DEFAULT})
if(LR_ENABLE_REAL_ALL)
  foreach(opt HIJACK_READER PERISCOPE_ENTITY PERISCOPE_HUD PERISCOPE_RADAR PERISCOPE_OVERLAY BATCH_READ_JITTER TEMPORAL_PHASE)
    set(LR_ENABLE_${opt} ON)
  endforeach()
endif()
set(LR_REAL_ENABLED OFF)
foreach(opt RPM SYSCALL KERNEL VMX DMA SMM GPU NET UEFI LINUX)
  if(LR_ENABLE_REAL_${opt})
    set(LR_REAL_ENABLED ON)
    add_compile_definitions(LR_HAS_REAL_${opt}=1)
  endif()
endforeach()
if(APPLE AND LR_REAL_ENABLED)
  message(FATAL_ERROR "Real backend features are unsupported on Apple. Leave LR_ENABLE_REAL_* options OFF.")
endif()
if(WIN32)
  add_compile_definitions(LR_PLATFORM_WINDOWS=1 LR_PLATFORM_LINUX=0)
  set(REAL_PLATFORM_LIBS "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_definitions(LR_PLATFORM_WINDOWS=0 LR_PLATFORM_LINUX=1)
  set(REAL_PLATFORM_LIBS pthread dl rt)
else()
  add_compile_definitions(LR_PLATFORM_WINDOWS=0 LR_PLATFORM_LINUX=0)
endif()
if(LR_REAL_ENABLED)
  file(GLOB REAL_PLATFORM CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/mode/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/cs2/*.cpp")
  if(WIN32)
    file(GLOB REAL_WIN CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/win/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/win/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/win/*.asm")
    list(APPEND REAL_PLATFORM ${REAL_WIN})
    # Always build the render pipeline factory on Windows so radar demos can
    # link without requiring -DLR_ENABLE_REAL_GPU (D3D11 path + Null fallback).
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/render_pipeline.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/render_pipeline_null.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/render_pipeline_d3d11_lifecycle.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/render_pipeline_d3d11_draw.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/render_pipeline_overlay_place.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/periscope_overlay.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/periscope_overlay_window.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/periscope_overlay_scramble.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/periscope_overlay_dxgi.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/gui.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/gui_widgets.cpp")
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/gpu/gui_radar_panel.cpp")
    # Educational kernel IOCTL stubs are always linked so IoctlBackend resolves
    # even when LR_ENABLE_REAL_KERNEL is OFF (full driver stack is opt-in).
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/kernel/ioctl_interface.cpp")
  endif()
  # VMX CPUID probes used by mode::t3_available — always available as lightweight stubs.
  if(NOT LR_ENABLE_REAL_VMX)
    list(APPEND REAL_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/vmx/cpuid.cpp")
  endif()
  lr_library(ac_real_platform ${REAL_PLATFORM})
  target_link_libraries(ac_real_platform PUBLIC ac_common ${REAL_PLATFORM_LIBS})
  if(WIN32)
    # periscope_overlay / DxgiComposite call D3D11CreateDevice; always resolve.
    target_link_libraries(ac_real_platform PUBLIC d3d11 dxgi dxguid)
  endif()
endif()
function(lr_real target directory)
  lr_glob_library(${target} "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/${directory}/*.cpp")
  target_link_libraries(${target} PUBLIC ac_real_platform ${ARGN})
endfunction()
if(LR_ENABLE_REAL_KERNEL)
  # ac_real_platform already compiles ioctl_interface.cpp on Windows so
  # IoctlBackend links without the full stack; exclude it from the kernel
  # library to avoid duplicate symbols when LR_ENABLE_REAL_KERNEL=ON.
  file(GLOB KERNEL_SOURCES CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/kernel/*.cpp")
  if(WIN32)
    list(FILTER KERNEL_SOURCES EXCLUDE REGEX "ioctl_interface\\.cpp$")
  endif()
  lr_library(ac_real_kernel ${KERNEL_SOURCES})
  target_link_libraries(ac_real_kernel PUBLIC ac_real_platform ac_common)
  if(WIN32)
    target_link_libraries(ac_real_kernel PUBLIC psapi advapi32)
  endif()
endif()
if(LR_ENABLE_REAL_VMX)
  lr_real(ac_real_vmx vmx)
endif()
if(LR_ENABLE_REAL_DMA)
  lr_real(ac_real_dma dma ac_common ac_sim)
  if(WIN32)
    target_link_libraries(ac_real_dma PUBLIC setupapi cfgmgr32 advapi32)
  endif()
endif()
if(LR_ENABLE_REAL_GPU)
  # On Windows, render_pipeline.cpp is already part of ac_real_platform above.
  # Keep a dedicated target for non-Windows or when platform did not absorb it.
  if(NOT WIN32)
    lr_real(ac_real_gpu gpu)
  endif()
endif()
if(LR_ENABLE_REAL_NET)
  lr_real(ac_real_net net)
  if(WIN32)
    # WinHTTP (TLS HTTPS), BCrypt (CSPRNG), Crypt32 (cert pin), Winsock
    target_link_libraries(ac_real_net PUBLIC ws2_32 winhttp bcrypt crypt32)
  endif()
endif()
if(LR_ENABLE_REAL_SMM)
  lr_real(ac_real_smm smm)
  if(WIN32)
    # TBS / firmware table APIs used by tpm.cpp and acpi.cpp
    target_link_libraries(ac_real_smm PUBLIC advapi32)
  endif()
endif()
# UEFI firmware analysis stack — available on Linux and Windows (OS firmware APIs).
if(LR_ENABLE_REAL_UEFI)
  lr_real(ac_real_uefi uefi)
  if(WIN32)
    target_link_libraries(ac_real_uefi PUBLIC setupapi advapi32)
  endif()
endif()
# Linux real stack (mem/proc/pcie/kmod/sensors). Live syscalls on Linux;
# pure helpers + host stubs remain available through an explicit opt-in.
if(LR_ENABLE_REAL_LINUX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/real/linux/CMakeLists.txt")
  add_subdirectory(lib/real/linux)
endif()
if(TARGET ac_real_platform)
  add_library(ac_real INTERFACE)
  target_link_libraries(ac_real INTERFACE ac_real_platform)
  foreach(target ac_real_kernel ac_real_vmx ac_real_dma ac_real_gpu ac_real_net ac_real_smm ac_real_uefi ac_linux)
    if(TARGET ${target})
      target_link_libraries(ac_real INTERFACE ${target})
    endif()
  endforeach()
  set(LR_HAS_REAL_PLATFORM 1)
  # Late PUBLIC links: earlier targets were created before ac_real existed.
  # PUBLIC so dependents (demos/tests) also pull the Windows real backends.
  if(TARGET ac_pipeline)
    target_link_libraries(ac_pipeline PUBLIC ac_real)
  endif()
  if(TARGET ac_t0_red)
    target_link_libraries(ac_t0_red PUBLIC ac_real)
  endif()
  if(TARGET ac_t0_blue)
    target_link_libraries(ac_t0_blue PUBLIC ac_real)
  endif()
  # ac_sim's Windows translation units (temporal.cpp, api_integrity.cpp,
  # batch_engine.cpp, dll_watch.cpp, forensic.cpp, self_verify.cpp, disguise.cpp,
  # system_normalize.cpp, behavioral_filter.cpp) call real::win::fuzzed_sleep /
  # g_Api() / peb::get_peb() under LR_PLATFORM_WINDOWS. Link ac_real_platform so
  # any executable that pulls those objects resolves them even without an
  # explicit ac_real link.
  if(TARGET ac_sim)
    target_link_libraries(ac_sim PUBLIC ac_real_platform)
  endif()
else()
  set(LR_HAS_REAL_PLATFORM 0)
endif()
add_compile_definitions(LR_HAS_REAL_PLATFORM=${LR_HAS_REAL_PLATFORM})

lr_exe(fps_demo demos/fps_demo.cpp ac_fps ac_server)

# Standalone visualizations of the telemetry surfaces at each tier.
# Prefer real backends when available (CS2 attach + optional GPU overlay).
# Console demos use main() — do not use WIN32 subsystem (that requires WinMain).
foreach(tier 0 1 2 3 4)
  if(TARGET ac_real)
    lr_exe(radar_t${tier} demos/radar_t${tier}.cpp ac_real ac_common ac_sim)
  else()
    lr_exe(radar_t${tier} demos/radar_t${tier}.cpp ac_common ac_sim)
  endif()
endforeach()

# ── Live radar overlay demo (Gap 5 closure) ────────────────────────
# Build: cmake -S . -B build -DLR_ENABLE_REAL_RPM=ON
# Run:   ./build/live_radar (requires cs2.exe running)
if(TARGET ac_real)
  # live_radar is multi-TU after monolith split (support + loop + main).
  add_executable(live_radar
    "${CMAKE_CURRENT_SOURCE_DIR}/demos/live_radar.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/demos/live_radar_support.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/demos/live_radar_loop.cpp")
  target_include_directories(live_radar PRIVATE ${LR_PUBLIC_INCLUDE_DIRS})
  target_link_libraries(live_radar PRIVATE ac_real ac_sim ac_common)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/donor_worker.cpp")
    lr_exe(donor_worker demos/donor_worker.cpp ac_real ac_common)
  endif()
  # Temporary diagnostic probes for entity-list / radar layout (educational).
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/entity_probe.cpp")
    lr_exe(entity_probe demos/entity_probe.cpp ac_real ac_common)
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/radar_diag.cpp")
    lr_exe(radar_diag demos/radar_diag.cpp ac_real ac_common)
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/scale_probe.cpp")
    lr_exe(scale_probe demos/scale_probe.cpp ac_real ac_common)
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/hud_probe.cpp")
    lr_exe(hud_probe demos/hud_probe.cpp ac_real ac_common)
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demos/icvar_probe.cpp")
    lr_exe(icvar_probe demos/icvar_probe.cpp ac_real ac_common)
  endif()
  lr_exe(desktop_dup_overlay demos/desktop_dup_overlay.cpp ac_real)
  lr_exe(tier_comparison demos/tier_comparison.cpp ac_real ac_common ac_sim)
  lr_exe(radar_multi_tier demos/radar_multi_tier.cpp ac_real ac_common ac_sim)
  lr_exe(full_prototype demos/full_prototype.cpp ac_real ac_common ac_sim)
  lr_exe(gui_demo demos/gui_demo.cpp ac_real ac_common)
  if(TARGET ac_real_uefi)
    lr_exe(uefi_fw demos/uefi_firmware/main.cpp ac_real_uefi ac_common)
  endif()
else()
  # The comparison's simulation mode is intentionally available on macOS.
  lr_exe(tier_comparison demos/tier_comparison.cpp ac_common ac_sim)
endif()
# full_prototype uses strategies::catalog / family_name / supports_real_mode —
# link ac_strategies after that target exists (defined later in this file).

# ── CS2 signature auto-updater (Python) ────────────────────────────
# Pulls a2x/cs2-dumper offsets into data/cs2 + lib/real/cs2/offsets_snapshot.hpp
# Defined after demo targets so POST_BUILD install can attach.
set(LR_UPDATE_CS2_SIGS_PY "${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_cs2_signatures.py")
if(EXISTS "${LR_UPDATE_CS2_SIGS_PY}")
  add_custom_target(update_cs2_signatures
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1
            python "${LR_UPDATE_CS2_SIGS_PY}"
            --out-dir "${CMAKE_CURRENT_SOURCE_DIR}/data/cs2"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Updating CS2 offsets/signatures from a2x/cs2-dumper"
    VERBATIM)
  # Copy runtime JSON next to demos so they can hot-load without rebuild
  foreach(_cs2_demo live_radar radar_t0)
    if(TARGET ${_cs2_demo})
      add_custom_command(TARGET ${_cs2_demo} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${_cs2_demo}>/data/cs2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/data/cs2/offsets_snapshot.json"
                "$<TARGET_FILE_DIR:${_cs2_demo}>/data/cs2/offsets_snapshot.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/data/cs2/offsets_snapshot.json"
                "$<TARGET_FILE_DIR:${_cs2_demo}>/offsets_snapshot.json"
        COMMENT "Install CS2 offsets_snapshot.json next to ${_cs2_demo}"
        VERBATIM)
    endif()
  endforeach()
endif()

# ── Post-build PE hardening for ALL Windows executables ────────────
# Strips Rich Header and zeroes TimeDateStamp from compiled PE.
set(_PE_TARGETS radar_t0 radar_t1 radar_t2 radar_t3 radar_t4
                live_radar desktop_dup_overlay gui_demo full_prototype
                tier_comparison radar_multi_tier
                fps_demo evasion_lab features_lab ops_lab structural_lab
                cs2_radar_t0 cs2_radar_t1 cs2_radar_t2
                cs2_radar_t3 cs2_radar_t4
                uefi_fw host_sim strategy_lab)
if(TARGET user_test)
  list(APPEND _PE_TARGETS user_test)
endif()
foreach(target ${_PE_TARGETS})
  if(TARGET ${target} AND WIN32)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -Dinput="$<TARGET_FILE:${target}>"
                -P "${CMAKE_SOURCE_DIR}/cmake/strip_rich_header.cmake"
        COMMENT "Stripping Rich Header and timestamp from ${target}..."
        VERBATIM)
    endif()
  endif()
endforeach()
unset(_PE_TARGETS)
unset(_type)

if(LR_BUILD_PROTOS)
  foreach(tier 0 1 2 3 4)
    lr_exe(proto_t${tier}_red demos/proto_t${tier}_red.cpp ac_t${tier}_red)
    lr_exe(proto_t${tier}_blue demos/proto_t${tier}_blue.cpp ac_t${tier}_blue ac_t${tier}_red)
    add_executable(cs2_radar_t${tier} "${CMAKE_CURRENT_SOURCE_DIR}/demos/cs2_radar/t${tier}/cs2_radar.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/demos/cs2_radar/t${tier}/main.cpp")
    target_include_directories(cs2_radar_t${tier} PRIVATE ${LR_PUBLIC_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/demos/cs2_radar/t${tier}")
  endforeach()
  target_link_libraries(proto_t1_blue PRIVATE ac_t0_blue)
  target_link_libraries(proto_t2_blue PRIVATE ac_t0_blue)
  target_link_libraries(cs2_radar_t0 PRIVATE ac_pipeline ac_t0_red ac_t0_blue ac_cs2 ac_common ac_sim)
  if(TARGET ac_real)
    target_link_libraries(cs2_radar_t0 PRIVATE ac_real)
  endif()
  target_link_libraries(cs2_radar_t1 PRIVATE ac_t1_red ac_t0_red ac_t1_blue ac_cs2 ac_common ac_sim)

  target_link_libraries(cs2_radar_t2 PRIVATE ac_t2_red ac_t2_blue ac_cs2 ac_common ac_sim)
  target_link_libraries(cs2_radar_t3 PRIVATE ac_t3_red ac_t3_blue ac_cs2 ac_common ac_sim ac_depth)
  target_link_libraries(cs2_radar_t4 PRIVATE ac_t4_red ac_t4_blue ac_t0_red ac_t0_blue ac_cs2 ac_common ac_sim ac_depth ac_server)
endif()
if(LR_BUILD_DUELS)
  foreach(tier 0 1 2 3 4)
    lr_exe(duel_t${tier} demos/duel_t${tier}.cpp ac_t${tier}_red ac_t${tier}_blue)
  endforeach()
  target_link_libraries(duel_t1 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t2 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t3 PRIVATE ac_t2_red ac_t0_red ac_server)
  target_link_libraries(duel_t4 PRIVATE ac_server)
endif()
if(LR_BUILD_STRATEGY_LAB)
  file(GLOB_RECURSE AC_STRATEGIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/strategies/t0/**/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/strategies/t1/**/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/strategies/t2/**/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/strategies/t3/**/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/strategies/t4/**/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/strategies/crosscutting/**/*.cpp")
  list(APPEND AC_STRATEGIES
    "${CMAKE_CURRENT_SOURCE_DIR}/strategies/framework/registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/strategies/framework/registry_run.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/strategies/framework/registry_dual.cpp"
  )
  list(REMOVE_DUPLICATES AC_STRATEGIES)
  lr_library(ac_strategies ${AC_STRATEGIES})
  target_link_libraries(ac_strategies PUBLIC ac_strategy_core ac_common ac_sim ac_blue ac_server ac_depth ac_fps ac_cs2 ac_cs2_signatures ac_lab ac_lab_scanner ac_t0_red ac_t0_blue ac_t1_red ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue)
  if(TARGET ac_real)
    target_link_libraries(ac_strategies PUBLIC ac_real)
  endif()
  # Late link: full_prototype was created before ac_strategies existed.
  if(TARGET full_prototype)
    target_link_libraries(full_prototype PRIVATE ac_strategies)
  endif()
  lr_exe(strategy_lab strategies/framework/main.cpp ac_strategies)
  if(LR_BUILD_DUELS)
    lr_exe(evasion_lab demos/evasion_lab.cpp ac_strategies ac_sim ac_t0_red)
    lr_exe(features_lab demos/features_lab.cpp ac_strategies ac_sim)
    lr_exe(ops_lab demos/ops_lab.cpp ac_strategies ac_sim)
    lr_exe(structural_lab demos/structural_lab.cpp ac_strategies ac_sim ac_server ac_depth)
  endif()
endif()
add_custom_target(stealth-check COMMAND ${CMAKE_COMMAND} -E echo "=== EDUCATIONAL AUDIT ===" COMMAND ${CMAKE_COMMAND} -E echo "Educational audit: review binaries for detectable string patterns." COMMENT "Educational audit only; no operational commands are run")

if(LR_BUILD_TESTS)
  enable_testing()
  function(lr_test target)
    lr_exe(${target} tests/${target}.cpp ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
  endfunction()
  if(TARGET ac_real)
    lr_exe(ac_scaffold_smoke tests/smoke_test.cpp ac_lab ac_server ac_depth ac_t0_red ac_t0_blue ac_t1_red ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue ac_real)
  else()
    lr_exe(ac_scaffold_smoke tests/smoke_test.cpp ac_lab ac_server ac_depth ac_t0_red ac_t0_blue ac_t1_red ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue)
  endif()
  add_test(NAME scaffold_smoke COMMAND ac_scaffold_smoke)
  # Radar pipeline integration: API integrity, AcceptGate metrics, BehavioralFilter.
  if(TARGET ac_pipeline)
    lr_test(pipeline_unit_test ac_pipeline ac_sim ac_common)
    if(TARGET ac_real)
      target_link_libraries(pipeline_unit_test PRIVATE ac_real)
    elseif(TARGET ac_real_platform)
      target_link_libraries(pipeline_unit_test PRIVATE ac_real_platform)
    endif()
  endif()
  lr_test(sim_unit_test ac_sim ac_common)
  if(TARGET ac_real)
    lr_test(real_backend_test ac_real ac_sim ac_common)
  endif()
endif()
