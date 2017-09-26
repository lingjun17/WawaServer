#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <memory>
#include "protocol.h"
#include <csignal>
#define __DEBUG
#include "util.h"


static int sockfd = 0;
static int filefd = 0;


int send_pkg(int sockfd, Pkg &pkg)
{
	int byte_sent = 0;
	int len = pkg.stHead.len + sizeof(pkg.stHead);
	while(1)
	{
		int s = send(sockfd, (char*)(&pkg) + byte_sent, len, 0);
		if(s >= 0)
		{
			byte_sent += s;
			log_debug("send packet %d", len);
		}
		else
		{
			log_debug("%s", strerror(errno));
			return -1;
		}

		if(byte_sent == len)
		{
			log_debug("packet fully sent");
			break;
		}
	}

	log_debug("SEND PKG TO SERVER CMD %d", pkg.stHead.cmd);
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
			log_debug("read head %d bytes read",r)
			len += r;
		}
		else
		{
			log_debug("recv head error %s", strerror(errno));
			return -1;
		}

		if (len == pkglen)
		{
			break;
		}
	}

	pkglen += pkg.stHead.len;
	if (pkglen > BUF_SIZE + sizeof(pkg.stHead))
	{
		log_debug("bad packet bodylen %d", pkg.stHead.len);
		return -2;
	}
	while(1)
	{
		r = recv(sockfd, (char*)(&pkg) + len, pkglen - len, 0);
		if (r > 0)
		{
			len += r;
			log_debug("client %d bytes recvd total %d pkglen %d", r, len, pkglen);
		}
		else
		{
			printf("recv pkg failed %s\n", strerror(errno));
			return -1;
		}

		if (len == pkglen)
		{
			printf("recv pkg succ\n");
			break;
		}
	}

	return 0;
}

void signal_cb(int sig)
{
	Pkg sendPkg;
	sendPkg.stHead.cmd = PKG_REMOTE_TO_RASPI_NTF;
	sendPkg.stHead.len = sizeof(sendPkg.stBody.stDataFromRemoteToRaspiNtf);
	sendPkg.stBody.stDataFromRemoteToRaspiNtf.op = 1;
	int iRet = send_pkg(sockfd, sendPkg);
	log_debug("exit!!!!! iRet %d", iRet);
	close(filefd);
	close(sockfd);
	exit(0);
}

int main()
{
	signal(SIGINT, signal_cb);

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	//"122.152.213.73"
	inet_pton(AF_INET, IP, &servaddr.sin_addr);

	int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	printf("connect server ret %d\n", ret);

	//封装数据包
	Pkg *pkg = new Pkg();
	pkg->stHead.cmd = PKG_REGISTER_CLIENT_REQ;
	pkg->stHead.len = sizeof(pkg->stBody.stRegisterClientReq);
	pkg->stBody.stRegisterClientReq.clientType = CTYPE_REMOTE;
	int iRet = send_pkg(sockfd, *pkg);
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

	log_debug("recv pkg cmd %d", recvpkg.stHead.cmd);

	if (recvpkg.stHead.cmd == PKG_REGISTER_CLIENT_RES)
	{
		log_debug("recv pkg succ!");
        int i = 0;
		while(recvpkg.stBody.stRegisterClientRes.fd[i] != 0)
        {
            log_debug("registered raspi fd %d", recvpkg.stBody.stRegisterClientRes.fd[i]);
            i++;
        }
	}

	int choise;
	printf("enter your choise:\n");
	scanf("%d", &choise);

	Pkg sendPkg;
	sendPkg.stHead.cmd = PKG_CHOOSE_SERVER_REQ;
	sendPkg.stHead.len = sizeof(sendPkg.stBody.stChooseServerReq);
	sendPkg.stBody.stChooseServerReq.choose_fd = choise;
	iRet = send_pkg(sockfd, sendPkg);
	if (iRet != 0)
	{
		printf("send pkg error\n");
		close(sockfd);
		return -1;
	}

	iRet = recv_pkg(sockfd, recvpkg);
	if (iRet != 0)
	{
		printf("recv pkg error\n");
		close(sockfd);
		return -1;
	}


	if (recvpkg.stBody.stChooseServerRes.result != 0)
	{
		log_debug("recv pkg cmd %d, build bridge failed! wait for package...", recvpkg.stHead.cmd);
		close(sockfd);
		return -1;
	}
	else
	{
		log_debug("recv pkg cmd %d, build bridge succ! wait for package...", recvpkg.stHead.cmd);
	}

    //获取文件指针
    filefd = open("t.h264", O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	//告诉系统我们文件写完了数据更新，但是我们要要重新打开才能在写
    //数据刷新 数据立即更新
	int totalbyte = 0;
	while(1)
	{
		iRet = recv_pkg(sockfd, recvpkg);
		if (iRet != 0)
		{
			printf("recv pkg error\n");
			close(sockfd);
			return -1;
		}
		else
		{
			printf("recv package cmd %d from raspi %d bytes recvd total %d\n", recvpkg.stHead.cmd, recvpkg.stHead.len, totalbyte);
			//log_debug("recvd : %s", recvpkg.stBody.stStreamDataNtf.buf);
			int w = write (filefd, recvpkg.stBody.stStreamDataNtf.buf, recvpkg.stHead.len);
			if (w != recvpkg.stHead.len)
			{
				log_debug("write error %d len %d errno %s",w, recvpkg.stHead.len, strerror(errno));
			}
            totalbyte += recvpkg.stHead.len;
		}
	}
    close(filefd);

    close(sockfd);
	return 0;
}

