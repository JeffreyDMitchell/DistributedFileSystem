#include "netutils.h"

int min(int a, int b)
{
    return a < b ? a : b;
}

// citation https://beej.us/guide/bgnet/html/
int sendAll(int s, char *buf, int len)
{
    int total = 0;        
    int bytesleft = len; 
    int n;

    while(bytesleft) 
    {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) return -1;
        total += n;
        bytesleft -= n;
    }

    return total;
}

unsigned long md5Sum(char * in_str)
{
    unsigned char md5_hash[MD5_DIGEST_LENGTH];

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, in_str, strlen(in_str));
    MD5_Final(md5_hash, &ctx);

    return *((long *) md5_hash);
}

void md5Str(char * in_str, char * hash_str)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, in_str, strlen(in_str));
    MD5_Final((unsigned char *) hash_str, &ctx);
}