//
// Created by junling on 2017/9/22.
//

#include "out.h"

#include <stdio.h>
#include <sys/time.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    //char buf[1024];
    //int fd = open("./t.h264", O_WRONLY|O_CREAT);
    //close(fd);
    /*fd=open(“/tmp/temp”,O_RDONLY);
    size=read(fd,buffer,sizeof(buffer));
    close(fd);*/
    int i = 0;
    while(1)
    {
        //memset(buf, 0, 1024);
        /*int r = 0;
        if((r = read(STDIN_FILENO, buf, 1024)) > 0)
        {
            write(fd, buf, r);
        }
        else if(r < 0)
        {
            printf("error\n");
            return -1;
        }
        else
        {
            break;
        }*/

        printf("%d\n", i++);

    }

    return 0;
}