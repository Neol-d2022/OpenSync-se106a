#include <signal.h>

#include "xsocket.h"

int socketLibInit(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

int socketLibDeInit(void)
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

int socketClose(SOCKET sock)
{
    int status = 0;

#ifdef _WIN32
    status = shutdown(sock, SD_BOTH);
    if (status == 0)
        status = closesocket(sock);
#else
    status = shutdown(sock, SHUT_RDWR);
    if (status == 0)
        status = close(sock);
#endif

    return status;
}

/* Based on the answer at https://stackoverflow.com/questions/28027937/cross-platform-sockets */
