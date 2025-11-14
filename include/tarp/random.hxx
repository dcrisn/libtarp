#pragma once

#include <algorithm>
#include <random>
#include <stdexcept>

namespace tarp {
namespace random {

namespace detail {
// Thread-local random engine
inline std::mt19937_64 &get_engine() {
    static thread_local std::mt19937_64 engine {std::random_device {}()};
    return engine;
}
}  // namespace detail

// set the seed used by the thread-local PRNG
inline void set_seed(std::uint64_t seed) {
    detail::get_engine().seed(seed);
}

// Get a random value that can be used for seeding.
inline std::uint64_t get_seed() {
    // Note: mt19937 doesn't expose its seed, so we return a random value
    // that can be used for seeding. For true reproducibility, use set_seed.
    return std::uniform_int_distribution<std::uint64_t> {}(
      detail::get_engine());
}

// Return true or false, with the likelihood of returning
// true being given by the likelihood_of_true fraction.
inline bool toss(float likelihood_of_true = 0.5) {
    const auto P = likelihood_of_true;
    if (P < 0.0 || P > 1.0) {
        throw std::invalid_argument("Probability must be in [0, 1]");
    }

    return std::bernoulli_distribution(P)(detail::get_engine());
}

// Pick from range [min, max].
template<typename T>
T pick(T min, T max) {
    auto &rng = detail::get_engine();
    if constexpr (std::is_integral_v<std::decay_t<T>>) {
        return std::uniform_int_distribution<T> {min, max}(rng);
    } else {
        return std::uniform_real_distribution<T> {min, max}(rng);
    }
}

// Pick a value from a list of values and weights:
// [(v1, w1), (v2, w2), ... (vn, wn)]
template<typename T, typename W = double>
T weighted_pick(std::initializer_list<std::pair<T, W>> values) {
    std::vector<W> cumulative_weights;
    cumulative_weights.reserve(values.size());

    W sum = 0;
    for (const auto &[v, w] : values) {
        if (w < 0) {
            throw std::invalid_argument("Weights must be non-negative");
        }
        sum += w;
        cumulative_weights.push_back(sum);
    }

    if (sum <= 0) {
        throw std::invalid_argument("Total weight must be positive");
    }

    W random_val =
      std::uniform_real_distribution<W> {0, sum}(detail::get_engine());
    auto it = std::lower_bound(
      cumulative_weights.begin(), cumulative_weights.end(), random_val);
    std::size_t index = std::distance(cumulative_weights.begin(), it);

    auto val_it = values.begin();
    std::advance(val_it, index);
    return *val_it;
}

// Pick a value from a vector with an associated
// list of weights:
// v[i1, i2, i3, ..., in]
// w[w1, w2, w3, ..., wn ]
template<typename T, typename W = double>
const T &weighted_pick(const std::vector<T> &values,
                       const std::vector<W> &weights) {
    if (values.size() != weights.size()) {
        throw std::invalid_argument("Values and weights must have same size");
    }
    if (values.empty()) {
        throw std::invalid_argument("Cannot pick from empty vector");
    }

    std::vector<W> cumulative_weights;
    cumulative_weights.reserve(weights.size());
    W sum = 0;
    for (const auto &w : weights) {
        if (w < 0) {
            throw std::invalid_argument("Weights must be non-negative");
        }
        sum += w;
        cumulative_weights.push_back(sum);
    }

    if (sum <= 0) {
        throw std::invalid_argument("Total weight must be positive");
    }

    W random_val =
      std::uniform_real_distribution<W> {0, sum}(detail::get_engine());
    auto it = std::lower_bound(
      cumulative_weights.begin(), cumulative_weights.end(), random_val);
    std::size_t index = std::distance(cumulative_weights.begin(), it);

    return values[index];
}

// Pick from range [min, max] with step.
// The value is picked from [min, ... min+step*n ..., max],
// for n >= 0.
template<typename T>
T pick(T min, T max, T step) {
    static_assert(std::is_integral_v<std::decay_t<T>>,
                  "Step-based pick only works with integers");
    if (step <= 0) throw std::invalid_argument("Step must be positive");
    T nsteps = (max - min) / step;
    double total_weight = 1 /*min*/ + 1 /*max*/ + nsteps;
    T x = pick<T>(0, nsteps);

    return weighted_pick({
      {min, 1 / total_weight     },
      {x,   nsteps / total_weight},
      {max, 1 / total_weight     }
    });
}

// Pick from initializer list
template<typename T>
T pick(std::initializer_list<T> values) {
    if (values.size() == 0) {
        throw std::invalid_argument("Cannot pick from empty list");
    }
    auto it = values.begin();
    std::advance(it, pick<std::size_t>(0, values.size() - 1));
    return *it;
}

// Pick from vector
template<typename T>
const T &pick(const std::vector<T> &values) {
    if (values.empty()) {
        throw std::invalid_argument("Cannot pick from empty vector");
    }
    return values[pick<std::size_t>(0, values.size() - 1)];
}

// Pick from vector [mutable]
template<typename T>
T &pick(std::vector<T> &values) {
    if (values.empty()) {
        throw std::invalid_argument("Cannot pick from empty vector");
    }
    return values[pick<std::size_t>(0, values.size() - 1)];
}

// Pick from container [mutable]
template<typename Container>
auto &choice(Container &container) {
    if (container.empty()) {
        throw std::invalid_argument("Cannot choose from empty container");
    }
    auto it = container.begin();
    std::advance(it, pick<std::size_t>(0, container.size() - 1));
    return *it;
}

// Pick from container (cons)
template<typename Container>
const auto &choice(const Container &container) {
    if (container.empty()) {
        throw std::invalid_argument("Cannot choose from empty container");
    }
    auto it = container.begin();
    std::advance(it, pick<std::size_t>(0, container.size() - 1));
    return *it;
}

// Pick n random bytes.
inline std::vector<std::uint8_t> bytes(std::size_t n) {
    std::vector<std::uint8_t> result(n);
    for (auto &byte : result) {
        byte = pick<std::uint8_t>(0, 255);
    }
    return result;
}

// Fill existing buffer with n random bytes
inline void fill_bytes(std::uint8_t *buffer, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        buffer[i] = pick<std::uint8_t>(0, 255);
    }
}

template<typename Container>
void shuffle(Container &container) {
    std::shuffle(container.begin(), container.end(), detail::get_engine());
}

// Sample n random elements from values.
template<typename T>
std::vector<T> sample(const std::vector<T> &values, std::size_t n) {
    if (n > values.size()) {
        throw std::invalid_argument("Sample size cannot exceed vector size");
    }

    std::vector<T> result = values;
    shuffle(result);
    result.resize(n);
    return result;
}

// Sample n random indices [0 ... population_size).
inline std::vector<std::size_t> sample_indices(std::size_t population_size,
                                               std::size_t count) {
    if (count > population_size) {
        throw std::invalid_argument(
          "Sample size cannot exceed population size");
    }

    std::vector<std::size_t> indices(population_size);
    std::iota(indices.begin(), indices.end(), 0);
    shuffle(indices);
    indices.resize(count);
    return indices;
}

// Normal/Gaussian distribution
template<typename T = double>
T normal(T mean = 0.0, T stddev = 1.0) {
    return std::normal_distribution<T> {mean, stddev}(detail::get_engine());
}

// Exponential distribution
template<typename T = double>
T exponential(T lambda = 1.0) {
    return std::exponential_distribution<T> {lambda}(detail::get_engine());
}

// Poisson distribution
inline int poisson(double mean) {
    return std::poisson_distribution<int> {mean}(detail::get_engine());
}

// Create a random string using characters from the given alphabet
inline std::string string(std::size_t length, const std::string &alphabet) {
    if (alphabet.empty()) {
        throw std::invalid_argument("Alphabet cannot be empty");
    }

    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        result += alphabet[pick<std::size_t>(0, alphabet.size() - 1)];
    }
    return result;
}

// Generate a random hex string of the given length
inline std::string hex_string(std::size_t length, bool include_prefix = false) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        result += hex_chars[pick<std::size_t>(0, 15)];
    }
    if (include_prefix) {
        result = "0x" + result;
    }
    return result;
}

// Generate a random alphanumeric string of the given length
inline std::string alphanumeric_string(std::size_t length) {
    static const std::string alphabet = "0123456789"
                                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz";
    return string(length, alphabet);
}

// Generate a random string with no numerals, only letters
inline std::string alpha_string(std::size_t length, bool lowercase = true) {
    static const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    return string(length, lowercase ? lower : upper);
}

// Generate a random numeric string
inline std::string numeric_string(std::size_t length) {
    static const std::string digits = "0123456789";
    return string(length, digits);
}

// --------------------------
// convenience aliases
// --------------------------
//
inline int randint(int min, int max) {
    return pick(min, max);
}

inline std::size_t randsz(std::size_t min, std::size_t max) {
    return pick(min, max);
}

inline double randdbl(double min = 0.0, double max = 1.0) {
    return pick(min, max);
}

inline float randfloat(float min = 0.0f, float max = 1.0f) {
    return pick(min, max);
}

// --------------------------


}  // namespace random
};  // namespace tarp
