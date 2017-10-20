//
// Created by junling on 2017/9/25.
//

#include "raspi.h"
#include "protocol.h"
#include <cstdio>
#include <cstring>
#include <ev.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string>

#define __DEBUG
#include "util.h"

static ev_io raspi_send_io;
static ev_io raspi_recv_io;
struct ev_loop *loop = EV_DEFAULT;
static Pkg send_pkg;
static int send_len;
static Pkg recv_pkg;
static int recv_len;
static int sockfd;
std::map<PKG_CMD_TYPE, Handler> handler_map;
std::map<PKG_CMD_TYPE, Handler>::iterator handler_map_it;

int handle_register_client_res(Pkg &pkg)
{
    log_debug("shakehands succ!");
    return 0;
}

int handle_restart_client_ntf(Pkg &pkg)
{
    log_debug("recv handle_restart_client_ntf!");

    return 0;
}

int handle_start_send_data_ntf(Pkg &pkg)
{
    send_pkg.construct();
    int r = 0;
    if((r = read(STDIN_FILENO, send_pkg.stBody.stStreamDataNtf.buf, BUF_SIZE)) < 0)
    {
        log_debug("read file error %s",strerror(errno));
    }
    else if(r > 0)
    {
        pkg.stHead.cmd = PKG_STREAM_DATA_NTF;
        pkg.stHead.len = r;
        //printf("hello test %s r %d\n", pkg.stBody.stStreamDataNtf.buf, r);
        int iRet = send_pkg_2_server();
        if (iRet != 0)
        {
            printf("send pkg error\n");
        }
        else if(iRet == 1)
        {
            printf("send pkg uncomplete\n");
        }
        return iRet;
    }
    else
    {
        pkg.construct();
        log_debug("read end!");
    }
    return 0;
}

int inithandler()
{
    handler_map.insert(std::make_pair(PKG_REGISTER_CLIENT_RES, handle_register_client_res));
    handler_map.insert(std::make_pair(PKG_RESTART_CLIENT_NTF, handle_restart_client_ntf));
    handler_map.insert(std::make_pair(PKG_START_SEND_DATA_NTF, handle_start_send_data_ntf));

    return 0;
}

int init()
{
    send_pkg.construct();
    send_len = 0;
    recv_pkg.construct();
    recv_len = 0;
    sockfd = 0;
    inithandler();
    return 0;
}


int handle_pkg(Pkg &pkg)
{
    handler_map_it = handler_map.find(pkg.stHead.cmd);
    if (handler_map_it != handler_map.end())
    {
        //log_debug("handle cmd %d", pkg.stHead.cmd);
        return handler_map_it->second(pkg);
    }
    log_debug("no hanlder found");
    return -1;
}

int setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void signal_cb(EV_P_ ev_signal *w, int revents)
{
    if (revents & EV_SIGNAL) {
        switch (w->signum) {
            case SIGINT:
            case SIGTERM:
                ev_unloop(EV_A_ EVUNLOOP_ALL);
        }
        printf("server exit!\n");
    }
}

int connect2server()
{
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

void recv_cb(EV_P_ ev_io *io, int revents)
{
    ssize_t r = recv(io->fd, (char*)&recv_pkg + recv_len,
                     recv_pkg.stHead.len + sizeof(recv_pkg.stHead) - recv_len, 0);
    if (r == 0) {
        // connection closed
        log_debug("server_recv close the connection");
        ev_io_stop(EV_A_ io);
        close(io->fd);
        return;
    } else if (r == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data
            // continue to wait for recv
            ev_io_stop(EV_A_ & raspi_send_io);
            ev_io_start(EV_A_ io);
            log_debug("wait for next read");
            return;
        } else {
            log_debug("server recv errorcode %d %s", errno, strerror(errno));
            ev_io_stop(EV_A_ io);
            ev_io_stop(EV_A_ & raspi_send_io);
            close(io->fd);
            return;
        }
    }
    else //正常的收包逻辑
    {
        //解析头
        recv_len += r;
        int pkglen = recv_pkg.stHead.len + sizeof(recv_pkg.stHead);
        //recv packet 1412 bytes this time 1412 bytes in total cmd 7 len 1296
        //log_debug("recv packet %ld bytes this time %d bytes in total cmd %d len %d", r, recvCtx->len, recvCtx->pkg.stHead.cmd, recvCtx->pkg.stHead.len);
        if(recv_len < sizeof(recv_pkg.stHead)) {
            log_debug("head is not full recvd");
            return;
        }
        else if(recv_pkg.stHead.len > BUF_SIZE) //包出错的情况
        {
            log_debug("bad packet bodylen %d", recv_pkg.stHead.len);
            //关闭该socket
            ev_io_stop(EV_A_ io);
            ev_io_stop(EV_A_ & raspi_send_io);
            close(io->fd);
            return;
        }
        else if (recv_len < pkglen)
        {
            log_debug("body is not full recvd recvlen %d packagelen %d", recv_len, pkglen);
            ev_io_stop(EV_A_ & raspi_send_io);
            ev_io_start(EV_A_ io);
            return;
        }

        //log_debug("full pkg received len %d cmd %d", recv_len, recv_pkg.stHead.cmd/*, recvCtx->pkg.stBody.stStreamDataNtf.buf*/);

        handle_pkg(recv_pkg);

        recv_len = 0;
        recv_pkg.construct();
        ev_io_stop(EV_A_ io);
    }
}

void send_cb(EV_P_ ev_io *io, int revents)
{
    int len = send_len + sizeof(send_pkg.stHead);
    //log_debug("len %d sendlen %d", len, pch->sendCtx.len);
    int s = send(io->fd, (char*)&send_pkg + send_len, len - send_len ,0);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for send
            log_debug("wait data");
            ev_io_stop(EV_A_ & raspi_recv_io);
            ev_io_start(EV_A_ io);
        } else {
            ev_io_stop(EV_A_ io);
            close(io->fd);
        }
    }
    else if (send_len < len)
    {
        //log_debug("send cb %d bytes send len %d",pch->sendCtx.len, len);
        send_len += s;
        ev_io_stop(EV_A_ & raspi_recv_io);
        ev_io_start(EV_A_ io);
    }
    else
    {
        log_debug("rest %d bytes send over", s);
        ev_io_stop(EV_A_ io);
        ev_io_start(EV_A_ & raspi_recv_io);
    }
}

int send_pkg_2_server()
{
    int s = send(sockfd, (char*)&send_pkg, sizeof(send_pkg.stHead) + send_pkg.stHead.len, 0);
    log_debug("send_pkg_2_server send pkg to remote %d bytes sent pkglen %lu", s, sizeof(send_pkg.stHead) + send_pkg.stHead.len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait fo
            ev_io_stop(EV_A_ & raspi_recv_io);
            ev_io_start(EV_A_ & raspi_send_io);
        } else {
            ev_io_stop(EV_A_ &raspi_send_io);
            close(raspi_send_io.fd);
            return -1;
        }
    } else if (s < send_len) {
        send_len += s;
        log_debug("send_pkg_2_server send pkg %d bytes sent", s);
        ev_io_stop(EV_A_ & raspi_recv_io);
        ev_io_start(EV_A_ & raspi_send_io);
        return 1;
    }
    else
    {
        log_debug("dispatchMsgFromRemoteToRaspi send pkg succ in send_pkg_to_client fd %d", sockfd);
        ev_io_start(EV_A_ & raspi_send_io);
        ev_io_start(EV_A_ & raspi_recv_io);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    log_debug("server start...");

    init();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGABRT, SIG_IGN);

    struct ev_signal sigint_watcher;
    struct ev_signal sigterm_watcher;
    ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
    ev_signal_init(&sigterm_watcher, signal_cb, SIGTERM);
    ev_signal_start(EV_DEFAULT, &sigint_watcher);
    ev_signal_start(EV_DEFAULT, &sigterm_watcher);

    sockfd = connect2server();
    if (sockfd < 0)
    {
        log_debug("connect error");
        return -1;
    }

    setnonblocking(sockfd);
    ev_io_init(&raspi_recv_io, recv_cb, sockfd, EV_READ);
    ev_io_init(&raspi_send_io, send_cb, sockfd, EV_WRITE);

    //send shakehands to server
    send_pkg.construct();
    send_pkg.stHead.cmd = PKG_REGISTER_CLIENT_REQ;
    send_pkg.stBody.stRegisterClientReq.clientType = CTYPE_RASPI;
    send_pkg_2_server();
    ev_io_start(loop, &raspi_send_io);
    ev_io_start(loop, &raspi_recv_io);
    ev_run(loop, 0);

    return 0;
}
