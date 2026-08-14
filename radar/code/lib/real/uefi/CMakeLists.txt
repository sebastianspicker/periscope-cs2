cmake_minimum_required(VERSION 3.20)

add_library(ac_uefi STATIC
    uefi_phys.cpp
    uefi_fw.cpp
    uefi_var.cpp
    uefi_tpm.cpp
    uefi_dma.cpp
    uefi_lab.cpp
)
target_include_directories(ac_uefi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
if(WIN32)
  target_link_libraries(ac_uefi PUBLIC setupapi advapi32)
endif()
