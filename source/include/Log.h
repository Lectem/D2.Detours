
#include <Windows.h>
#include <fmt/format.h>
#define LOG(fmtstr, ...) OutputDebugStringA(fmt::format(LOG_PREFIX fmtstr, __VA_ARGS__).c_str())
#define LOGW(fmtstr, ...) OutputDebugStringW(fmt::format(LOG_PREFIX fmtstr, __VA_ARGS__).c_str())
