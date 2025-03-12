#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if __cplusplus >= 202002L
#include <format>
#endif

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

#if __cplusplus >= 202002L

// C++23 introduces std::print, but this is missing in c++20.
// C++20 does have std::format however. The below provides a
// wrapper around std::format to subtitute for the lack of
// std::print.
template<typename... msg>
void print(std::format_string<msg...> fmt, msg &&...vargs) {
    std::cout << std::vformat(fmt.get(), std::make_format_args(vargs...))
              << std::endl;
}
#endif

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

// make fd point to /dev/null/ rather than its current file description.
std::pair<bool, std::string> attach_fd_to_dev_null(int fd);

// Close fd_to_redirect and reopen to point to the same file description as
// pointed to by the target_description fd.
std::pair<bool, std::string> duplicate_fd(int target_description,
                                          int fd_to_redirect);

// Check if fd is open.
bool is_valid_fd(int fd);

// Check if fd is open for readin or writing. Note a socket
// is readable AND writable so both of these will return true.
// Conversely, a pipe has readable and writable ends, so only
// one of these will return true depending on which end of the
// pipe they are called on.
std::pair<bool, std::string> fd_open_for_reading(int fd);
std::pair<bool, std::string> fd_open_for_writing(int fd);

// check if two files are identical (same size, same content).
// In case of error, the second result will be a non-empty string and the bool
// should be ignored. The bool (if there is no error) indicates whether the
// files are identical or not.
std::pair<bool, std::string> files_identical(const std::string &fpath_a,
                                             const std::string &fpath_b);

// Generate a file with num_bytes bytes, with each byte
// picked at random from the [0, 255] range.
bool generate_file_random_bytes(const std::string &abspath,
                                std::size_t num_bytes);

// Get a vector with NUM random bytes.
std::vector<std::uint8_t> get_random_bytes(unsigned num);

}  // namespace io
}  // namespace utils
}  // namespace tarp
