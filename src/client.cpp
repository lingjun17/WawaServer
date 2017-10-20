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
            //log_debug("send packet %d bytes len %d", s, len);
        }
        else
        {
            log_debug("in send_pkg %s", strerror(errno));
            return -1;
        }

        if(byte_sent == len)
        {
            log_debug("packet fully sent %d bytes sent cmd %d len %d", byte_sent, pkg.stHead.cmd, pkg.stHead.len);
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

static int sockfd = 0;
static int camera_pid = 0;
static int child_pid = 0;

int connect2server()
{
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &servaddr.sin_addr);
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret != 0)
    {
        return -1;
    }
    return sockfd;
}

//父进程结束的回调
void atexit_func()
{
    log_debug("kill camera pid %d father pid %d", camera_pid, child_pid);
    kill(camera_pid, 9);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int sockfd = connect2server();
	printf("connect server sockfd %d\n", sockfd);

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
    if (iRet != 0 || recvpkg.stHead.cmd != PKG_REGISTER_CLIENT_RES)
    {
        printf("recv pkg error\n");
        return -1;
    }

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

        if (recvpkg.stHead.cmd == PKG_RESTART_CLIENT_NTF)
        {
            log_debug("kill child pid %d father pid %d", child_pid, getpid());
            kill(child_pid, SIGHUP);
            recvpkg.construct();
            iRet = recv_pkg(sockfd, recvpkg);
            if (iRet != 0)
            {
                printf("recv pkg error\n");
                return -1;
            }
        }
        else if (recvpkg.stHead.cmd == PKG_START_SEND_DATA_NTF)
        {
            log_debug("recv send data ntf");
        }
        else
        {
            continue;
        }

        child_pid = fork();
        if (child_pid < 0)
        {
            log_debug("fork error!");
            return -2;
        }
        else if (child_pid == 0) //子进程
        {
            if (recvpkg.stHead.cmd == PKG_START_SEND_DATA_NTF) {
                int pipefd[2];
                pipe(pipefd);
                camera_pid = fork();
                if (camera_pid < 0) {
                    log_debug("fork error 2");
                    return -1;
                } else if (camera_pid == 0) {
                    //prctl(PR_SET_PDEATHSIG, SIGHUP);
                    //signal(SIGHUP, signal_cb);
                    //signal(SIGCHLD,SIG_IGN);
                    close(pipefd[0]);
                    int iret = dup2(pipefd[1], STDOUT_FILENO);
                    if (iret < 0) {
                        log_debug("dup error");
                        return -1;
                    }
                    /*execlp("raspivid", "raspivid", "-vf", "-hf", "-w",
                           "640", "-h", "480", "-fps", "10",
                           "-t", "0", "-o", "-", NULL);*/
                    execlp("./out", "./out", NULL);

                } else {
                    atexit(atexit_func);
                    close(pipefd[1]);
                    Pkg pkg;
                    pkg.construct();
                    while (1) {
                        //FILE *fp = fopen("./video.h264", "w");
                        int r = 0;
                        if ((r = read(pipefd[0], pkg.stBody.stStreamDataNtf.buf, BUF_SIZE)) < 0) {
                            log_debug("read file error %s", strerror(errno));
                            break;
                        } else if (r > 0) {
                            pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
                            pkg.stHead.len = r;
                            //printf("hello test %s r %d\n", pkg.stBody.stStreamDataNtf.buf, r);
                            int iRet = send_pkg(sockfd, pkg);
                            if (iRet != 0) {
                                printf("send pkg error\n");
                                //pclose(pfile);
                                exit(-1);
                            } else {

                                //write(STDIN_FILENO, pkg.stBody.stStreamDataNtf.buf, r);
                                //printf("send pkg succ!!!! %s %d\n",__FILE__, __LINE__);
                            }
                        } else {
                            pkg.construct();
                            log_debug("read end!");
                            break;
                        }
                    }
                }
                /*FILE *pfile = popen("raspivid -vf -hf -w 640 -h 480 -fps 20 -t 0 -o -", "r");
                if (pfile == NULL)
                {
                    log_debug("popen exec error %s", strerror(errno));
                    return -1;
                }
                int filefd = fileno(pfile);
                */


            }
        }

        log_debug("wait for new client");
    }

	return 0;
}

