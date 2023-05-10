#ifndef _COM_H_
#define _COM_H_

#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "netutils.h"

#define RAW_COM_LEN 256

// this was arbitrary... pick a better number
#define MAX_CHUNK_NAME 512

// com buffer offsets
#define METHOD_OFF 0
#define CNUM_OFF 1
#define TIMESTAMP_OFF 2
#define FSIZE_OFF 10
#define FIN_OFF 18
#define FNAME_OFF 19

// methods
#define INVALID -1
#define GET 0
#define PUT 1
#define LST 2
#define SUCCESS 3

struct comseg
{
    char    method;
    char    chunk_num;
    long    time_stamp;
    long    f_size;
    char    fin;
    char    f_name[MAX_FILE_NAME + 1];
};

int sendCom(struct comseg * com, int s);
int recvCom(struct comseg * com, int s);
void printCom(struct comseg * com);
void buildCom(
    struct comseg * com,
    char method,
    char chunk_num,
    long time_stamp,
    long f_size,
    char fin,
    char * f_name
);

#endif