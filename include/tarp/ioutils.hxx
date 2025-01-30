#pragma once

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace tarp {
namespace utils {
namespace io {

/*
 * In c++ 20 there is python like string - format capability.
 * The below is a substitute for older versions.
 *
 * Adapted from
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf.
 */
template<typename... Args>
std::string sfmt(const std::string &fmt, Args... vargs) {
    /* get total number of characters that snprintf would've written; +1
     * since the return value does not account for the terminating NULL */
    int len = std::snprintf(nullptr, 0, fmt.c_str(), vargs...) + 1;
    if (len <= 0) {
        throw std::runtime_error("sfmt string formatting error");
    }

    size_t buffsz = static_cast<size_t>(len);
    std::unique_ptr<char[]> buff(new char[buffsz]);

    std::snprintf(buff.get(), buffsz, fmt.c_str(), vargs...);

    /* -1 to leave out the terminating Null */
    return std::string(buff.get(), buff.get() + buffsz - 1);
}

template<typename... Args>
void println(Args &&...args) {
    (std::cout << ... << std::forward<Args>(args)) << '\n';
}

// Create the file on the specified path if it does not exist.
// The permissions will be as specified by fopen()
// (umask applied to 0666).
//
// Otherwise if it already exists, update its last-accessed
// and last-modified timestamps to the current time.
// If the file is a symlink, the symlink itself (rather than its
// target) will have its timestamps updated; IOW the symlink
// is not followed.
std::pair<bool, std::string> touch(const std::string &fpath);

}  // namespace io
}  // namespace utils
}  // namespace tarp
