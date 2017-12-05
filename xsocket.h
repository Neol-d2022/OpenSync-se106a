#ifndef _XSOCKET_H_LOADED
#define _XSOCKET_H_LOADED

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* Windows XP. */
#endif
#include <Ws2tcpip.h>
#include <winsock2.h>

#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <arpa/inet.h>
#include <netdb.h> /* Needed for getaddrinfo() and freeaddrinfo() */
#include <sys/socket.h>
#include <unistd.h> /* Needed for close() */
typedef int SOCKET;
#define INVALID_SOCKET (-1)

#endif

int socketLibInit(void);
int socketLibDeInit(void);
int socketClose(SOCKET sock);

#endif
/* Based on the answer at https://stackoverflow.com/questions/28027937/cross-platform-sockets */
