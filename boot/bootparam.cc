#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "bootparam.h"

int main(int argc, char **argv)
{
        if (argc != 3) {
                fprintf(stderr, "usage: bootparam image /dev/hda?\n");
                return 0;
        }
        short devnum = 3*256 + atoi(argv[2] + strlen(argv[2]) - 1);
        printf("rootdev is %x\n", devnum);

        int image = open(argv[1], O_RDWR);
        if (image < 0) {
                perror("open");
                return 0;
        }
        lseek(image, 512 + XROOTDEV, 0);
        write(image, &devnum, 2);
        close(image);
        return 0;
}
