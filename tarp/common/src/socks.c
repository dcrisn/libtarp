#include <assert.h>
#include <errno.h>
#include <unistd.h>      /* read(), write(), close() */
#include <limits.h>      /* SSIZE_MAX */
#include <stdio.h>       /* remove()  */
#include <string.h>      /* strncpy */
#include <sys/un.h>      /* struct sockaddr_un */
#include <netinet/in.h>  /* struct sockaddr_in, struct sockaddr_in6 */
#include <netdb.h>       /* getaddrinfo() */
#include <poll.h>        /* poll API */

#include <tarp/common.h>
#include <tarp/socks.h>

/* 
 * Let this be non-ancillary data (int32_t) accompanying ancillary data
 * sent with sendmsg(). Note on Linux at least 1 byte of non-ancillary data 
 * is required to be sent when going over SOCK_STREAM sockets. */
#define SHARED_DESC_MAGIC 0x99e1        /* 1001 1001  1110 0001 */

/*
 * Reliably write nbytes from the src buffer to the dst descriptor.
 *
 * This function deals with partial writes which may occur due to
 * e.g. signal interrupts. Unless WRITE_FAIL is returned, then the whole
 * nbytes have been written to dst.
 *
 * It also reattempts the write 5 times in a row when the write fails
 * with one of EINTR, EAGAIN or EWOULDBLOCK. If one of those reattempts
 * actually succeeds, the reattempt counter is reset to 0 (meaning if
 * another error like this were to occur, another 5 reattempts would be
 * carried out). These 3 errors may be considered temporary nonfatal
 * conditions so reattempting a write makes sense.
 *
 * Other errors however are likely fatal so reattempting the write in
 * that case is not advisable. The function in that case returns WRITE_FAIL,
 * as it's unlikely write() will succeed and there are probably more
 * significant issues with the system anyway.
 *
 * <-- return
 *     0 on success. Else WRITE_FAIL if write() has failed with an error
 *     considered to be FATAL or if the maximum number of write reattempts
 *     has been reached without success.
 */
static int full_write(int dst, char *src, int16_t nbytes){
    int tries         = 0;
    int max_tries     = 5;
    int to_write      = nbytes;
    int bytes_written = 0;

    while(to_write){ /* writing 0 bytes is implementation defined, so do not */
        bytes_written = write(dst, src, to_write);
        if (tries == max_tries) return WRITE_FAIL;
        if (bytes_written == -1){
            if (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK){
                ++tries; continue;
            } else return WRITE_FAIL;
        };
        src      += bytes_written;
        to_write -= bytes_written;
        tries = 0;  /* reset tries */
    } /* while write */

    return 0;
}

int transfer(int dst, int src, int16_t buffsz){
    char buff[buffsz];
    int16_t bytes_read = 0;

    while ( (bytes_read = read(src, buff, buffsz)) ){
        if (bytes_read == -1) return READ_FAIL;
        if (full_write(dst, buff, bytes_read) == WRITE_FAIL){
            return WRITE_FAIL;
        }
    } /* while read */

    return 0;
}

/*
 * Set appropriate socket flags on sd based on mask.
 *
 * --> mask
 *     A mask of various SOCKS_* flags. This function will potentially set
 *     various socket options with setsockopt() based on the mask specification.
 *
 * --> sd
 *     A socket file descriptor, already created by a call to socket().
 *
 * <-- return
 *     0 on success, -1 on failure.
 */
static int socks_set_opts(uint32_t mask, int sd){
    printf("mask is %u\n", mask);
    if (mask & SOCKS_CLIENT){
        return 0;
    }
	else if (mask & SOCKS_SERVER){
		int flag = 1;
		if (!setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))){
			return 0;
		}
	}
    
    puts("returning -1");
	return -1;
}

/*
 * Create, configure, and set up new socket based on mask specification.
 *
 * --> mask
 *     A mask of various SOCKS_* flags. This function will potentially set
 *     various socket options with setsockopt() based on the mask specification.
 *
 * --> addr
 *     A already initialized socket address structure of the appropriate type such
 *     that it's consistent with the flags specified in mask. The respective struct
 *     is then cast to the generic `struct sockaddr` when passed to this function.
 *
 * --> addrlen
 *     The size of the actual socket address structure before casting.
 *
 * <-- return
 *     -1 on failure, otherwise a socket descriptor configured based on MASK.
 *     The socket is first created by a call to `socket()`, then potentially
 *     various socket flags are set via a call to `socks_set_opts()`, and then,
 *     depending on the flags in mask, the socket is either `connect()`ed or
 *     bound with `bind()` and then potentially put in `listen()`ing mode.
 *
 * Currently only sockets in the following domains are supported: AF_UNIX, AF_INET,
 * AF_INET6.
 */
static int socks_make(uint32_t mask, struct sockaddr *addr, socklen_t addrlen){
    printf("mask is %u\n", mask);
	ASSUME(addr && addrlen>0, "!(addr && addrlen>0)");
	int32_t domain = 0;
	int32_t type   = 0;
	int sd         = 0;

	/* determine domain and type based on mask */
	domain = mask & SOCKS_UNIX  ? AF_UNIX  :
             mask & SOCKS_INET  ? AF_INET  :
             mask & SOCKS_INET6 ? AF_INET6 :
             -1;

	type   = mask & SOCKS_STREAM ? SOCK_STREAM :
             mask & SOCKS_DGRAM  ? SOCK_DGRAM  :
             -1;

	/* verify domain and type are sane */
	if (domain == -1){
		errno = EPFNOSUPPORT;
		return -1;
	}

	if (type == -1){
		errno = ESOCKTNOSUPPORT;
		return -1;
	}

    puts("here1");
	/* create socket and set flags based on mask */
	if (( sd = socket(domain, type, 0)) == -1){
        puts("here3");
		return -1;
	}
    
    puts("here2");
	if (socks_set_opts(mask, sd) == -1) goto fail;
	printf("sd == %i\n",sd);

	/* client vs server configuration */
	if (mask & SOCKS_CLIENT){
		if (!(mask & SOCKS_CONNECT) || connect(sd, addr, addrlen)==0) return sd;
	}
	else if (mask & SOCKS_SERVER){
		if (   (!(mask & SOCKS_BIND)   || bind(sd, addr, addrlen)==0)
			&& (!(mask & SOCKS_LISTEN) || ((mask & SOCKS_STREAM) && listen(sd, 1)==0))
			){ return sd; }
	}

fail:
    close(sd);
    return -1;
}


/*
 * Set up a connected or listening internet socket
 *
 * The function can transparently handle either INET (ipv4) or
 * INET6 (ipv6) socket setup.
 *
 * --> mask
 *     a mask of flags specifying the type of socket to create
 *     and how to configure it.
 *
 * --> path
 *     The argument supplied must either be a hostname or domain name to
 *     resolve, or a presentation-format address: dotted decimal for ipv4
 *     or hex string for ipv6. If the latter (address is numeric) then mask
 *     MUST contain the SOCKS_INET_NUMERIC_HOST flag.
 *
 *     The argument may also be NULL, in which case the loopback address will be
 *     used (for client sockets) or the wildcard address (for listening server
 *     sockets).
 *
 * --> service
 *     Either a service name to be resolved, or a numeric port number. If the
 *     latter (numeric value), then SOCKS_INET_NUMERIC_PORT should be specified
 *     in mask.
 *
 * <-- return
 *     -1 on error, otherwise a socket descriptor ready for `accept()`ing
 *     connections on (provided both SOCKS_BIND and SOCKS_LISTEN were specified
 *     in mask) or writing to and reading from.
 */
static int socks_inet_setup(uint32_t mask, const char *path, const char *service){
    int ret = 0;          /* store return value of intermediate function calls */
    int sd  = 0;          /* socket descriptor to be returned */
    int32_t domain        = 0;
    int32_t type          = 0;
    int32_t addrinfo_mask = 0;

    domain = (mask & SOCKS_INET)  ? AF_INET :
             (mask & SOCKS_INET6) ? AF_INET6:
             AF_UNSPEC;
    type   = (mask & SOCKS_STREAM) ? SOCK_STREAM :
             (mask & SOCKS_DGRAM)  ? SOCK_DGRAM  :
             -1;
    if (type == -1){
        errno = EINVAL;
        return -1;
    }

    addrinfo_mask |= (mask & SOCKS_INET_NUMERIC_HOST) ? AI_NUMERICHOST : 0;
    addrinfo_mask |= (mask & SOCKS_INET_NUMERIC_PORT) ? AI_NUMERICSERV : 0;
    addrinfo_mask |= (mask & SOCKS_SERVER) ? AI_PASSIVE : 0;

	struct addrinfo hints, *result, *list;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = domain;
    hints.ai_socktype = type;
    hints.ai_flags    = addrinfo_mask;

    if ((ret = getaddrinfo(path, service, &hints, &list))){
        COMPLAIN("getaddrinfo() failure (%d): '%s'", ret, gai_strerror(ret));
        return -1;
    }

    /* find valid address */
    for (result = list; result; result = result->ai_next){
        if ( (sd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1){
            continue;
        }

        if ( (sd = socks_make(mask, result->ai_addr, result->ai_addrlen)) == -1){
            continue;
        }

        break;
    }

    freeaddrinfo(list);
    if (!result){
        COMPLAIN("Failed to find valid address and set up socket with getaddrinfo()");
        return -1;
    }

    return sd;
}

/*
 * Set up a connected or listening unix domain socket
 *
 * --> mask
 *     a mask of flags specifying the type of socket to create
 *     and how to configure it.
 *
 * --> srvpath
 *     An absolute file system path to either connect to (if client) or bind to
 *     (if server).
 *
 * --> clpath
 *     ONLY used for NAMED client sockets. Unused otherwise and it should be
 *     NULL. Stream socket in the unix domain can be 'nameless' that is to say
 *     unbound and they would still be able to receive messages back from the
 *     server they have connected to. Datagram sockets on the other hand, MUST
 *     bind() to a path to be able to receive messages. In this case, the path
 *     to bind to should be specified in clpath. Stream unix sockets can also
 *     bind (in which case they would be named rather than nameless) -- but they
 *     do not have to.
 *
 *     This argument is ONLY used if SOCKS_UNIX_UNNAMED is NOT set in mask.
 *
 * <-- return
 *     -1 on error, otherwise a socket descriptor ready for `accept()`ing
 *     connections on (provided both SOCKS_BIND and SOCKS_LISTEN were specified
 *     in mask) or writing to and reading from.
 */
static int socks_unix_setup(uint32_t mask, const char *srvpath, const char *clpath){
    printf("mask is %u\n", mask);
    int sd = 0;
	struct sockaddr_un uds = { .sun_family = AF_UNIX };
	memset(&uds.sun_path, 0, sizeof(uds.sun_path));

	if (!srvpath || strlen(srvpath) >= sizeof(uds.sun_path)){
		errno = EINVAL;
		return -1;
	}
    if (mask & SOCKS_SERVER){
        if (remove(srvpath) == -1 && errno != ENOENT){
            return -1;
        }
    }
	strncpy(uds.sun_path, srvpath, sizeof(uds.sun_path)-1);

	sd = socks_make(mask & ~SOCKS_CONNECT, (struct sockaddr *)&uds, sizeof(struct sockaddr_un));
    if (sd == -1){
        return sd;
    }else if (mask & SOCKS_SERVER){\
        goto success;
    }
    
    /* client, manually: first optionally bind() and then connect() */
    if (mask & SOCKS_CLIENT && !(mask & SOCKS_UNIX_UNNAMED) ){
        if (!clpath){
            COMPLAIN("Asked for named unix client socket but no path was specified to bind to");
            errno = EFAULT; 
            goto fail;
        }

    	struct sockaddr_un claddr = { .sun_family = AF_UNIX };
        memset(&claddr.sun_path, 0, sizeof(claddr.sun_path));

        if (!clpath || strlen(clpath) >= sizeof(claddr.sun_path)){
            errno = EINVAL;
            goto fail;
        }
        if (remove(clpath) == -1 && errno != ENOENT){
            goto fail;
        }
        strncpy(claddr.sun_path, clpath, sizeof(claddr.sun_path)-1);
        
        puts("binding");
        if (bind(sd, (struct sockaddr *)&claddr, sizeof(claddr))){
            goto fail;
        }
    }

    if (mask & SOCKS_CONNECT && connect(sd, (struct sockaddr *)&uds, sizeof(uds)) == 0){
        goto success;
    }

success:
    errno = 0;
    return sd;

fail:
    close(sd);
    return -1;
}

/*
 * Set up a connected or listening socket in a supported domain.
 *
 * --> mask
 *     a mask of flags specifying the type of socket to create
 *     and how to configure it.
 *
 * --> path
 *     + for unix sockets: an absolute file system path to either connect to or bind to.
 *     + for INET and INET6, it can be:
 *      a) NULL, in which case it will use (for client sockets) INADDR_LOOPBACK (ipv4) or
 *      IN6ADDR_LOOPBACK_INIT or (for server sockets) INADDR_ANY (ipv4) or IN6ADDR_ANY_INIT
 *      (ipv6).
 *      b) a presentation-format address: dotted-decimal for ipv4 or hex-string
 *      for ipv6
 *      c) a hostname/domain name to resolve
 *
 * --> port
 *     + for internet sockets, either a service name to resolve or a numeric
 *     port value (in this case the SOCKS_INET_NUMERIC_PORT flag must be
 *     specified in mask).
 *     + for unix sockets, normally NULL EXCEPT: when setting up a CLIENT
 *     DATAGRAM socket AND the caller wants it to be able to receive messages
 *     back from the server (see socks_unix_setup() FMI). In this case, an
 *     absolute file system path to bind to should be specified, just like for
 *     the path argument.
 *
 * <-- return
 *     -1 on error, otherwise a socket descriptor ready for `accept()`ing
 *     connections on (provided both SOCKS_BIND and SOCKS_LISTEN were specified
 *     in mask) or writing to and reading from.
 *
 * Only sockets in the following domains are currently supported: AF_UNIX,
 * AF_INET, AF_INET6.
 */
int socks_setup(uint32_t mask, const char *path, const char *port){
	if (mask & SOCKS_UNIX){
		return socks_unix_setup(mask, path, port);
	}else if (mask & SOCKS_INET || mask & SOCKS_INET6){
		return socks_inet_setup(mask, path, port);
	}
	return -1;
}

int socks_sharedes(int sender, int descript, struct sockaddr *receiver_addr, socklen_t receiver_len){
    ssize_t ret = 0;

    /* msgbuff will internally be cast to a `struct cmshdr` so we must ensure
     * it's aligned */
    union {
        struct cmsghdr __align;
        char buff[CMSG_SPACE(sizeof(int))];
    } aligned_msg = {0};
    
    struct msghdr msg;     /* argument type expected by sendmsg() and recvmsg() */
    memset(&msg, 0, sizeof(msg));
    msg.msg_name    = (receiver_addr && receiver_len) ? receiver_addr : NULL;
    msg.msg_namelen = (receiver_addr && receiver_len) ? receiver_len  : 0;

    int32_t magic    = SHARED_DESC_MAGIC; 
    struct iovec iov = {0};
    iov.iov_base     = &magic;
    iov.iov_len      = sizeof(magic);

    msg.msg_iov    = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control =  aligned_msg.buff;
    msg.msg_controllen = sizeof(aligned_msg.buff);
    
    /* ancillary data is a sequence of cmsghdr structures; 
     * only need 1 here as we send just 1 descriptor */
    struct cmsghdr *control = CMSG_FIRSTHDR(&msg);  /* fill in */
    control->cmsg_len = CMSG_LEN(sizeof(descript));   /* room for a descriptor */
    control->cmsg_level = SOL_SOCKET;
    control->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(control), &descript, sizeof(descript));

    if ( (ret = sendmsg(sender, &msg, 0)) == -1){
        COMPLAIN("sendmsg failed (%li): '%s'", ret, strerror(errno));
        return -1;
    }
    return 0;
}

int socks_getdes(int receiver){
    ssize_t ret = 0;
    int descript = -1;

    /* msgbuff will internally be cast to a `struct cmshdr` so we must ensure
     * it's aligned */
    union {
        struct cmsghdr __align;
        char buff[CMSG_SPACE(sizeof(int))];
    } aligned_msg = {0};
    
    struct msghdr msg;     /* argument type expected by sendmsg() and recvmsg() */
    memset(&msg, 0, sizeof(msg));

    int32_t magic    = 0;
    struct iovec iov = {0};
    iov.iov_base     = &magic;
    iov.iov_len      = sizeof(magic);

    msg.msg_iov    = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control =  aligned_msg.buff;
    msg.msg_controllen = sizeof(aligned_msg.buff);
    
    if ((ret = recvmsg(receiver, &msg, 0)) == -1){
        COMPLAIN("recvmsg() failed (%li): '%s'", ret, strerror(errno));
        return -1;
    }

    /* ancillary data is a sequence of cmsghdr structures; 
     * only need 1 here as we send just 1 descriptor */
    struct cmsghdr *control = CMSG_FIRSTHDR(&msg);   /* validate */
    if (!control || control->cmsg_len != CMSG_LEN(sizeof(descript))){
        COMPLAIN("Invalid cmsghdr length (%zu)", CMSG_LEN(sizeof(descript)));
        return -1;
    }
    if (control->cmsg_level != SOL_SOCKET){
        COMPLAIN("Unexpected cmsg_level -- not SOL_SOCKET)");
        return -1;
    }
    if (control->cmsg_type != SCM_RIGHTS){
        COMPLAIN("cmsg_type != SCM_RIGHTS");
        return -1;
    }

    memcpy(&descript, CMSG_DATA(control), sizeof(int));
    SAY("Received FD %d", descript);

    return descript;
}

/*
 * Wrapper around the poll() system call for waiting on a single descriptor.
 *
 * For monitoring multiple descriptors, call poll() directly: there is no
 * advantage to having a wrapper.
 *
 * The timeout value must be in milliseconds and will be passed to poll() as is.
 *
 * <-- return
 *     Whatever poll() returns*; errno is also set by poll().
 *
 *     *except on success; in that case, rather than returning the number of file
 *      descriptors that are ready (which would be meaningless because there is
 *      only ONE descriptor), the .revents mask associated with that descriptor
 *      is returned instead.
 */
int pollfd(int fd, int timeout){
	short evmask     = POLLIN | POLLOUT;
	const int num_fd = 1;
	int ret = 0;

	struct pollfd pollfd = {
		.fd      = fd,
		.events  = evmask,
		.revents = 0
	};

	if ((ret=poll(&pollfd, num_fd, timeout)) > 0){
		return pollfd.revents; /* success */
	}

	return ret;
}
