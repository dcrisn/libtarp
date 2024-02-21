#ifndef TARP_INIPARSE_HELPERS_H
#define TARP_INIPARSE_HELPERS_H

#include <stdio.h>

#include <tarp/iniparse.h>

/* enabled/disable debug prints */
#ifdef DEBUG_MODE
#   define say(...) fprintf(stderr, __VA_ARGS__);
#else
#   define say(...)
#endif

/*
 * see source files for coments/docs
 * */

const char *iniParse_err2str(enum iniParseError errnum);
enum iniParseError iniParse_get_list_error(
        struct iniparse_ctx *ctx,
        enum iniParseListState prev,
        enum iniParseListState next);

void iniParse_exit_print(enum iniParseError error, uint32_t ln);

uint32_t read_line(FILE *f, char **buff, size_t *buffsz);
bool is_empty_line(char *line);
bool is_comment_line(char *line);
bool is_section_line(char *line, char name[], size_t buffsz);
bool is_record_line(char *line, char k[], char v[], size_t buffsz);
bool is_list_head(char *line, char k[], size_t buffsz);
bool is_list_start(struct iniparse_ctx *ctx, char *line);
bool is_list_end(struct iniparse_ctx *ctx, char *line);
bool is_list_entry(char *line, char v[], size_t buffsz, bool *islast);
char *get_list_token(struct iniparse_ctx *ctx, char *line, char buff[], size_t buffsz);

char *strip_lws(char *s);
void strip_tws(char *s);
void strip_comment(char *s);


#endif
