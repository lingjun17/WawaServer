#include "server.h"
#include "handler.h"

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

static ev_io listen_io;
static struct Channel channel[CHANNEL_MAX];
struct ev_loop *loop = EV_DEFAULT;
std::map<int, Channel*> channel_map;
std::map<PKG_CMD_TYPE, Handler> handler_map;
std::map<PKG_CMD_TYPE, Handler>::iterator handler_map_it;
std::map<int, Channel*> bridge_map;

int main(int argc, char *argv[])
{
    log_debug("server start...");

    inithandler();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGABRT, SIG_IGN);

    struct ev_signal sigint_watcher;
    struct ev_signal sigterm_watcher;
    ev_signal_init(&sigint_watcher, signal_cb, SIGINT);
    ev_signal_init(&sigterm_watcher, signal_cb, SIGTERM);
    ev_signal_start(EV_DEFAULT, &sigint_watcher);
    ev_signal_start(EV_DEFAULT, &sigterm_watcher);

    int listenfd;
    listenfd = create_and_bind(IP, PORT);
    if (listenfd < 0)
    {
        log_debug("bind error");
    }
    if (listen(listenfd, 1024) == -1)
    {
        log_debug("listen error");
    }

    setfastopen(listenfd);
    setnonblocking(listenfd);
    ev_io_init(&listen_io, accept_cb, listenfd, EV_READ);
    ev_io_start(loop, &listen_io);
    ev_run(loop, 0);

    return 0;
}

int inithandler()
{
    handler_map.insert(std::make_pair(PKG_REGISTER_CLIENT_REQ, handle_register_client_req));
    handler_map.insert(std::make_pair(PKG_CHOOSE_SERVER_REQ, handle_choose_server_req));
    return 0;
}


void accept_cb(EV_P_ ev_io *io, int revents)
{
    int serverfd = accept(io->fd, NULL, NULL);
    if (serverfd == -1) {
        log_debug("accept error");
        return;
    }

    sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    memset((void *)&client_addr,0,addr_size);
    getpeername(serverfd,(sockaddr *)&client_addr,&addr_size);

    log_debug("accept a connection from %s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);


    int opt = 1;
    setsockopt(serverfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    setsockopt(serverfd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
    setnonblocking(serverfd);
    //old version
    Channel *pChannel = get_free_channel();
    pChannel->fd = serverfd;
    channel_map.insert(std::make_pair(serverfd, pChannel));  // make map from fd to channel point
    ev_io_init(&pChannel->recvCtx.io, server_recv_cb, serverfd, EV_READ);
    ev_io_init(&pChannel->sendCtx.io, server_send_cb, serverfd, EV_WRITE);
    pChannel->recvCtx.pChannel = pChannel;
    pChannel->sendCtx.pChannel = pChannel;
    //ev_io_stop(EV_A_ &pChannel->sendCtx.io);
    ev_io_start(EV_A_ &pChannel->recvCtx.io); //server recv first
}

int handle(Pkg &pkg)
{
    handler_map_it = handler_map.find(pkg.stHead.cmd);
    if (handler_map_it != handler_map.end())
    {
        return handler_map_it->second(pkg);
    }
    else
    {
        log_debug("no hanlder found");
        return 0;
    }
}

void check_break_fd(int type, int fd)
{
    if (type == CTYPE_REMOTE)
    {
        std::map<int, Channel*>::iterator it = bridge_map.find(fd);
        if (it != bridge_map.end())
        {
            Pkg sendPkg;
            sendPkg.construct();
            sendPkg.stHead.fd = it->second->fd;
            sendPkg.stHead.cmd = PKG_RESTART_CLIENT_NTF;
            sendPkg.stHead.len = sizeof(sendPkg.stBody.stRestartClientNtf);
            sendPkg.stBody.stRestartClientNtf.Reserve = 1;
            response(sendPkg);
            log_debug("send stop pkg to raspi fd %d", sendPkg.stHead.fd);
            it->second->status = ACTIVE; //
            bridge_map.erase(fd);
        }
    }
}

void server_recv_cb(EV_P_ ev_io *io, int revents)
{
    RecvCtx *recvCtx = reinterpret_cast<RecvCtx*>(io);

    ssize_t r = recv(recvCtx->io.fd, (char*)&recvCtx->pkg + recvCtx->len,
                     recvCtx->pkg.stHead.len + sizeof(recvCtx->pkg.stHead) - recvCtx->len, 0);

    if (r == 0) {
        // connection closed
        log_debug("server_recv close the connection fd %d", recvCtx->io.fd);
        free_channel(recvCtx->pChannel);
        check_break_fd(recvCtx->pChannel->type, recvCtx->io.fd);

        return;
    } else if (r == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // continue to wait for recv
            ev_io_start(EV_A_ & recvCtx->io);
            log_debug("wait for next read");
            return;
        } else {
            log_debug("server recv errorcode %d %s", errno, strerror(errno));
            free_channel(recvCtx->pChannel);
            return;
        }
    }
    else //正常的收包逻辑
    {
        //解析头
        recvCtx->len += r;
        int pkglen = recvCtx->pkg.stHead.len + sizeof(recvCtx->pkg.stHead);
        if(recvCtx->len < sizeof(recvCtx->pkg.stHead)) {
            log_debug("head is not full recvd");
            return;
        }
        else if(recvCtx->pkg.stHead.len > BUF_SIZE) //包出错的情况
        {
            log_debug("bad packet bodylen %d", recvCtx->pkg.stHead.len);
            //关闭该socket
            free_channel(recvCtx->pChannel);
            return;
        }
        else if (recvCtx->len < pkglen)
        {
            //log_debug("body is not full recvd recvlen %d packagelen %d", recvCtx->len, pkglen);
            ev_io_start(EV_A_ & recvCtx->io);
            return;
        }

        //log_debug("full pkg received len %d cmd %d", recvCtx->len, recvCtx->pkg.stHead.cmd/*, recvCtx->pkg.stBody.stStreamDataNtf.buf*/);
        //如果收到的包id为0 表明需要直接响应 否则代表转发到对应的fd
        if (recvCtx->pkg.stHead.fd == 0)
        {
            recvCtx->pkg.stHead.fd = recvCtx->io.fd;
            handle(recvCtx->pkg);
        }
        else
        {
            int iRet = dispatch(recvCtx->pkg, recvCtx->io.fd, recvCtx->pkg.stHead.fd);
            if (iRet == -2) //对面已经关闭 stop树莓派
            {
                log_debug("dispatch msg failed iRet %d", iRet);
            }
        }

        recvCtx->len = 0;
        recvCtx->pkg.construct();
    }
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

int create_and_bind(const char *host, int port)
{
    int s, listen_sock;

    listen_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == -1) {
        log_debug("create socket error %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    setsockopt(listen_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
    int err = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    if (err != 0)
    {
        log_debug("reuse port error %s", strerror(errno));
        return errno;
    }
    else {
        log_debug("port reuse enabled");
    }

    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;
    if (host == NULL)
    {
        my_addr.sin_addr.s_addr = htons(INADDR_ANY);
    }
    else
    {
        my_addr.sin_addr.s_addr = inet_addr(host);
    }
    my_addr.sin_port = htons(port);

    s = bind(listen_sock, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));
    if (s == 0) {
        log_debug("bind %s : %d succ", host, port)
        return listen_sock;
    } else {
        log_debug("bind error");
        close(listen_sock);
    }

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

int setfastopen(int fd)
{
    int s = 0;
#ifdef TCP_FASTOPEN
    if (1) {
#ifdef __APPLE__
        int opt = 1;
#else
        int opt = 5;
#endif
        s = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt));

        if (s == -1) {
            if (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT) {
                log_debug("fast open is not supported on this platform");
            } else {
                log_debug("setsockopt");
            }
        }
    }
#endif
    return s;
}

Channel* get_free_channel()
{
    for (int i = 0; i < CHANNEL_MAX; ++i)
    {
        if (channel[i].status == NON_ACTIVE)
        {
            channel[i].status = ACTIVE;
            channel[i].idx = i;
            channel[i].recvCtx.pkg.construct();
            channel[i].recvCtx.len = 0;
            channel[i].sendCtx.pkg.construct();
            channel[i].sendCtx.len = 0;
            log_debug("get free channel %d", i);
            return channel+i;
        }
    }
    log_debug("no useable channel");
    return NULL;
}

int free_channel(Channel *c)
{
    if (c->idx >= CHANNEL_MAX || c->idx < 0)
    {
        log_debug("freechannel error channel num");
        return -1;
    }

    channel[c->idx].status = NON_ACTIVE;
    channel[c->idx].recvCtx.len = 0;
    channel[c->idx].sendCtx.len = 0;

    ev_io_stop(EV_A_ &(channel[c->idx].recvCtx.io));
    ev_io_stop(EV_A_ &(channel[c->idx].sendCtx.io));

    int fd = channel[c->idx].fd;
    channel_map.erase(fd);
    log_debug("channel_map erased key %d", fd);
    close(fd);
    log_debug("free channel %d", c->idx);
    return 0;
}

//callback 调用的时候 pkg已经赋值
void server_send_cb(EV_P_ ev_io *io, int revents)
{
    log_debug("server send cb");
    SendCtx *sendCtx = reinterpret_cast<SendCtx*>(io);
    Channel *pch = sendCtx->pChannel;

    int len = pch->sendCtx.pkg.stHead.len + sizeof(pch->sendCtx.pkg.stHead);
    log_debug("len %d sendlen %d", len, pch->sendCtx.len);
    int s = send(pch->fd, (char*)&pch->sendCtx.pkg + pch->sendCtx.len, len - pch->sendCtx.len ,0);
    if (s < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for send
            log_debug("wait data");
            ev_io_start(EV_A_ & pch->sendCtx.io);
        } else {
            log_debug("socket closed in send action fd %d", pch->fd);
            check_break_fd(pch->type, pch->fd);
            free_channel(pch);
        }
    }
    else
    {
        pch->sendCtx.len += s;
        log_debug("send cb already send %d bytes total len %d this time send %d",pch->sendCtx.len, len, s);
        if (pch->sendCtx.len < len)
        {
            ev_io_start(EV_A_ & pch->sendCtx.io);
        }
        else
        {
            log_debug("rest %d bytes send over", s);
            ev_io_stop(EV_A_ & pch->sendCtx.io);
            ev_io_start(EV_A_ & pch->recvCtx.io);
            //需要开启源头的接收io
            ev_io_start(EV_A_ & pch->sendCtx.pSrcChannel->recvCtx.io);
        }
    }
}
