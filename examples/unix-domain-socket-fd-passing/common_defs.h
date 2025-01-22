#pragma once

struct named_pipe_msg {
  int pid;
  int fd;
};


static const char *sockpath = "/tmp/test.sock";
static const char *fifopath = "/tmp/test.fifo";
