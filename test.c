#include "com.h"
#include "netutils.h"

int main(int argc, char ** argv)
{
    long it;
    char blocker = 0;
    // if(argc < 2) return -1;

    // printf("%lu\n", md5Sum(argv[1]));

    for(it = 0; it < 1000l; it++)
        printf("%u\n", (unsigned int) md5Sum((char *) &it));
}