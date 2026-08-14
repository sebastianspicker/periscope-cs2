#pragma once
#include <cstdint>

namespace real::win {

/// Zero MZ/PE signatures in the main image header page (after saving a copy).
void erase_pe_header() noexcept;

/// Restore the previously saved header page.
void restore_pe_header() noexcept;

/// True if erase_pe_header is currently active.
bool pe_header_erased() noexcept;

}  // namespace real::win
