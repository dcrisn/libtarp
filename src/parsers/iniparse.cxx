#include <string>

#include <tarp/common.h>

#define CXX_INIPARSE_IMPL__  /* to get the iniParse C API */
#include <tarp/iniparse.h>

using namespace std;
using namespace tarp;

// todo:
// iniparse.c has various static variables eg ALLOW_GLOBAL_RECORDS;
// create an opaque 'context' struct instead to keep these in; and in the
// case of the cxx class, keep them in the instance.
// also do not automatically exit if the config file is incorrect;
// also add some utilities for converting from string to whatever type;
// use these for converting to float:
// https://en.cppreference.com/w/cpp/string/basic_string/stof
void iniParser::parse_string(const std::string &s){
    UNUSED(s);
}

#if 0
class iniParser {
public:
    iniParser(void) = default;
    iniParser(const iniParser&) = delete;
    iniParser &operator=(const iniParser&) = delete;
    iniParser(iniParser&&) = delete;
    iniParser &operator=(iniParser&&) = delete;

    void parse_string(const std::string &s);
    void parse_file(const std::string &s);
    bool has(const std::string &key);
    bool islist(const std::string &key);
    auto get(const std::string &key);

private:
    std::unordered_map<std::string, std::any> m_map;
};
#endif
