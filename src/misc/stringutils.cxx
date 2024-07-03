#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <set>

#include <tarp/stringutils.hxx>

using namespace std::string_literals;

namespace tarp {
namespace utils {
namespace string {

namespace {
// NOTE: this function is just an alias to provide a more intuitive,
// self-documenting name for this use-case.
static inline auto find_split_points(const std::string &s,
                                     const std::string &sep) {
    return find_needle_positions(s, sep);
}

// Find the slice of string INPUT that starts at the leftmost non-matching
// character and ends at the rightmost nonmatching character. A matching
// character is:
// - any whitespace character, if characters is not specified
// - otherwise any character given in characters.
//
// @return a {s,e} pair of indices.
// - Both s and e are >= 0.
// - S is the start index (inclusive) and e is the end index of the slice
//   (exclusive). Therefore, in interval notation, the result is to be
//   interpreted as [s,e).
std::pair<size_t, size_t>
find_nonmatching_slice(const std::string &input,
                       const std::optional<std::string> &characters = {}) {
    if (input.empty()) {
        return {0, 0};  // empty string
    }

    std::string chars2strip = characters.value_or(""s);
    std::set<char> chars {chars2strip.begin(), chars2strip.end()};
    bool strip_whitespace = !characters.has_value();

    // Nothing to do: no whitespace to strip NOR anything else.
    if (!strip_whitespace && chars.empty()) {
        return {0, input.size()};  // intact input string
    }

    std::size_t end_index;
    std::size_t start_index;

    std::size_t len = input.size();

    // NOTE: beware of overflow here; strings should be of sensible length.
    // Absurdly long one-line minified files may cause UB.
    ssize_t j;
    std::size_t i;

    for (i = 0; i < len; ++i) {
        if (strip_whitespace) {
            if (!std::isspace(input[i])) {
                break;
            }
        } else if (!chars.count(input[i])) {
            // not one of the chars in set
            break;
        }
    }

    // string has been stripped of every char!
    if (i >= len) {
        return {0, 0};
    }

    // index of first non-matching char in input string.
    start_index = i;

    for (j = (len - 1); j >= 0; --j) {
        if (strip_whitespace) {
            if (!std::isspace(input[j])) break;
        } else if (!chars.count(input[j])) {
            // not one of the chars in set
            break;
        }
    }

    // string has been stripped of every char!
    if (j < 0) {
        return {0, 0};
    }
    // index *after* the last non-matching char.
    end_index = j + 1;

    if (start_index > end_index) {
        throw std::logic_error("Bad string slice: start_index > end_index!");
    }

    return {start_index, end_index};
}

std::optional<std::string> replace_regex(const std::string &input,
                                         const std::string &pattern,
                                         const std::string &replacement,
                                         bool case_sensitive) {
    std::string result {input};

    auto flags = std::regex_constants::extended;
    if (!case_sensitive) {
        flags |= std::regex_constants::icase;
    }

    // stray escapes etc can cause an exception to be thrown
    try {
        std::regex_replace(result.begin(),
                           input.begin(),
                           input.end(),
                           std::regex(pattern, flags),
                           replacement);
    } catch (...) {
        return std::nullopt;
    }

    return result;
}



};  // anonymous namespace

std::vector<std::string> read_lines(const std::string &filepath,
                                    bool throw_on_failed_open) {
    std::ifstream f(filepath);
    if (!f and throw_on_failed_open) {
        auto errmsg = "Failed to open file for reading: '"s + filepath + "'";
        throw std::invalid_argument(errmsg);
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(f, line)) {
        lines.push_back(line + "\n");
    }

    return lines;
}

std::vector<std::size_t> find_needle_positions(const std::string &haystack,
                                               const std::string &needle) {
    std::vector<std::size_t> needle_positions;
    if (needle.empty()) {
        return needle_positions;
    }

    std::size_t idx = 0;
    std::size_t len = haystack.size();

    while (idx < len) {
        auto pos = haystack.find(needle, idx);
        if (pos == std::string::npos) {
            break;
        }
        needle_positions.push_back(pos);
        idx = pos + 1;
    }

    return needle_positions;
}

std::vector<std::string> split(const std::string &s, const std::string &sep) {
    std::vector<std::string> tokens;

    auto separator_positions = find_split_points(s, sep);
    std::size_t idx = 0;
    std::size_t string_len = s.size();

    for (auto split_pos : separator_positions) {
        if (idx >= string_len) break;

        std::size_t substring_len = split_pos - idx;
        if (substring_len > 0) {
            tokens.push_back(s.substr(idx, substring_len));
        }

        idx = split_pos + 1;
    }

    // substring following the last split point
    if (idx < string_len) {
        std::size_t substring_len = string_len - idx;
        tokens.push_back(s.substr(idx, substring_len));
    }

    return tokens;
}

bool match(const std::string input, const std::string re, bool case_sensitive) {
    // NOTE: Extended POSIX regex flavor is used.
    auto flags =
      std::regex_constants::extended | std::regex_constants::optimize;
    if (!case_sensitive) {
        flags |= std::regex_constants::icase;
    }

    std::regex pattern(re, flags);
    std::smatch results;
    return regex_match(input, results, pattern);
}

std::string rstrip(const std::string &input,
                   const std::optional<std::string> &characters) {
    auto [starti, endi] = find_nonmatching_slice(input, characters);
    return input.substr(0, endi);
}

std::string lstrip(const std::string &input,
                   const std::optional<std::string> &characters) {
    auto [starti, endi] = find_nonmatching_slice(input, characters);

    return input.substr(starti, input.size() - starti);
}

std::string strip(const std::string &input,
                  const std::optional<std::string> &characters) {
    return lstrip(rstrip(input, characters), characters);
}

std::string repeat(const std::string &s, unsigned n) {
    std::string result;
    for (unsigned i = 0; i < n; ++i) {
        result += s;
    }
    return result;
}

bool is_integer(const std::string &input) {
    std::string s = tarp::utils::string::strip(input);
    if (s.empty()) {
        return false;
    }

    for (const auto &c : s) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}

std::optional<double> to_float(const std::string &input) {
    std::string s = tarp::utils::string::strip(input);
    if (s.empty()) {
        return {};
    }

    errno = 0;

    // C++11 does not allow assigning string literals to char ** anymore;
    // this is just a way to work around that.
    char flag[1] = {'x'};
    char *marker = flag;
    double converted = std::strtod(s.c_str(), &marker);

    // if marker was set to NUL by strtod, it means the whole string
    // is valid (see man page). Therefore if this is not the case, either
    // the whole or part of the string is invalid.
    if (*marker != '\0') {
        return {};
    }

    // conversion overflows/underflows
    if (errno == ERANGE) {
        return {};
    }

    return converted;
}

// Same as to_integer, but convert to a double.
std::optional<double> to_float(const std::string &input);

std::optional<bool> to_boolean(const std::string &s) {
    std::string input = tarp::utils::string::strip(s);
    if (input.empty()) {
        return {};
    }

    std::transform(s.begin(), s.end(), input.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (input == "true" or input == "1") {
        return true;
    }
    if (input == "false" or input == "0") {
        return false;
    }

    return {};
}

bool is_boolean(const std::string &s) {
    return to_boolean(s).has_value();
}

std::string to_lower(const std::string &s) {
    std::string result {s};
    std::transform(s.begin(), s.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

std::string to_upper(const std::string &s) {
    std::string result {s};
    std::transform(s.begin(), s.end(), result.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return result;
}

std::optional<std::string> replace(const std::string &input,
                                   const std::string &pattern,
                                   const std::string &replacement,
                                   bool use_regex,
                                   bool case_sensitive) {
    if (use_regex) {
        return replace_regex(input, pattern, replacement, case_sensitive);
    }

    std::string result;
    auto indices = case_sensitive ? find_needle_positions(input, pattern)
                                  : find_needle_positions(to_lower(input),
                                                          to_lower(pattern));
    auto patlen = pattern.size();

    std::size_t prev_end_index = 0;

    for (std::size_t idx : indices) {
        std::cerr << "index is " << idx << std::endl;
        result += input.substr(prev_end_index, idx - prev_end_index);

        // skip the matched pattern and append replacement in its stead
        result += replacement;
        prev_end_index = idx + patlen;
    }

    result += input.substr(prev_end_index, input.size() - prev_end_index);

    return result;
}

std::string replace_char(const std::string &s, char a, char b) {
    std::string result {s};
    std::replace(result.begin(), result.end(), a, b);
    return result;
}

}  // namespace string
}  // namespace utils
}  // namespace tarp
