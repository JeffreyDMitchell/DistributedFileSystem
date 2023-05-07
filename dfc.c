
/*
TODO:
    - properly get home directory!!!!!
    - adapt com (de)serialization to be endian agnostic
    - some bug where a gile was uplaoded without a name???
        - MUST MAKE SURE THAT FILE IS NOT DIRECTORY
CONCERNS:
    - when putting multiple files, i dont do any sort of preliminary check
      to ensure that servers are up, I will just fail them one at a time after timeout
      this could take a while
    - as file length is indicated by a 4 byte value, file size is capped at 4GB
*/

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <stdio.h>
#include <sys/time.h>

#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "netutils.h"
#include "com.h"

#define CONFIG_PATH "/home/jeff/dfc.conf"
#define CONFIG_MAX_LINE 1024
#define UTIL_STR_LEN 256
#define SERVER_CT 4
#define SERVER_NAME_LENGTH 32
// determines level of redundancy.
#define DIST_FACTOR 2

struct s_info
{
    struct netinfo net_inf;
    char name[SERVER_NAME_LENGTH];
};

// parsed file name
// more convenient than working with strings.
struct entry
{
    int chunk;
    long timestamp;
    char name[MAX_FILE_NAME];
};

struct version
{
    long timestamp;
    // 'booleans', indicate presence of a given chunk
    int chunks[SERVER_CT];
};

struct f_stub
{
    char name[MAX_FILE_NAME];
    struct version * versions;
    int version_ct;
};

// supported commands
char * commands[] = {"get", "put", "list", NULL};
struct s_info servers[SERVER_CT];

int parseCommand(char * command)
{
    for(int i = 0; commands[i]; i++)
        if(!strcasecmp(commands[i], command))
            return i;

    return -1;
}

char * nameFromPath(char * path)
{
    char * filename = strrchr(path, '/');

    if(filename)
        return filename + 1;

    return path;
}

int allChunksPresent(int * chunk_arr)
{
    int ret = 1;
    for(int i = 0; i < SERVER_CT; i++)
        ret &= chunk_arr[i];

    return ret;
}

int versionCmp(const void * a, const void * b)
{
    struct version * v_a = (struct version *) a;
    struct version * v_b = (struct version *) b;

    if(v_a->timestamp > v_b->timestamp) return 1;
    if(v_a->timestamp < v_b->timestamp) return -1;

    return 0;
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

void initServers()
{
    // initialize servers
    memset(servers, 0, SERVER_CT * sizeof(struct netinfo));

    FILE * config;
    if(!(config = fopen(CONFIG_PATH, "r")))
    {
        printf("Config file not found.\n");
        exit(-1);
    }

    // setup regex
    regex_t server_line;
    int group_ct = 4;
    regmatch_t groups[group_ct];
    char utilstr[UTIL_STR_LEN];
    if(regcomp(&server_line, "server ([A-z0-9]+) ([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+):([0-9]+)", REG_EXTENDED))
    {
        printf("Regex compilation failed. (config server line)\n");
        exit(-1);
    }

    // arbitrary decision for max line length...
    char line[CONFIG_MAX_LINE];
    memset(line, 0, CONFIG_MAX_LINE);
    int ct = 0;

    while(ct < SERVER_CT && fgets(line, CONFIG_MAX_LINE, config))
    {
        // printf("%d: %s\n", ct++, line);
        if(regexec(&server_line, line, group_ct, groups, 0) == REG_NOMATCH) 
        {
            printf("Invalid line in server config: %s\n", line);
            continue;
        }

        // get server name
        strncpy(servers[ct].name, (line + groups[1].rm_so), (groups[1].rm_eo - groups[1].rm_so));

        // populate ip
        // TODO limit n here, potential buffer overflow
        // also do something better to append a null char to the end rather than just 
        // zeroing string first lol
        memset(utilstr, 0, UTIL_STR_LEN);
        strncpy(utilstr, (line + groups[2].rm_so), (groups[2].rm_eo - groups[2].rm_so));
        inet_pton(AF_INET, utilstr, &servers[ct].net_inf.sin.sin_addr.s_addr);
        
        // populate port
        memset(utilstr, 0, UTIL_STR_LEN);
        strncpy(utilstr, (line + groups[3].rm_so), (groups[3].rm_eo - groups[3].rm_so));
        servers[ct].net_inf.sin.sin_port = htons(atoi(utilstr));

        // fill out remainder of netinfo struct for server
        servers[ct].net_inf.sin.sin_family = AF_INET;
        servers[ct].net_inf.addr_len = sizeof(servers[ct].net_inf.sin);

        ct++;
        // printf("Successfully populated server.\n");
    }

    regfree(&server_line);

    if(ct < SERVER_CT)
    {
        printf("Too few servers outlined in %s.\n", CONFIG_PATH);
        exit(-1);
    }
}

// queries servers and populates a list of file 'stubs'
// stubs should be NULL, and ct 0!
void getFileState(struct f_stub ** stubs, int * stub_ct)
{
    int client_sock;
    *stub_ct = 0;

    // for each server
    for(int server_num = 0; server_num < SERVER_CT; server_num++)
    {
        if((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            printf("Failed to create socket.\n");
            continue;
        }

        if(connect(client_sock, (struct sockaddr *) &servers[server_num].net_inf.sin, servers[server_num].net_inf.addr_len) == -1)
        {
            printf("Failed to connect to server '%s'.\n", servers[server_num].name);
            continue;
        }

        // TODO temp testing prints. remove before submission
        // printf("Successfully connected to server '%s'!\n", servers[server_num].name);

        // CONNECTION STUFF STARTS HERE
        struct comseg com;
        buildCom(
            &com,
            LST,
            0,
            0,
            0,
            1,
            ""
        );

        // printCom(&com);
        sendCom(&com, client_sock);

        // recieve names of all files present on server
        // TODO scary inf loop
        while(1)
        {
            // recieve com segment from server
            recvCom(&com, client_sock);

            // if finished, get out
            if(com.fin) break;

            struct entry e;
            if(parseEntry(&e, com.f_name))
            {
                printf("Failed to parse entry for string: %s\n", com.f_name);
                continue;
            }

            // printf(
            //     "Recieved entry:\n"
            //     "Chunk Number: %d\n"
            //     "Timestamp: %ld\n"
            //     "File Name: %s\n",
            //     e.chunk, e.timestamp, e.name);

            // search stubs list for file name
            struct f_stub * cur_stub = NULL;
            for(int i = 0; i < *stub_ct; i++)
            {
                // names do not match, continue
                if(strcmp((*stubs)[i].name, e.name)) continue;

                cur_stub = &(*stubs)[i];
                break;
            }

            // no existing stub found
            if(cur_stub == NULL)
            {
                // add another stub
                (*stubs) = realloc((*stubs), ((*stub_ct)+1) * sizeof(struct f_stub));
                cur_stub = (*stubs) + (*stub_ct);
                *stub_ct = (*stub_ct) + 1;
                
                // init stub (the version bits are redundant with the call to memset)
                memset(cur_stub, 0, sizeof(struct f_stub));
                strcpy(cur_stub->name, e.name);
                cur_stub->version_ct = 0;
                cur_stub->versions = NULL;
            }

            // cur now points to the file stub for this file name
            // need to see if version currently exists or not
            // much the same process as above
            struct version * cur_vers = NULL;
            for(int i = 0; i < cur_stub->version_ct; i++)
            {
                // names do not match, continue
                if(e.timestamp != cur_stub->versions[i].timestamp) continue;

                cur_vers = &cur_stub->versions[i];
                break;
            }

            // no existing version found
            if(cur_vers == NULL)
            {
                // add another version
                // cur_vers = cur_stub->versions + cur_stub->version_ct;
                cur_stub->versions = realloc(cur_stub->versions, (cur_stub->version_ct + 1) * sizeof(struct version));
                cur_vers = cur_stub->versions + (cur_stub->version_ct);
                cur_stub->version_ct = cur_stub->version_ct + 1;

                // init version
                memset(cur_vers, 0, sizeof(struct version));
                cur_vers->timestamp = e.timestamp;
                // again, this bit is redundant with the memset call
                for(int i = 0; i < SERVER_CT; i++)
                    cur_vers->chunks[i] = 0;
            }

            // mark chunk of current version accounted for...
            // all that work
            cur_vers->chunks[e.chunk] = 1;
        }

        // terminate connection
        shutdown(client_sock, SHUT_RDWR);
        close(client_sock);
    }

    // TODO sort chunks by timestamp, value more recent versions
    // this is.. mostly untested
    for(int i = 0; i < (*stub_ct); i++)
        qsort((*stubs)[i].versions, (*stubs)[i].version_ct, sizeof(struct version), versionCmp);
}

void putRoutine(int ct, char ** files)
{
    // for each file indicated by client...
    int client_sock, fd;
    off_t f_length;
    struct stat file_stat;
    struct timeval tv;
    for(int f_num = 0; f_num < ct; f_num++)
    {   
        if((fd = open(files[f_num], O_RDONLY)) == -1)
        {
            printf("File '%s' does not exist, or cannot be read.\n", files[f_num]);
            continue;
        }

        if(fstat(fd, &file_stat) == -1)
        {
            printf("Failed to get file stats for file '%s'.\n", files[f_num]);
            close(fd);
            continue;
        }

        // get file size
        f_length = file_stat.st_size;

        // get time for timestamp
        gettimeofday(&tv, NULL);

        // for each server
        for(int server_num = 0; server_num < SERVER_CT; server_num++)
        {
            if((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                printf("Failed to create socket for file '%s'. Continuing...\n", files[f_num]);
                continue;
            }

            if(connect(client_sock, (struct sockaddr *) &servers[server_num].net_inf.sin, servers[server_num].net_inf.addr_len) == -1)
            {
                // printf("Failed to connect to server '%s' for file '%s'. Continuing...\n", servers[server_num].name, files[f_num]);
                // continue;

                // per discussion in lecture, if even a single server is down, put should fail.
                printf("%s put failed.\n", nameFromPath(files[f_num]));
                break;
            }

            // TODO temp testing prints. remove before submission
            // printf("Successfully connected to server '%s' for file '%s'!\n", servers[server_num].name, files[f_num]);

            // CONNECTION STUFF STARTS HERE
            
            // compute md5 hash of file for more varied distribution (sort of...)
            // TODO signedness issues???? ahhhh
            unsigned int hash_off = (unsigned int) md5Sum(nameFromPath(files[f_num]));
            struct comseg com;
            for(int c = 0; c < DIST_FACTOR; c++)
            {
                unsigned int chunk_num = (server_num + hash_off + c) % SERVER_CT;

                off_t off_cur = (chunk_num * f_length / SERVER_CT);
                off_t off_next = ((chunk_num + 1) * f_length / SERVER_CT);

                buildCom(
                    &com,
                    PUT,                                    // method
                    chunk_num,                              // chunk number
                    (tv.tv_sec * (int)1E6) + tv.tv_usec,    // timestamp, milliseconds
                    (off_next - off_cur),                   // file size
                    (c == DIST_FACTOR-1),                   // fin (no more chunks)
                    nameFromPath(files[f_num])              // file name
                );
                // printCom(&com);
                sendCom(&com, client_sock);

                sendfile(
                    client_sock, 
                    fd, 
                    &off_cur, 
                    (off_next - off_cur)
                );
            }

            // terminate connection
            shutdown(client_sock, SHUT_RDWR);
            close(client_sock);
        }

        close(fd);
    }
}

void getRoutine(int ct, char ** files)
{
    // TODO
}

void lstRoutine()
{
    struct f_stub * stubs = NULL;
    int stub_ct = 0;
    getFileState(&stubs, &stub_ct);

    struct f_stub * cur_stub;
    struct version * cur_vers;
    int complete;
    for(int f_idx = 0; f_idx < stub_ct; f_idx++)
    {
        complete = 0;
        cur_stub = &stubs[f_idx];
        for(int v_idx = 0; v_idx < cur_stub->version_ct; v_idx++)
        {
            cur_vers = &cur_stub->versions[v_idx];
            if(allChunksPresent(cur_vers->chunks))
            {
                complete = 1;
                break;
            }
        }

        printf("%s%s\n", cur_stub->name, (complete ? "" : " [incomplete]"));
    }

    // TODO must free structures
    for(int f_idx = 0; f_idx < stub_ct; f_idx++)
        free(stubs[f_idx].versions);

    free(stubs);
}

int main(int argc, char ** argv)
{
    // TODO change this to allow list...
    // if(argc < 3)
    // {
    //     printf("Usage: %s [command] [filename] ... [filename]\n", argv[0]);
    //     exit(-1);
    // }

    initServers();

    // switch based on command
    switch(parseCommand(argv[1]))
    {
        case GET:
        getRoutine(argc - 2, argv + 2);
        break;

        case PUT:
        putRoutine(argc - 2, argv + 2);
        break;

        case LST:
        lstRoutine();
        break;    

        default:
        printf("Unrecognized command.\n");    
    }
}