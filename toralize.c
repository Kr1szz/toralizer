#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>

#define TOR_PROXY_HOST "127.0.0.1"
#define TOR_PROXY_PORT 9050
#define BUFFER_SIZE 4096
#define MAX_RETRIES 3
#define TIMEOUT_SECONDS 10

// SOCKS5 protocol constants
#define SOCKS5_VERSION 0x05
#define SOCKS5_CONNECT 0x01
#define SOCKS5_AUTH_NOAUTH 0x00
#define SOCKS5_IPV4 0x01
#define SOCKS5_IPV6 0x04

// Global variables
static int (*real_connect)(int sockfd, const struct sockaddr *addr, socklen_t len) = NULL;
static volatile int is_tor_connection = 0;
static volatile int shutdown_requested = 0;

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    shutdown_requested = 1;
}

// Set socket timeout
int set_socket_timeout(int sockfd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

// Check if we're connecting to Tor proxy
int is_tor_proxy_address(const struct sockaddr *addr, socklen_t len) {
    if (len == sizeof(struct sockaddr_in)) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        return (addr_in->sin_addr.s_addr == inet_addr(TOR_PROXY_HOST));
    }
    return 0;
}

// SOCKS5 authentication with retry logic
int socks5_authenticate_with_retry(int sockfd) {
    unsigned char auth[2];
    int retry = 0;
    
    while (retry < MAX_RETRIES && !shutdown_requested) {
        // Send no-authentication method
        auth[0] = SOCKS5_VERSION;
        auth[1] = SOCKS5_AUTH_NOAUTH;
        
        if (send(sockfd, auth, 2, 0) < 0) {
            retry++;
            continue;
        }
        
        // Receive authentication response
        unsigned char response;
        if (recv(sockfd, &response, 1, 0) < 0) {
            retry++;
            continue;
        }
        
        if (response == SOCKS5_AUTH_NOAUTH) {
            return 0;
        }
        
        retry++;
        usleep(100000); // 100ms delay
    }
    
    return -1;
}

// SOCKS5 connect request with timeout
int socks5_connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t len) {
    unsigned char buf[BUFFER_SIZE];
    unsigned char reply[BUFFER_SIZE];
    unsigned char address[16];
    int len_type = 0;
    
    // Determine address type
    if (len == sizeof(struct sockaddr_in)) {
        len_type = SOCKS5_IPV4;
    } else if (len == sizeof(struct sockaddr_in6)) {
        len_type = SOCKS5_IPV6;
    } else {
        return -1;
    }
    
    // Set timeout
    if (set_socket_timeout(sockfd, TIMEOUT_SECONDS) < 0) {
        return -1;
    }
    
    // SOCKS5 handshake
    buf[0] = SOCKS5_VERSION;
    buf[1] = SOCKS5_CONNECT;
    buf[2] = 0; // Number of authentication methods
    
    if (send(sockfd, buf, 3, 0) < 0) {
        return -1;
    }
    
    // Receive authentication response
    if (recv(sockfd, &reply[0], 1, 0) < 0) {
        return -1;
    }
    
    if (reply[0] != 0x00) {
        return -1;
    }
    
    // Build connect request
    buf[0] = SOCKS5_VERSION;
    buf[1] = SOCKS5_CONNECT;
    buf[2] = 0; // Reserved
    buf[3] = len_type; // Address type
    
    if (len_type == SOCKS5_IPV4) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
        memcpy(&buf[4], &addr_in->sin_addr, 4);
        buf[8] = htons(addr_in->sin_port) & 0xFF;
        buf[9] = htons(addr_in->sin_port) >> 8;
    } else if (len_type == SOCKS5_IPV6) {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
        memcpy(&buf[4], &addr_in6->sin6_addr, 16);
        buf[4] = 0; // Compress IPv6 address if possible
        buf[5] = 0;
        buf[6] = htons(addr_in6->sin6_port) & 0xFF;
        buf[7] = htons(addr_in6->sin6_port) >> 8;
    }
    
    // Send connect request
    if (send(sockfd, buf, 10, 0) < 0) {
        return -1;
    }
    
    // Receive reply
    if (recv(sockfd, &reply[0], 1, 0) < 0) {
        return -1;
    }
    
    if (reply[0] != 0x00) {
        return -1;
    }
    
    return 0;
}

// Override connect function with enhanced features
int connect(int sockfd, const struct sockaddr *addr, socklen_t len) {
    // Check for shutdown
    if (shutdown_requested) {
        return real_connect(sockfd, addr, len);
    }
    
    // If this is a connection to Tor proxy, use real connect
    if (is_tor_proxy_address(addr, len)) {
        return real_connect(sockfd, addr, len);
    }
    
    // Create socket to Tor proxy
    int tor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tor_fd < 0) {
        return -1;
    }
    
    struct sockaddr_in tor_addr;
    memset(&tor_addr, 0, sizeof(tor_addr));
    tor_addr.sin_family = AF_INET;
    tor_addr.sin_port = htons(TOR_PROXY_PORT);
    
    if (inet_pton(AF_INET, TOR_PROXY_HOST, &tor_addr.sin_addr) <= 0) {
        close(tor_fd);
        errno = EADDRNOTAVAIL;
        return -1;
    }
    
    // Connect to Tor proxy with retry
    int retry = 0;
    while (retry < MAX_RETRIES && !shutdown_requested) {
        if (real_connect(tor_fd, (struct sockaddr *)&tor_addr, sizeof(tor_addr)) >= 0) {
            break;
        }
        
        if (errno != EINPROGRESS) {
            close(tor_fd);
            return -1;
        }
        
        retry++;
        usleep(100000); // 100ms delay
    }
    
    if (shutdown_requested) {
        close(tor_fd);
        return -1;
    }
    
    // Perform SOCKS5 authentication
    if (socks5_authenticate_with_retry(tor_fd) < 0) {
        close(tor_fd);
        errno = ETIMEDOUT;
        return -1;
    }
    
    // Send SOCKS5 connect request
    if (socks5_connect_with_timeout(tor_fd, addr, len) < 0) {
        close(tor_fd);
        errno = ETIMEDOUT;
        return -1;
    }
    
    // Close Tor proxy connection as SOCKS5 handshake is complete
    close(tor_fd);
    
    return 0;
}

// Register signal handlers
__attribute__((constructor))
void init_toralizer() {
    // Get real connect function
    real_connect = dlsym(RTLD_NEXT, "connect");
    if (!real_connect) {
        fprintf(stderr, "Error: Could not find real connect function\n");
        exit(1);
    }
    
    // Set up signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
}
