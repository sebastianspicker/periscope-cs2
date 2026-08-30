# strip_rich_header.cmake
# Post-build PE hardening: strips Rich Header + zeroes TimeDateStamp.
#
# The MSVC linker embeds a Rich Header (XOR-obfuscated compiler fingerprint:
# product ID, version, build number, and all linked library checksums).
# VAC uses this as the #1 binary identifier. The PE TimeDateStamp leaks
# the exact build time.
#
# This script reads the PE binary, patches both fields in-place, and writes
# the result to a temp file before overwriting the original (safe on failure).

if(NOT input)
  message(FATAL_ERROR "Usage: cmake -P strip_rich_header.cmake -Dinput=<path_to_pe>")
endif()
if(NOT EXISTS "${input}")
  message(STATUS "File not found, skipping: ${input}")
  return()
endif()

# Read the PE as raw binary bytes (HEX mode, no encoding issues)
file(READ "${input}" hex HEX)
string(LENGTH "${hex}" hex_len)

# Locate Rich Header marker ("Rich" = 52 69 63 68)
string(FIND "${hex}" "52696368" RICH_POS)
if(RICH_POS LESS 0)
  message(STATUS "No Rich Header found: ${input}")
  set(RICH_FOUND 0)
else()
  set(RICH_FOUND 1)
endif()

# Locate PE signature ("PE\0\0" = 50 45 00 00)
string(FIND "${hex}" "50450000" PE_POS)
if(PE_POS LESS 0)
  message(FATAL_ERROR "PE signature not found: ${input}")
endif()

if(RICH_FOUND AND PE_POS GREATER RICH_POS)
  # Zero bytes from Rich marker through PE signature
  math(EXPR ZERO_LEN "${PE_POS} - ${RICH_POS} + 8")
  string(REPEAT "00" ${ZERO_LEN} ZERO_BLOCK)

  string(SUBSTRING "${hex}" 0 "${RICH_POS}" HEAD)
  string(SUBSTRING "${hex}" "${PE_POS}" -1 TAIL)
  set(hex "${HEAD}${ZERO_BLOCK}${TAIL}")
  message(STATUS "Rich Header stripped (${ZERO_LEN} hex bytes)")
endif()

# Zero PE TimeDateStamp
# Read e_lfanew from offset 0x3C (4 bytes LE)
string(SUBSTRING "${hex}" 120 8 LFA_HEX)
# Swap LE bytes for arithmetic
string(SUBSTRING "${LFA_HEX}" 0 2 B0)
string(SUBSTRING "${LFA_HEX}" 2 2 B1)
string(SUBSTRING "${LFA_HEX}" 4 2 B2)
string(SUBSTRING "${LFA_HEX}" 6 2 B3)
string(CONCAT LFA "${B3}${B2}${B1}${B0}")
math(EXPR LFA_DEC "0x${LFA}")
# Timestamp is at LFA + 4 (sig) + 2 (Machine) + 2 (NumberOfSections)
math(EXPR TS_BYTE "${LFA_DEC} + 8")
math(EXPR TS_HEX "${TS_BYTE} * 2")

string(SUBSTRING "${hex}" 0 "${TS_HEX}" HEAD2)
string(SUBSTRING "${hex}" "${TS_HEX}" -1 TAIL2)
set(hex "${HEAD2}00000000${TAIL2}")
message(STATUS "TimeDateStamp zeroed at byte offset ${TS_BYTE}")

# Verify length unchanged (safety check)
string(LENGTH "${hex}" new_len)
if(NOT hex_len EQUAL new_len)
  message(FATAL_ERROR "PE length mismatch after patching: ${hex_len} -> ${new_len}")
endif()

# Write to temp file first, then rename (atomic on same filesystem)
set(tmp "${input}.patched")
file(WRITE "${tmp}" "${hex}")
if(NOT EXISTS "${tmp}")
  message(FATAL_ERROR "Failed to write patched PE: ${tmp}")
endif()
file(RENAME "${tmp}" "${input}")
message(STATUS "Patched: ${input} (${new_len} hex bytes)")
