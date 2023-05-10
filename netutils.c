#include "netutils.h"

int min(int a, int b)
{
    return a < b ? a : b;
}

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
// citation: https://stackoverflow.com/questions/2597608/c-socket-connection-timeout
// this was brutal... I figured connection timeout would be as simple as a call to setsockopt
// instead, I spent an hour reading about select, poll, and syncrhonous IO.
// code below is essentially the same as the best practice laid out in the stack overflow thread
int connectTimeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms) 
{
    int rc = 0;

    // configure socket to be non-blocking
    int initial_flags;
    if((initial_flags = fcntl(sockfd, F_GETFL, 0) < 0)) 
        return -1;
    
    if(fcntl(sockfd, F_SETFL, initial_flags | O_NONBLOCK) < 0) 
        return -1;

    // begin connection
    do 
    {
        if(connect(sockfd, addr, addrlen) < 0)
        {

            // fail if connect errors
            if((errno != EWOULDBLOCK) && (errno != EINPROGRESS)) 
            {
                rc = -1;
            }
            else 
            {
                // set deadline to now + timeout_ms
                struct timespec now;
                if(clock_gettime(CLOCK_MONOTONIC, &now) < 0) 
                { 
                    rc=-1; 
                    break; 
                }
                struct timespec deadline = { .tv_sec = now.tv_sec, .tv_nsec = now.tv_nsec + timeout_ms*1000000l};
                // wait for the connection to complete.
                do {
                    // check deadline
                    if(clock_gettime(CLOCK_MONOTONIC, &now) < 0) 
                    { 
                        rc=-1; 
                        break; 
                    }

                    int ms_until_deadline = (int)((deadline.tv_sec - now.tv_sec) * 1000l + (deadline.tv_nsec - now.tv_nsec) / 1000000l);
                    if(ms_until_deadline < 0) 
                    { 
                        // we're past the deadline, break out
                        rc=0; 
                        break; 
                    }

                    // wait for connect to complete
                    // TODO change this to a normal struct, reference?
                    struct pollfd pfds[] = { { .fd = sockfd, .events = POLLOUT } }; // wtf...
                    rc = poll(pfds, 1, ms_until_deadline);

                    // check if poll succeeded
                    if(rc>0) 
                    {
                        int error = 0; 
                        socklen_t len = sizeof(error);
                        int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

                        if(retval==0) 
                            errno = error;
                        if(error!=0) 
                            rc=-1;
                    }
                }
                // If poll was interrupted, try again.
                while(rc==-1 && errno==EINTR);
                // fail on timeout
                if(rc==0) 
                {
                    errno = ETIMEDOUT;
                    rc=-1;
                }
            }
        }
    } 
    // this nastly little loop allows us to cont / break. sorta feels like a hacky equivalent to GOTO, i wonder why people use this...
    while(0);

    // restore original flags
    if(fcntl(sockfd, F_SETFL, initial_flags) < 0) 
        return -1;
    
    // if we got here, success!
    return rc;
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