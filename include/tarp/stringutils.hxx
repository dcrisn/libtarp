#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace tarp::utils::string {

// Read whole file into memory, split it into lines, and return a vector
// thereof.
std::vector<std::string> read_lines(const std::string &filepath,
                                    bool throw_on_failed_open = false);

// Find the (0-based) indices in haystack where needle occurs.
std::vector<std::size_t> find_needle_positions(const std::string &haystack,
                                               const std::string &needle);

// Split the string s on the separator sep.
std::vector<std::string> split(const std::string &s, const std::string &sep);

// Join up all the tokens into one single string, with each token separated by
// sep.
template<template<typename> class container = std::vector>
std::string join(const container<std::string> &tokens,
                 const std::string sep = " ");

// True if input matches against the regular expression pattern re, else False.
// The regular expression flavor/engine used is Extended POSIX. NOTE: the whole
// string must match, end to end.
bool match(const std::string input, const std::string re, bool case_sensitive);

// strip off all chars in characters if specified, otherwise all whitespace,
// from the right end of the input string.
std::string rstrip(const std::string &input,
                   const std::optional<std::string> &characters = {});

// strip off all chars in characters if specified, otherwise all whitespace,
// from the left end of the input string.
std::string lstrip(const std::string &input,
                   const std::optional<std::string> &characters = {});

// strip off all chars in characters if specified, otherwise all whitespace,
// from both the left and right ends of the input string.
std::string strip(const std::string &input,
                  const std::optional<std::string> &characters = {});

// return the concatenation of n repetitions of input s;
// i.e.  s * n, as typical of higher-level languages.
// NOTE:
//   if n=0, an empty string is returned;
//   if n=1, the string is returned unchanged.
std::string repeat(const std::string &s, unsigned n);

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

template<template<typename> class container>
std::string join(const container<std::string> &tokens, const std::string sep) {
    std::string result;
    auto len = tokens.size();

    for (size_t i = 0; i < len; ++i) {
        result.append(tokens[i]);

        // last (and first, of course) token is not followed by a sep
        if (i < (len - 1)) {
            result.append(sep);
        }
    }

    return result;
}

}  // namespace tarp::utils::string
