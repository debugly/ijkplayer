#ifndef NETWORK_H
#define NETWORK_H

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>

//struct sockaddr_storage {
//#if HAVE_STRUCT_SOCKADDR_SA_LEN
//    uint8_t ss_len;
//    uint8_t ss_family;
//#else
//    uint16_t ss_family;
//#endif /* HAVE_STRUCT_SOCKADDR_SA_LEN */
//    char ss_pad1[6];
//    int64_t ss_align;
//    char ss_pad2[112];
//};

char * inet_ntop4(const u_char *src, char *dst, socklen_t size);
char * inet_ntop6(const u_char *src, char *dst, socklen_t size);
char * inet_ntop_win32(int af, const void *src, char *dst, socklen_t size);

#endif /* NETWORK_H */
