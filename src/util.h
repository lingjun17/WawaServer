//
// Created by junling on 2017/7/13.
//

#ifndef LS_UTIL_H
#define LS_UTIL_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#ifdef __DEBUG
#define log_debug(format, ...) \
do{\
fprintf (stdout, format, ## __VA_ARGS__);\
fprintf (stdout, "\n"); \
}while(0);
//#define log_error(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#else
#define log_debug(format, ...)
#define log_error(format, ...)
#endif
/*

int send_pkg(int fd, Pkg &pkg)
{
    int pos = 0;
    while(pos < sizeof(Pkg))
    {
        int send_num = send(fd,(char*)&pkg+pos, sizeof(pkg), 0);
        if(send_num < 0)
        {
            printf("send error\n");
            return -1;
        }
        else if (send_num == 0)
        {
            printf("send over or client closed\n");
            return 0;
        }
        pos += send_num;
    }
    printf("send succ\n");
    return 0;
}

int send_stream(int fd, char *buf, int len)
{
    int pos = 0;
    while(pos < len)
    {
        int send_num = send(fd,buf+pos, len, 0);
        if(send_num < 0)
        {
            printf("send error\n");
            return -1;
        }
        else if (send_num == 0)
        {
            printf("send over or client closed\n");
            return 0;
        }
        pos += send_num;
    }
    printf("send succ\n");
    return 0;
}

Pkg* recv_pkg(int fd)
{
    Pkg *pkg = new Pkg();
    int pos = 0;
    while(pos < sizeof(Pkg))
    {
        int rec_num = recv(fd, (char*)pkg+pos, sizeof(pkg), 0);
        if(rec_num < 0)
        {
            printf("recv error\n");
            return NULL;
        }
        pos += rec_num;
    }
    printf("recv succ\n");
    return &pkg;
}

Pkg* recv_stream(int fd)
{
    char buf[1024];
    memset(buf, 0, 1024);
    int pos = 0;
    while(pos < sizeof(Pkg))
    {
        int rec_num = recv(fd, (char*)pkg+pos, sizeof(pkg), 0);
        if(rec_num < 0)
        {
            printf("recv error\n");
            return NULL;
        }
        pos += rec_num;
    }
    printf("recv succ\n");
    return &pkg;
}*/

#endif //LS_UTIL_H
//struct timeval tv;\
//gettimeofday(&tv, NULL);\
//tm *current_time = localtime(&tv.tv_sec);\
//char buf[1024];
//sprintf(buf, "[LOG_DEBUG][%04d%02d%02d %d:%d:%d.%d]",current_time->tm_year+1900,current_time->tm_mon+1, \
//current_time->tm_mday,current_time->tm_hour,current_time->tm_min,current_time->tm_sec,tv.tv_usec);\
//fprintf (stdout, "%s\n", buf);
