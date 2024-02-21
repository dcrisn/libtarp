#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <tarp/common.h>
#include <tarp/iniparse.h>

int mycb(
        uint32_t ln,
        enum iniParseListState list,
        const char *section,
        const char *k, const char *v)
{
    fprintf(stdout, "called [%u]: [%s], %s=%s, list=%d\n", ln, section, k, v, list);
    return 0;
}

int main(int argc, char **argv){
    if (argc != 2){
        fprintf(stderr, " FATAL : sole argument must be path to a config file to parse\n");
        exit(EXIT_FAILURE);
    }

    struct iniparse_ctx *ctx;
    ctx = iniParse_init(true, false, ".", true);
    char *path = argv[1];
    return iniParse_parse(ctx, path, mycb);
}
