#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory>
#include "protocol.h"

#define __DEBUG
#include "util.h"


int send_pkg(int sockfd, Pkg &pkg)
{
    int byte_sent = 0;
    int len = pkg.stHead.len + sizeof(pkg.stHead);
    while(1)
    {
        int s = send(sockfd, (char*)(&pkg) + byte_sent, len - byte_sent, 0);
        if(s >= 0)
        {
            byte_sent += s;
            log_debug("send packet %d bytes len %d", s, len);
        }
        else
        {
            log_debug("%s", strerror(errno));
            return -1;
        }

        if(byte_sent == len)
        {
            log_debug("packet fully sent %d bytes sent", byte_sent);
            break;
        }
    }
    return 0;
}

int recv_pkg(int sockfd, Pkg &pkg)
{
    pkg.construct();

    int len = 0;
    int r = 0;
    int pkglen = sizeof(pkg.stHead);
    while(1)
    {
        r = recv(sockfd, (char*)(&pkg) + len, pkglen - len, 0);
        if (r > 0)
        {
            //log_debug("read head %d bytes read",r)
            len += r;
        }
        else
        {
            log_debug("recv head error");
            return -1;
        }

        if (len == pkglen)
        {
            break;
        }
    }

    pkglen += pkg.stHead.len;
    while(1)
    {
        r = recv(sockfd, (char*)(&pkg) + len, pkglen - len, 0);
        if (r > 0)
        {
            len += r;
            //log_debug("client %d bytes recvd total %d pkglen %d", r, len, pkglen);
        }
        else
        {
            printf("recv pkg failed\n");
            return -1;
        }

        if (len == pkglen)
        {
            //printf("recv pkg succ\n");
            break;
        }
    }

    return 0;
}

int main()
{
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	inet_pton(AF_INET, IP, &servaddr.sin_addr);

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	printf("connect server ret %d\n", ret);

    //封装数据包
    Pkg pkg;
    pkg.stHead.cmd = PKG_REGISTER_CLIENT_REQ;
    pkg.stHead.len = sizeof(pkg.stBody.stRegisterClientReq);
    pkg.stBody.stRegisterClientReq.clientType = CTYPE_RASPI;
    int iRet = send_pkg(sockfd, pkg);
    if (iRet != 0)
    {
        printf("send pkg error\n");
        return -1;
    }

	log_debug("sent shakehands req to server\n");

    Pkg recvpkg;
    iRet = recv_pkg(sockfd, recvpkg);
    if (iRet != 0)
    {
        printf("recv pkg error\n");
        return -1;
    }

    //log_debug("recv pkg cmd %d", recvpkg.stHead.cmd);
    log_debug("shakehands succ!");

    while(1)
    {
        //wait for send data
        recvpkg.construct();
        iRet = recv_pkg(sockfd, recvpkg);
        if (iRet != 0)
        {
            printf("recv pkg error\n");
            return -1;
        }

        if (recvpkg.stHead.cmd == PKG_START_SEND_DATA_NTF)
        {
            //start send data
            log_debug("start send data");
            char sbuf[BUF_SIZE];
            int pos = 0;
            int fd = 0;
            if((fd = open("/Users/junling/test.txt",O_RDONLY)) == -1) {
                printf("cannot open file/n");
                exit(1);
            }

            Pkg pkg;
            pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
            pkg.stHead.len = 0;
            memset(pkg.stBody.stStreamDataNtf.buf, 0, BUF_SIZE);
            //memcpy(pkg.stBody.stStreamDataNtf.buf, sbuf, r);

            while(1)
            {
                int r = 0;
                if((r = read(fd, pkg.stBody.stStreamDataNtf.buf, BUF_SIZE)) < 0)
                {
                    log_debug("read file error %s",strerror(errno));
                    break;
                    //send(sockfd, sbuf, strlen(sbuf), 0);
                }
                else if(r > 0)
                {
                    pkg.stHead.len = r;
                    int iRet = send_pkg(sockfd, pkg);
                    if (iRet != 0)
                    {
                        printf("send pkg error\n");
                        return -1;
                    }
                }
                else
                {
                    log_debug("read end!");
                    break;
                }

            }
        }

        recvpkg.construct();
        iRet = recv_pkg(sockfd, recvpkg);
        if (iRet != 0)
        {
            printf("recv pkg error\n");
            return -1;
        }

        if(recvpkg.stHead.cmd == PKG_REMOTE_TO_RASPI_NTF)
        {
            log_debug("continue next remote");
            continue;
        }
    }

	return 0;
}

