#ifndef __SOCKS__H
#define __SOCKS__H

/*
 * Utility functions to easily to set up socket of any kind.
 * The function must be directed as to the type of socket to set up by
 * passing a mask of the relevant flags.
 */

#include <stdint.h>
#include <sys/socket.h>  /* struct sockaddr, socket() etc */


/* 
 * FLAGS (0, 0xffff] used to form mask for socket setup customization.
 */
#define SOCKS_CLIENT              (1U << 0)    /* client socket */
#define SOCKS_SERVER              (1U << 1)    /* server socket */
#define SOCKS_CONNECT             (1U << 2)    /* do call connect() */ 
#define SOCKS_BIND                (1U << 3)    /* do call bind() */
#define SOCKS_LISTEN              (1U << 4)    /* do call listen() */
#define SOCKS_DGRAM               (1U << 5)    /* datagram socket */
#define SOCKS_STREAM              (1U << 6)    /* stream socket */
#define SOCKS_UNIX                (1U << 7)    /* unix domain socket */
#define SOCKS_UNIX_UNNAMED        (1U << 8)    /* unnamed unix sockets (do not bind()) */
#define SOCKS_INET                (1U << 9)    /* IPv4 socket */
#define SOCKS_INET6               (1U << 10)   /* IPv6 socket */
#define SOCKS_INET_NUMERIC_HOST   (1U << 11)   /* Host string argument represents a numeric internet address */
#define SOCKS_INET_NUMERIC_PORT   (1U << 12)   /* Port string argument represents a numeric port value */

/* IO failure return codes */
enum {READ_FAIL=-1, WRITE_FAIL=-2};

/*
 * Read buffsz bytes at a time from src until EOF or error and write to dst
 *
 * An internal buffer will be created of size buffsz which read() will read
 * into and write() will write from.
 *
 * If read() returns:
 * a) -1: transfer() considers it to have failed
 * b) 0 : transfer() considers it to have succeeded but
 *    the internal loop never enters; this is when read
 *    returns EOF. In other words, there is nothing to transfer.
 * c) a certain numbers of bytes that have been read;
 *    In this case the internal loop is entered and write()
 *    will attempt to write that number of bytes to dst.
 *
 * The number of bytes read by `read()` will be written
 * by `write()` in an internal loop, as described above.
 *
 * The full_write() wrapper (see socks.c) is used rather than 
 * the plain write() in order to attempt recovery from partial writes.
 *
 * <-- return
 *     0 on success; WRITE_FAIL on write failure; READ_FAIL on read failure.
 */
int transfer(int dst, int src, int16_t buffsz);

/* 
 * Set up a socket as described by MASK.
 *
 * --> mask
 *     A mask formed by OR-ing one or more flags shown above. The mask
 *     determines the kind of socket that will be set up and how.
 *
 * --> path
 *     a) A file system path (for unix domain sockets);
 *     b) Either a presentation formated ipv4 (dotted decimal notation) or 
 *        ipv6 string (hex string), or otherwise the name of a host to resolve
 *
 * --> service
 *     a) NULL for unix domain sockets
 *     b) for the inet and inet6 domains, a port number: either a numeric string 
 *     or otherwise the name of a service to resolve.
 *
 * <-- return
 *     -1 on error, else a integer representing a valid socket descriptor.
 */
int socks_setup(uint32_t mask,  const char *path, const char *service);

int socks_sharedes(int sender, int descript, struct sockaddr *receiver_addr, socklen_t receiver_len);
int socks_getdes(int receiver);

int pollfd(int fd, int timeout);

#endif
