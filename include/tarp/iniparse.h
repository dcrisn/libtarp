#pragma once

#include <stdint.h>
#include <stdbool.h>

/* maximum allowable line length in config file */
#define MAX_LINE_LEN 1024U

#if(!defined(__cplusplus) || defined(CXX_INIPARSE_IMPL__))
#ifdef __cplusplus
extern "C"{
#endif

struct iniparse_ctx;

/*
 * Used to communicate the state of a list. The parser is line-oriented
 * and it does NOT parse the whole file in one go or ahead of time.
 * That is to say, the parser can only know if there has been an error
 * on a preceding line or whether the current line is erroneous but does
 * *not* know whether an error is caused by any subsequent line.
 *
 * To deal with lists, state such as the below must be maintained: the head
 * of a list sets LIST_HEAD, the opening bracket sets LIST_OPEN, all susequent
 * items BUT THE LAST set LIST_ONGOING, and the last item sets LIST_LAST.
 * Finally, the closing brace resets back to NOLIST.
 */
enum iniParseListState{
    LIST_HEAD    = 5,   /* current line/entry is the head (key) of a list */
    LIST_OPEN    = 4,   /* current line/entry is an opening bracket for a list */
    LIST_ONGOING = 2,   /* current line/entry is a regular entry in a list */
    LIST_LAST    = 1,   /* current line/entry is the last item in a list */
    NOLIST       = 0    /* current line/entry is a closing bracket for a list */
};

/* iniparse error numbers;
 * also see iniparse_error_strings in iniparse.c */
enum iniParseError{
    INIPARSE_SUCCESS = 0,  /* no error */
    INIPARSE_NOSECTION,
    INIPARSE_MALFORMED,
    INIPARSE_MALFORMED_LIST,
    INIPARSE_TOOLONG,
    INIPARSE_NESTED,
    INIPARSE_NOLIST,
    INIPARSE_EMPTY_LIST,
    INIPARSE_MISSING_COMMA,
    INIPARSE_REDUNDANT_COMMA,
    INIPARSE_REDUNDANT_BRACKET,
    INIPARSE_LIST_NOT_STARTED,
    INIPARSE_LIST_NOT_ENDED,
    INIPARSE_SENTINEL        /* max index in iniparse_error_strings */
};

/*
 * Callback to be called by ini_parse (and ini_parse_string)
 * on every .ini config line being parsed. The callback will
 * get called with the arguments shown below.
 *
 * Note error conditions cause the program to exit with an error/diagnostic
 * message. The callback does NOT get to recover from errors or ignore them.
 *
 * --> ln
 * line number in the config file, starting from 1.
 *
 * --> list
 * used for dealing with lists; see iniParseListState.
 *
 * --> section
 * ini config section
 *
 * --> k
 * section key (if !list) or list name (if list > 0)
 *
 * --> v
 * section value (if !list) or list entry (if list > 0)
 */
typedef
int (* config_cb)(
        uint32_t ln,
        enum iniParseListState list,
        const char *section,
        const char *k,
        const char *v
        );

/*
 * Initialize the ini parser state.
 *
 * --> ctx
 * ctx is an opaque handle to the parser and must be non-NULL.
 * Multiple parsers can be used at the same time as they each
 * maintain their state entirely in the opaque handle;
 * Once initialized, the parser must always be called with
 * the corresponding handle.
 *
 * --> allow_globals
 * Consider key-value pairs that appear before any section title legal
 *
 * --> allow_empty_lists
 * Consider lists without any list items to be legal
 *
 * --> section_delim
 * A character that represents section nesting e.g. a.b.c;
 * see SECTION_NS_SEP in iniparse.c
 *
 * --> exit_on_error
 * If true, the parser will print an error message and exit on
 * any parsing errors encountered (invalid configuration file being parsed).
 * Otherwise if false, an error code will be returned to the called instead.
 */
struct iniparse_ctx *iniParse_init(
        bool allow_globals,
        bool allow_empty_lists,
        const char *section_delim,
        bool exit_on_error
        );

/*
 * After this call the user can reuse ctx or
 * call free() on it if appropriate */
void iniParse_destroy(struct iniparse_ctx *ctx);

/*
 * Parse the .ini config file specified by path.
 *
 * The parsing is carried out line by line and for each
 * relevant entry config_cb is called.
 *  - Empty lines and lines containing only a comment are skipped.
 *  - lines not understood result in a fatal error. The callback
 *    does not get to ignore or recover from such errors. Instead,
 *    the config line should be fixed to be syntactically
 *    correct/understood.
 *
 * --> path
 * path to .ini config file
 */
int iniParse_parse(
        struct iniparse_ctx *ctx,
        const char *path,
        config_cb cb
        );


#ifdef __cplusplus
}  /* extern "C" */
#endif
#endif /* C api include guard */

#ifdef __cplusplus
#include <string>
#include <unordered_map>
#include <any>

namespace tarp {

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

} /* namespace tarp */


#endif  /* __cplusplus */
