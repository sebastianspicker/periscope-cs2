#include "real/error.hpp"
#include "real/platform.hpp"

#include <cerrno>
#include <cstring>
#include <string>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

namespace real {

std::string format_os_error(unsigned long code) {
#if LR_PLATFORM_WINDOWS
  LPSTR message = nullptr;
  const DWORD length = ::FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(code),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&message), 0, nullptr);
  if (length == 0 || message == nullptr) {
    return "Windows error " + std::to_string(code);
  }

  std::string result(message, length);
  ::LocalFree(message);
  while (!result.empty() &&
         (result.back() == '\n' || result.back() == '\r' || result.back() == ' ' ||
          result.back() == '\t')) {
    result.pop_back();
  }
  return result;
#elif LR_PLATFORM_LINUX
  char buffer[256]{};
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
  return ::strerror_r(static_cast<int>(code), buffer, sizeof(buffer));
#else
  if (::strerror_r(static_cast<int>(code), buffer, sizeof(buffer)) != 0) {
    return "errno " + std::to_string(code);
  }
  return buffer;
#endif
#else
  return "OS error " + std::to_string(code);
#endif
}

Result<void> os_error(const char* context) {
#if LR_PLATFORM_WINDOWS
  const unsigned long code = ::GetLastError();
#elif LR_PLATFORM_LINUX
  const unsigned long code = static_cast<unsigned long>(errno);
#else
  const unsigned long code = 0;
#endif
  std::string msg = std::string(context != nullptr ? context : "OS operation") +
                    ": " + format_os_error(code);
  return Result<void>(msg);
}

}  // namespace real
