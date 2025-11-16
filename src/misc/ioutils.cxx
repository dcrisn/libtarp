#include <tarp/ioutils.hxx>
#include <tarp/string_utils.hxx>

#include <filesystem>
#include <fstream>
#include <random>

#include <cerrno>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace tarp {
namespace utils {
namespace io {

namespace fs = std::filesystem;
using namespace std::string_literals;

bool is_valid_fd(int fd) {
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

std::pair<bool, std::string> fd_open_for_reading(int fd) {
    errno = 0;

    int res = fcntl(fd, F_GETFL);
    if (res < 0) {
        return {false, strerror(errno)};
    }

    int file_access_mode = res & O_ACCMODE;

    // NOTE: the access mode flags are not actually flags so they cannot
    // be tested as 'file_access_mode & O_RDONLY'! for example,
    // O_RDONLY on this system is actually 0 so '&' would always evaluate
    // to 0 in that case.
    if (file_access_mode == O_RDONLY) {
        return {true, ""};
    }

    if (file_access_mode == O_RDWR) {
        return {true, ""};
    }

    return {false, ""};
}

std::pair<bool, std::string> fd_open_for_writing(int fd) {
    errno = 0;
    int res = fcntl(fd, F_GETFL);
    if (res < 0) {
        return {false, strerror(errno)};
    }

    auto file_access_mode = res & O_ACCMODE;

    if (file_access_mode == O_WRONLY) {
        return {true, ""};
    }

    if (file_access_mode == O_RDWR) {
        return {true, ""};
    }

    return {false, ""};
}

std::pair<bool, std::string> duplicate_fd(int target_description,
                                          int fd_to_redirect) {
    if (dup2(target_description, fd_to_redirect) < 0) {
        return {false, strerror(errno)};
    }
    return {true, ""};
}

// point fd to /dev/null
std::pair<bool, std::string> attach_fd_to_dev_null(int fd) {
    int nulldev_fd = open("/dev/null", O_RDWR);
    if (nulldev_fd == -1) {
        return {false, "Failed to open /dev/null: "s + strerror(errno)};
    }

    auto [ok, e] = duplicate_fd(nulldev_fd, fd);
    ::close(nulldev_fd);
    if (!ok) {
        return {false, "Failed /dev/null binding: "s + e};
    }

    return {true, ""};
}

std::pair<bool, std::string> files_identical(const std::string &fpath_a,
                                             const std::string &fpath_b) {
    std::ifstream a, b;
    a.open(fpath_a, std::ios_base::in | std::ios_base::binary);
    if (!a.is_open()) {
        return {false, "failed to open " + fpath_a};
    }

    b.open(fpath_a, std::ios_base::in | std::ios_base::binary);
    if (!b.is_open()) {
        return {false, "failed to open " + fpath_b};
    }

    if (fs::file_size(fpath_a) != fs::file_size(fpath_b)) {
        return {false, ""};
    }

    std::vector<char> buffer_a, buffer_b;
    constexpr std::size_t BUFFSZ = 1024 * 1024;
    buffer_a.resize(BUFFSZ);
    buffer_b.resize(BUFFSZ);

    std::size_t total_bytes_read_a = 0;
    std::size_t total_bytes_read_b = 0;

    while (!a.fail() and !b.fail()) {
        a.read(&buffer_a[0], buffer_a.size());
        auto bytes_read_a = a.gcount();
        total_bytes_read_a += bytes_read_a;

        b.read(&buffer_b[0], buffer_b.size());
        auto bytes_read_b = b.gcount();
        total_bytes_read_b += bytes_read_b;

        if (bytes_read_a != bytes_read_b or
            total_bytes_read_a != total_bytes_read_b) {
            throw std::logic_error("Unexpected condition");
        }

        for (unsigned i = 0; i < bytes_read_a; ++i) {
            if (buffer_a[i] != buffer_b[i]) {
                return {false, ""};
            }
        }
    }
    return {true, ""};
}

bool generate_file_random_bytes(const std::string &abspath,
                                std::size_t num_bytes) {
    std::ofstream of;
    of.open(abspath, std::ios_base::out | std::ios_base::binary);
    if (!of.is_open()) {
        return false;
    }

    auto bytes = get_random_bytes(num_bytes);
    for (auto byte : bytes) {
        of << byte;
    }

    of.flush();
    of.close();
    return true;
}

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

std::vector<std::uint8_t> get_random_bytes(unsigned num) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, 255);
    std::vector<std::uint8_t> bytes;

    for (unsigned i = 0; i < num; ++i) {
        bytes.push_back(dist(rng));
    }
    return bytes;
}

std::vector<std::uint8_t> get_cycling_bytes(unsigned num) {
    std::vector<std::uint8_t> bytes;
    for (unsigned i = 0; i < num; ++i) {
        bytes.push_back(i);
    }
    return bytes;
}

std::vector<std::uint8_t> repeat(std::uint8_t byte, unsigned n) {
    std::vector<std::uint8_t> bytes;

    for (unsigned i = 0; i < n; ++i) {
        bytes.push_back(byte);
    }
    return bytes;
}

// Pad buffer with trailing zeros to align to word_size bytes
void pad_to_alignment(std::vector<std::uint8_t> &buff, std::size_t word_size) {
    const std::size_t remainder = buff.size() % word_size;
    if (remainder != 0) {
        const std::size_t padding = word_size - remainder;
        buff.reserve(buff.size() + padding);
        buff.insert(buff.end(), padding, 0);
    }
}

// Pad buffer with trailing zeros to align to word_size bytes;
// Returns the padded buffer (original is not modified)
std::vector<std::uint8_t>
get_aligned_buffer(const std::vector<std::uint8_t> &buff,
                   std::size_t word_size) {
    auto padded = buff;
    pad_to_alignment(padded, word_size);
    return padded;
}

std::pair<std::string, std::string>
make_unique_file(const std::string &dirpath,
                 const std::string &file_name_prefix) {
    auto constexpr template_suffix = "XXXXXX";
    std::string path = dirpath + "/" + file_name_prefix + "." + template_suffix;
    std::vector<char> buff(path.begin(), path.end());
    buff.push_back('\0');
    int ret = mkstemp(buff.data());  // creates and opens a unique temp file
    if (ret < 0) {
        return {"", strerror(errno)};
    }

    close(ret);
    path.assign(buff.data());
    return {path, ""};
}

}  // namespace io
}  // namespace utils
}  // namespace tarp
