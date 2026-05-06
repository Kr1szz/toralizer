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