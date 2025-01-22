#include "common_defs.h"

#include <tarp/ioutils.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>

// receive file descriptor over unix domain socket.
// cannot fail. if we fail, we just exit.
void get_fd_over_uds(int *fd_received, bool use_sock_stream) {
    int server_fd = -1;
    const char *path = sockpath;
    struct result res = make_server_uds(path, 100, &server_fd, use_sock_stream);
    if (!res.ok) {
        perr(res);
        exit(EXIT_FAILURE);
    }

    int received_fd = -1;
    uint8_t buff[1024];
    size_t num_bytes_read = 0;

    // If we use a stream socket, then we must first accept the connection
    // and then use the connection socket for receiving data.
    if (use_sock_stream) {
        int connfd = accept(server_fd, NULL, NULL);
        if (connfd < 0) {
            perror("accept()");
            exit(EXIT_FAILURE);
        } else {
            // we won't need this again.
            close(server_fd);

            // use it below in the call to recvmsg
            server_fd = connfd;
        }
    }

    const unsigned MIN_MSZ_SZ_EXPECTED = 1;
    res = receive_msg_with_fd(server_fd,
                              &received_fd,
                              buff,
                              sizeof(buff),
                              MIN_MSZ_SZ_EXPECTED,
                              true,
                              &num_bytes_read);
    if (!res.ok) {
        perr(res);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr,
            "Message received with fd=%d and contents='%s'\n",
            received_fd,
            buff);

    *fd_received = received_fd;
}

// for this the program must be invoked with sudo (or must be given the
// appropriate capability), otherwise the syscall to pidfd_getfd will fail.
// If we run as sudo, the named pipe file will also end up being sudo-owned, so
// the client will have to be invoked as sudo as well.
void get_fd_over_pidfd(int *fd_received) {
    const char *path = fifopath;
    remove(path);

    if (mkfifo(path, S_IRWXU | S_IRWXG) < 0) {
        perror("mkfifo()");
        exit(EXIT_FAILURE);
    }

    int fifofd = open(path, O_RDONLY);
    if (fifofd < 0) {
        perror("fifo open()");
        exit(EXIT_FAILURE);
    }

    uint8_t buff[1024];
    int num_read = read(fifofd, buff, sizeof(buff));
    if (num_read == -1) {
        perror("read()");
        exit(EXIT_FAILURE);
    }

    if (num_read != sizeof(struct named_pipe_msg)) {
        fprintf(stderr, "sizeof(named_pipe_msg) != num_read\n");
        exit(EXIT_FAILURE);
    }

    struct named_pipe_msg *msg = (struct named_pipe_msg *)buff;
    int pid = msg->pid;
    int fd_to_get = msg->fd;

    int pidfd = syscall(SYS_pidfd_open, pid, 0);
    if (pidfd == -1) {
        perror("syscall(SYS_pidfd_open)");
        exit(EXIT_FAILURE);
    }

    int fd = syscall(SYS_pidfd_getfd, pidfd, fd_to_get, 0);
    if (fd == -1) {
        perror("syscall(SYS_pidfd_getfd)");
        exit(EXIT_FAILURE);
    }
    *fd_received = fd;
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
        fprintf(
          stderr, "unknown arg: '%s'. Expected: stream|datagram\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    int fd_received = -1;

    if (use_pidfd) {
        get_fd_over_pidfd(&fd_received);
    } else {
        get_fd_over_uds(&fd_received, use_sock_stream);
    }

    uint8_t buff[1024];
    ssize_t n = read(fd_received, buff, sizeof(buff));
    if (n < 0) {
        perror("read()");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Read from received_fd: '%s'\n", buff);

    return 0;
}
