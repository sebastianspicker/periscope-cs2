# Makefile — WDK driver build + MSVC usermode/host tests
#
# Usage:
#   make test          # usermode client only (no WDK)
#   make host_tests    # host ABI unit tests
#   make driver        # kernel .sys (requires WDK km + WDM env)
#   make all           # test + host_tests (+ driver if WDK present)
#
# WDK: set WDM or WDK_ROOT to the Windows Kits\10 root.

WDK_ROOT     ?= $(WDM)
ifeq ($(WDK_ROOT),)
WDK_ROOT     = C:\Program Files (x86)\Windows Kits\10
endif

BUILD_CFG    = Release
ARCH         = x64

DRV_SRC      = vuln_driver.c
DRV_OBJ      = vuln_driver.obj
DRV_TARGET   = vuln_driver.sys
DRV_CERT     = MyTestCert

TEST_SRC     = user_test.cpp
TEST_TARGET  = user_test.exe
HOST_SRC     = host_tests.cpp
HOST_TARGET  = gdrv_host_tests.exe

CC           = cl.exe
LINK         = link.exe
SIGNTOOL     = signtool.exe

# Prefer a known kit with km if present (override with WDK_VER=...)
WDK_VER     ?= 10.0.26100.0
WDK_INC      = $(WDK_ROOT)\Include\$(WDK_VER)
WDK_LIB      = $(WDK_ROOT)\Lib\$(WDK_VER)\km\x64

INCLUDES     = /I"$(WDK_INC)\km" /I"$(WDK_INC)\km\crt" /I"$(WDK_INC)\shared" /I.

DRV_CFLAGS   = /c /nologo /W3 /O2 /Gy /GR- /GS- /kernel \
               /Fo$(DRV_OBJ) \
               /D"_AMD64_=1" /D"AMD64=1" \
               /D"NTDDI_VERSION=0x0A00000A" \
               /D"_WIN32_WINNT=0x0A00" \
               $(INCLUDES)

DRV_LFLAGS   = /driver /subsystem:native /entry:DriverEntry \
               /nologo /incremental:no /opt:ref /opt:icf \
               /machine:x64 \
               "$(WDK_LIB)\ntoskrnl.lib" \
               "$(WDK_LIB)\hal.lib"

TEST_CFLAGS  = /nologo /O2 /EHsc /std:c++20 /Fe$(TEST_TARGET) /I. \
               /D"_CRT_SECURE_NO_WARNINGS" /D"WIN32_LEAN_AND_MEAN"
TEST_LFLAGS  = advapi32.lib psapi.lib

HOST_CFLAGS  = /nologo /O2 /EHsc /std:c++20 /Fe$(HOST_TARGET) /I. \
               /D"_CRT_SECURE_NO_WARNINGS"

.PHONY: all driver test host_tests clean install load unload sign full_test help

all: test host_tests

help:
	@echo gdrv.sys BYOVD Makefile
	@echo   make test         Build user_test.exe
	@echo   make host_tests   Build gdrv_host_tests.exe
	@echo   make driver       Build vuln_driver.sys (needs WDK km)
	@echo   make install/load/unload/sign/full_test

driver: $(DRV_TARGET)

test: $(TEST_TARGET)

host_tests: $(HOST_TARGET)

$(DRV_OBJ): $(DRV_SRC) gdrv_abi.h
	$(CC) $(DRV_CFLAGS) $(DRV_SRC)

$(DRV_TARGET): $(DRV_OBJ)
	$(LINK) $(DRV_LFLAGS) $(DRV_OBJ) /out:$(DRV_TARGET)
	@echo === Driver built: $(DRV_TARGET) ===

$(TEST_TARGET): $(TEST_SRC) gdrv_abi.h gdrv_pack.hpp
	$(CC) $(TEST_CFLAGS) $(TEST_SRC) /link $(TEST_LFLAGS)
	@echo === Test tool built: $(TEST_TARGET) ===

$(HOST_TARGET): $(HOST_SRC) gdrv_abi.h gdrv_pack.hpp
	$(CC) $(HOST_CFLAGS) $(HOST_SRC)
	@echo === Host tests built: $(HOST_TARGET) ===

sign: $(DRV_TARGET)
	$(SIGNTOOL) sign /v /s PrivateCertStore /n $(DRV_CERT) \
		/t http://timestamp.digicert.com $(DRV_TARGET)

install: $(DRV_TARGET)
	copy /Y $(DRV_TARGET) C:\Windows\System32\drivers\$(DRV_TARGET)

load:
	- sc stop gdrv 2>nul
	- sc delete gdrv 2>nul
	sc create gdrv type=kernel binPath=C:\Windows\System32\drivers\$(DRV_TARGET)
	sc start gdrv
	@echo === Driver loaded. Device: \\.\gdrv ===

unload:
	- sc stop gdrv 2>nul
	- sc delete gdrv 2>nul

full_test: driver sign install load
	$(TEST_TARGET) --scan
	- $(TEST_TARGET) --entities
	$(TEST_TARGET) --processes
	- sc stop gdrv 2>nul
	- sc delete gdrv 2>nul

clean:
	-del /Q $(DRV_OBJ) $(DRV_TARGET) $(TEST_TARGET) $(HOST_TARGET) *.obj *.exp *.lib 2>nul
