//
// Created by junling on 2017/9/21.
//

#include "test.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory>
#include "protocol.h"

#define __DEBUG
#include "util.h"


int main()
{
    //signal(SIGPIPE, signal_cb);
    FILE *pfile = /*popen("./out", "r");/*/popen("raspivid -w 320 -h 240 -fps 10 -t 0 -o -", "r");
    if (pfile == NULL)
    {
        log_debug("popen exec error %s", strerror(errno));
        return -1;
    }

    char buf[102400];

    while(1)
    {
        int r = 0;
        memset(buf, 0, 102400);
        if((r = read(fileno(pfile), buf, 102399)) < 0)
        {
            log_debug("read file error %s",strerror(errno));
            pclose(pfile);
            break;
            //send(sockfd, sbuf, strlen(sbuf), 0);
        }
        else if(r > 0)
        {
            //printf("%s", buf);
            //sleep(10);
            log_debug("succ read %d bytes",r);
        }
        else
        {
            //log_debug("read end!");
            break;
        }
    }

    return 0;
}

