#include <tarp/ioutils.hxx>
#include <tarp/string_utils.hxx>

#include <cerrno>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace tarp {
namespace utils {
namespace io {

std::pair<bool, std::string> touch(const std::string &fpath) {
    using namespace std::string_literals;

    struct stat status;

    // create file if it doesn't exist, and return.
    int ret = ::stat(fpath.c_str(), &status);
    if (ret < 0) {
        if (errno != ENOENT) {
            return {false, "stat():"s + str::strerr()};
        }

        FILE *f = NULL;
        if (!(f = fopen(fpath.c_str(), "a"))) {
            return {false, "fopen(): "s + str::strerr()};
        }

        fclose(f);
        return {true, ""};
    }

    // otherwise if the file exists, try to update the last-accessed
    // and last-modified timestamps.

    // NULL = set both last-access and last-modification timestamps
    // to the current time.
    ret = utimensat(AT_FDCWD, fpath.c_str(), NULL, AT_SYMLINK_NOFOLLOW);
    if (ret < 0) {
        return {false, "utimensat(): "s + str::strerr()};
    }

    return {true, ""};
}
}  // namespace io
}  // namespace utils
}  // namespace tarp
