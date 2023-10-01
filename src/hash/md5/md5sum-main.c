#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <tarp/hash/md5sum.h>


static void usage(const char *const progname){
    printf(
        "\n%s [-h]|[-s STRING1 STRING2, ..., STRING_N]|[FILE1 FILE2, ..., FILE_N]\n"
        "    -h         print this help message and exit\n"
        "    -s STRING  produce an md5 digest on stdout for each STRING\n\n"
        "If -s is not specified, it's assumed the positional arguments\n"
        "are names of files instead. An MD5 digest will be produced for each.\n",
        progname
        );
}

int main(int argc, char **argv){
    uint8_t output[16];
    bool string_mode = false;
    int arg = -1;
    int rc = 0;

    /* no cli args */
    if (argc == 1){
        if (MD5_fdigest(STDIN_FILENO, output) == 0){
            MD5_print(output);
        }
        exit(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    /* else parse cli */
    while ( (arg = getopt(argc, argv, "hs")) != -1){
        switch(arg){
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        case 's':
            string_mode = true;
            break;
        }
    }

    for (int i = optind; i < argc; ++i){
        if (string_mode)   rc = MD5_sdigest(argv[i], output);
        else               rc = MD5_file_digest(argv[i], output);

        if (rc == 0) MD5_print(output);

        /* print a new line only between checksum prints */
        if (argc > i+1) printf("\n");
    }
}
