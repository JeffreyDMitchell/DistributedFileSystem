#include "com.h"

int sendCom(struct comseg * com, int s)
{
    // compose raw com buffer
    char snd_buff[RAW_COM_LEN];
    memset(snd_buff, 0, RAW_COM_LEN);

    *(snd_buff + METHOD_OFF) = com->method;
    *(snd_buff + CNUM_OFF) = com->chunk_num;
    *((long *)(snd_buff + TIMESTAMP_OFF)) = com->time_stamp;
    *((long *)(snd_buff + FSIZE_OFF)) = com->f_size;
    *(snd_buff + FIN_OFF) = com->fin;

    strncpy(snd_buff + FNAME_OFF, com->f_name, MAX_FILE_NAME);

    // send it off
    return sendAll(s, snd_buff, RAW_COM_LEN);
}

int recvCom(struct comseg * com, int s)
{
    char raw[RAW_COM_LEN];
    memset(com, 0, sizeof(struct comseg));
    memset(raw, 0, RAW_COM_LEN);

    // recv the com buffer
    // TODO implement timeout and failure case
    int total = 0;
    int bytes;
    // printf("before recv com\n");
    while(total < RAW_COM_LEN)
    {
        bytes = recv(s, raw + total, RAW_COM_LEN - total, 0);
        if(bytes < 0) return -1;

        total += bytes;
        // printf("total: %d\n", total);
    }
    // printf("after recv com\n");

    // parse the com buffer
    com->method = (char) *(raw + METHOD_OFF);
    com->chunk_num = (char) *(raw + CNUM_OFF);
    com->time_stamp = *((long *)(raw + TIMESTAMP_OFF));
    com->f_size = *((long *)(raw + FSIZE_OFF));
    com->fin = (char) *(raw + FIN_OFF);

    strncpy(com->f_name, (raw + FNAME_OFF), MAX_FILE_NAME);

    return 0;
}

void printCom(struct comseg * com)
{
    printf(
        "Method:      \t%d\n"
        "Chunk Number:\t%d\n"
        "Timestamp:   \t%ld\n"
        "File Size:   \t%ld\n"
        "Fin:         \t%d\n"
        "File Name:   \t%s\n",
        com->method, com->chunk_num, com->time_stamp, com->f_size, com->fin, com->f_name
        );
}



void buildCom(
    struct comseg * com,
    char method,
    char chunk_num,
    long time_stamp,
    long f_size,
    char fin,
    char * f_name
)
{
    memset(com, 0, sizeof(struct comseg));

    com->method = method;
    com->chunk_num = chunk_num;
    com->time_stamp = time_stamp;
    com->f_size = f_size;
    com->fin = fin;

    strcpy(com->f_name, f_name);
}