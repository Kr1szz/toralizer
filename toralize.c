#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TOR_PROXY_HOST "127.0.0.1"
#define TOR_PROXY_PORT 9050
#define BUFFER_SIZE 4096

/*
SOCKS5  protocol constants
*/
#define SOCKS5_VERSION 0x05
#define SOCKS5_CONNECT 0x01
#define SOCKS5_AUTH_NOAUTH 0x00
#define SOCKS5_IPV4 0x01
#define SOCKS5_IPV6 0x04

/*
Global variables
*/
static int (*real_connect)(int sockfd, const struct sockaddr *addr,
                           socklen_t len) = NULL;
static int is_tor_connection = 0;

/*
Checking if we are connecting to Tor  proxy
*/
int is_tor_proxy_address(const struct sockaddr *addr, socklen_t len) {
  if (len == sizeof(struct sockaddr_in)) {
    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
    return (addr_in->sin_addr.s_addr == inet_addr(TOR_PROXY_HOST));
  }
  return 0;
}

/*
Soks5 authentication
*/
int socks5_authenticate(int sockfd) {
  unsigned char auth[2];
  auth[0] = SOCKS5_VERSION;
  auth[1] = SOCKS5_AUTH_NOAUTH;
  /* file descriptor, buffer, literal and flags */
  if (send(sockfd, auth, 2, 0) < 0)
    return -1;

  unsigned char response;
  if (recv(sockfd, &response, 1, 0) < 0)
    return -1;

  if (response != SOCKS5_AUTH_NOAUTH)
    return -1;

  return 0;
}


/* SOCKS5 Connection Request*/
int socks5_connect(int sockfd, const struct sockaddr_in *addr) {
  unsigned char buffr[BUFFER_SIZE];
  unsigned char reply[BUFFER_SIZE];
  unsigned char address[0];
  int len = 0;

  // socks5 handshake
  buffr[0] = SOCKS5_VERSION;
  buffr[1] = SOCKS5_CONNECT;
  // Number of authentication methods

  if (send(sockfd, buffr, 3, 0) < 0)
    return -1;

  if (recv(sockfd, &reply[0], 1, 0) < 0)
    return -1;

  if (reply[0] != 0x00)
    return -1;

  // Build connect request
  buffr[0] = SOCKS5_VERSION;
  buffr[1] = SOCKS5_CONNECT;
  buffr[2] = 0;
  buffr[3] = SOCKS5_IPV4;
  // copy ip addresses

  memcpy(&buffr[4], &addr->sin_addr, 4);
  buffr[8] = htons(addr->sin_port) & 0xFF;
  buffr[9] = htons(addr->sin_port) >>8 ;
     // Send connect request
    if (send(sockfd, buffr, 10, 0) < 0) 
        return -1;
    
    
    // Receive reply
    if (recv(sockfd, &reply[0], 1, 0) < 0) 
        return -1;
    
    
    if (reply[0] != 0x00) 
        return -1;

  return 0;
}


// Override connect function
int connect(int sockfd, const struct sockaddr *addr, socklen_t len) {
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
        return -1;
    }
    
    // Connect to Tor proxy
    if (real_connect(tor_fd, (struct sockaddr *)&tor_addr, sizeof(tor_addr)) < 0) {
        close(tor_fd);
        return -1;
    }
    
    // Perform SOCKS5 authentication
    if (socks5_authenticate(tor_fd) < 0) {
        close(tor_fd);
        return -1;
    }
    
    // Send SOCKS5 connect request
    if (socks5_connect(tor_fd, (struct sockaddr_in *)addr) < 0) {
        close(tor_fd);
        return -1;
    }
    
    // Close Tor proxy connection as SOCKS5 handshake is complete
    close(tor_fd);
    
    return 0;
}
// Initialization function
__attribute__((constructor))
void init_toralizer() {
    // Get real connect function
    real_connect = dlsym(RTLD_NEXT, "connect");
    if (!real_connect) {
        fprintf(stderr, "Error: Could not find real connect function\n");
        exit(1);
    }
}