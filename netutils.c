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

int recvFile(FILE * fptr, long size, int sock)
{
    char in_buff[IN_BUFF_LENGTH];
    memset(in_buff, 0, IN_BUFF_LENGTH);

    long total = 0;
    int last_chunk;
    while(total < size)
    {
        if((last_chunk = recv(sock, in_buff, min(size - total, IN_BUFF_LENGTH), 0)) == -1)
            return -1;

        fwrite(in_buff, sizeof(char), last_chunk, fptr);
        total += last_chunk;
    }

    return total;
}

int parseEntry(struct entry * e, char * f_name)
{
    memset(e, 0, sizeof(struct entry));

    // setup regex
    regex_t entry_line;
    int group_ct = 4;
    regmatch_t groups[group_ct];
    char utilstr[UTIL_STR_LEN];
    if(regcomp(&entry_line, "([0-9]+)\\.([0-9]+)\\.(.+)", REG_EXTENDED))
    {
        printf("Regex compilation failed. (file name / entry parser)\n");
        exit(-1);
    }

    if(regexec(&entry_line, f_name, group_ct, groups, 0) == REG_NOMATCH) 
    {
        printf("Invalid filename: %s\n", f_name);
        regfree(&entry_line);
        return -1;
    }

    // grab chunk number
    memset(utilstr, 0, UTIL_STR_LEN);
    strncpy(utilstr, (f_name + groups[1].rm_so), (groups[1].rm_eo - groups[1].rm_so));
    e->chunk = atoi(utilstr);

    // grab timestamp
    memset(utilstr, 0, UTIL_STR_LEN);
    strncpy(utilstr, (f_name + groups[2].rm_so), (groups[2].rm_eo - groups[2].rm_so));
    e->timestamp = atol(utilstr);

    // grab filename
    strncpy(e->name, (f_name + groups[3].rm_so), (groups[3].rm_eo - groups[3].rm_so));

    regfree(&entry_line);
    return 0;
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