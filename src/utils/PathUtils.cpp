#include "PathUtils.h"
#include <string>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <libgen.h>
#elif defined(_WIN32)
#include <windows.h>
#include <libgen.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif

namespace Utils {
std::string GetExecutableDir() {
    char path[1024];
#if defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::string(dirname(path));
    }
#elif defined(_WIN32)
    DWORD size = GetModuleFileNameA(NULL, path, sizeof(path));
    if (size > 0 && size < sizeof(path)) {
        return std::string(dirname(path));
    }
#else
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    if (count != -1) {
        path[count] = '\0';
        return std::string(dirname(path));
    }
#endif
    return "";
}
} // namespace Utils
