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
#include <regex.h>

#include <stdlib.h>

#define MAX_QUEUED_CONNECTIONS 20
#define IN_BUFF_LENGTH 4096
#define UTIL_STR_LEN 256
#define MAX_FILE_NAME 240

struct netinfo
{
    struct sockaddr_in sin;
    socklen_t addr_len;
};

// parsed file name
// more convenient than working with strings.
struct entry
{
    int chunk;
    long timestamp;
    char name[MAX_FILE_NAME];
};

int min(int a, int b);
int sendAll(int s, char *buf, int len);
int recvFile(FILE * fptr, long size, int sock);
int parseEntry(struct entry * e, char * f_name);
void md5Str(char * in_str, char * hash_str);
unsigned long md5Sum(char * in_str);

#endif