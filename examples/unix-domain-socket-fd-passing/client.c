#include "common_defs.h"

#include <tarp/ioutils.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void send_fd_to_process(int fd) {
    // in this mode, we assume the server has opened a named pipe at
    // /tmp/test.sock. we send the child pid and the fd to the server over this
    // pipe.

    fprintf(stderr, "~ opening fifo\n");
    int fifofd = open(fifopath, O_WRONLY);
    if (fifofd == -1) {
        perror("open()");
        exit(EXIT_FAILURE);
    }

    struct named_pipe_msg msg;
    msg.fd = fd;
    msg.pid = getpid();

    fprintf(stderr, "~ writing to fifo\n");
    int num_written = write(fifofd, &msg, sizeof(msg));
    if (num_written == -1) {
        perror("write()");
        exit(EXIT_FAILURE);
    }

    if (num_written != sizeof(msg)) {
        fprintf(stderr,
                "partial write to named pipe: %d/%zu\n",
                num_written,
                sizeof(msg));
    }
}

int main(int argc, const char **argv) {
    if (argc == 1) {
        fprintf(stderr, "missing argument: stream|datagram|pidfd\n");
        exit(EXIT_FAILURE);
    }

    bool use_sock_stream = false;
    bool use_pidfd = false;
    if (strcmp(argv[1], "stream") == 0) {
        use_sock_stream = true;
    } else if (strcmp(argv[1], "datagram") == 0) {
        use_sock_stream = false;
    } else if (strcmp(argv[1], "pidfd") == 0) {
        use_pidfd = true;
    } else {
        fprintf(stderr,
                "unknown arg: '%s'. Expected: stream|datagram|pidfd\n",
                argv[1]);
        exit(EXIT_FAILURE);
    }

    int pipefd[2] = {-1, -1};
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    char string[1024] = "mystring";
    fprintf(stderr, "writing to pipe\n");
    int written = write(pipefd[1], string, sizeof(string));
    if (written != sizeof(string)) {
        perror("write()");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "after write to pipe\n");

    // share file descriptor using the new pidfd facility
    if (use_pidfd) {
        fprintf(stderr, "using pifd\n");
        send_fd_to_process(pipefd[0]);
        return 0;
    }

    // else pass fd over unix domain socket.
    int sockfd = -1;
    struct result res = make_client_uds(
      sockpath, "/tmp/test.client.sock", &sockfd, use_sock_stream);
    if (!res.ok) {
        perr(res);
        exit(EXIT_FAILURE);
    }

    res = send_msg_with_fd(
      sockfd, pipefd[0], (uint8_t *)string, strlen(string) + 1, true, NULL);
    if (!res.ok) {
        perr(res);
        EXIT_FAILURE;
    }

    fprintf(stdout,
            "Sent file descriptor %d, pre-filled with bytes '%s'\n",
            pipefd[0],
            string);
    return 0;
}
