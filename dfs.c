/*
TODO
    - make sure exitting works properly
*/

#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "netutils.h"
#include "com.h"

#define IN_BUFF_LENGTH 1024

struct thread_args
{
    int client_sock;
};

void putRoutine(struct comseg * com, int sock)
{
    // build file name
    char chunk_name[MAX_CHUNK_NAME];
    memset(chunk_name, 0, MAX_CHUNK_NAME);
    // <chunknumber>.<timestamp>.<f_name>
    sprintf(chunk_name, "%d.%ld.%s", com->chunk_num, com->time_stamp, com->f_name);

    // create file
    FILE * fptr;
    if((fptr = fopen(chunk_name, "w")) == NULL)
    {
        printf("Could not create file.\n");
        // TODO what should we do here? shut down connection? probably
        return;
    }

    char in_buff[IN_BUFF_LENGTH];
    memset(in_buff, 0, IN_BUFF_LENGTH);

    long bytes = com->f_size;
    long total = 0;
    int last_chunk;
    while(total < bytes)
    {
        last_chunk = recv(sock, in_buff, min(bytes - total, IN_BUFF_LENGTH), 0);
        fwrite(in_buff, sizeof(char), last_chunk, fptr);
        total += last_chunk;
    }

    fclose(fptr);
    // printf("Done!!\n\n");
}

void getRoutine(struct comseg * com)
{
    
}

void lstRoutine(struct comseg * com, int sock)
{
    // get all files in directory
    DIR *dir;
    struct dirent * entry;

    if(!(dir = opendir("./")))
    {
        perror("Unable to open directory.\n");
        return;
    }

    while((entry = readdir(dir)))
    {
        // not '.' or '..'
        if(!(strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))) continue;

        buildCom(
            com,
            SUCCESS,
            0,
            0,
            0,
            0,
            entry->d_name
        );
        printCom(com);
        sendCom(com, sock);
    }

    buildCom(
        com,
        SUCCESS,
        0,
        0,
        0,
        1,
        ""
    );
    printCom(com);
    sendCom(com, sock);

    closedir(dir);
}   

void manageDirectory(char *path)
{
    struct stat info;

    if(stat(path, &info))
    {
        // dir does not exist, must create
        // why is everyone so obnoxious regarding these permissions...
        // like just tell me what to put here stop monologuing
        if(mkdir(path, S_IRWXU | S_IRWXO | S_IRWXG))
        {
            printf("Failed to create directory.\n");
            exit(-1);
        }

        // created successfully
        return;
    }
    else if(S_ISDIR(info.st_mode))
    {
        // directory exists
        return;
    }

    // path exists, is not a directory
    printf("Invalid directory.\n");
    exit(-1);
}

void * connTask(void * t_args)
{
    pthread_detach(pthread_self());
    struct thread_args args = *((struct thread_args *) t_args);
    free(t_args);

    printf("Server recieved a connection.\n");

    // recieve command segment
    struct comseg com;
    
    do
    {
        recvCom(&com, args.client_sock);

        printCom(&com);

        switch(com.method)
        {
            case PUT:
            printf("Putting...\n");
            putRoutine(&com, args.client_sock);
            break;

            case GET:
            printf("Getting...\n");
            break;

            case LST:
            printf("Listing...\n");
            lstRoutine(&com, args.client_sock);
            break;

            default:
            printf("Unrecognized method recieved. That's not good...\n");
        }
    }
    while(!com.fin);

    close(args.client_sock);

    return NULL;
}

int main(int argc, char ** argv)
{
    int client_sock, server_sock;
    int port_num;
    struct netinfo server_info, client_info;


    // SIGNALS
    signal(SIGPIPE, SIG_IGN);

    // verify command line arguments
    if(argc < 3)
    {
        printf("Usage: %s [directory] [port]\n", argv[0]);
        exit(-1);
    }

    // check if directory exists, create if missing, exit if impossible
    manageDirectory(argv[1]);
    // if directory management was successful, change directory to provided
    chdir(argv[1]);

    // check port
    if((port_num = atoi(argv[2])) == 0)
    {
        printf("Bad port.\n");
        exit(-1);
    }

    // configuring server info
    memset(&server_info, 0, sizeof(server_info));
    server_info.sin.sin_family = AF_INET;
    server_info.sin.sin_port = htons(port_num);
    server_info.sin.sin_addr.s_addr = INADDR_ANY;
    server_info.addr_len = sizeof(server_info.sin);

    // preparing client info
    memset(&client_info, 0, sizeof(client_info));
    client_info.addr_len = sizeof(client_info.sin);

    // create socket
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Socket creation error.\n");
        exit(-1);
    }

    int dumb = 1;
    if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &dumb, sizeof(dumb)) < 0) 
    {
        perror("setsockopt");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // bind
    if(bind(server_sock, (struct sockaddr *) &server_info.sin, server_info.addr_len) == -1)
    {
        printf("Bind failed.\n");
        exit(-1);
    }

    if(listen(server_sock, MAX_QUEUED_CONNECTIONS) == -1)
    {
        printf("Listen failed.\n");
        exit(-1);
    }

    while(1)
    {
        // accept incoming connections
        if((client_sock = accept(server_sock, (struct sockaddr *) &client_info.sin, (socklen_t *) &client_info.addr_len)) == -1)
        {
            printf("Accept failure.\n");
            continue;
        }

        // create and set up thread arguments
        struct thread_args * args = malloc(sizeof(struct thread_args));
        args->client_sock = client_sock;

        pthread_t t;
        if(pthread_create(&t, NULL, connTask, (void *) args))
        {
            // for now we'll just drop the connection and continue. I doubt this will happen much if at all
            printf("Failed to spawn thread.\n");
            close(client_sock);
            continue;
        }
    }
}