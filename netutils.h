#ifndef _NETUTILS_H_
#define _NETUTILS_H_

#include <errno.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <string.h>

#define MAX_QUEUED_CONNECTIONS 20

struct netinfo
{
    struct sockaddr_in sin;
    socklen_t addr_len;
};

int min(int a, int b);
int sendAll(int s, char *buf, int len);
void md5Str(char * in_str, char * hash_str);
unsigned long md5Sum(char * in_str);

#endif