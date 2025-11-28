#include <cstring>
#include <sys/types.h>
#include <tarp/random.hxx>

#include <cerrno>
#include <vector>

extern "C" {
#include <sys/random.h>
}

namespace tarp {
namespace random {

std::vector<std::uint8_t> get_secure_random_bytes(std::size_t n,
                                                  std::string *e) {
    std::vector<std::uint8_t> buff;
    buff.resize(n);

    //const std::uint32_t flags = GRND_NONBLOCK;
    // Do wait for initialization to reliably get the bytes.
    const std::uint32_t flags = 0;

    ssize_t ret = getrandom(buff.data(), buff.size(), flags);

    if (ret < 0) {
        if (e) *e = strerror(errno);
        return {};
    }

    // can occur when interrupted by a signal etc.
    // See man 2 getrandom.
    else if (ret < static_cast<ssize_t>(n)) {
        if (e) *e = "partial bytes";
        return {};
    }

    return buff;
}

}  // namespace random
}  // namespace tarp
