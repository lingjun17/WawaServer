//
// Created by junling on 2017/8/28.
//

#ifndef WAWA_SERVER_H
#define WAWA_SERVER_H

#include "ev.h"
#include "protocol.h"
#include <map>
#include <queue>


enum CHANNEL_STATUS
{
    NON_ACTIVE = 0,
    ACTIVE = 1,
};

struct Channel;


struct RecvCtx
{
    ev_io io;
    Pkg pkg;
    uint32_t len;
    Channel *pChannel;
};

struct SendCtx
{
    ev_io io;
    Pkg pkg;
    uint32_t len;
    Channel *pChannel;
};

struct Channel
{
    int fd;             //fd
    int status;         //1 active 0 unactive
    int idx;            //channel idx in array
    RecvCtx recvCtx;
    SendCtx sendCtx;
};

struct Bridge
{
    Channel *pRaspChannel; //raspi
    Channel *pRemoteChannel; //android
    int status;
    int idx;
};
/*
extern std::map<int, Tunnel*> tunnel_map; //fd->channel

class Tunnel
{
public:
    Tunnel()
    {
        pPkgQueue = new std::queue<Pkg>();
        memset(&raspiRecvCtx, 0, sizeof(raspiRecvCtx));
        memset(&raspiSendCtx, 0, sizeof(raspiSendCtx));
        memset(&remoteRecvCtx, 0, sizeof(remoteRecvCtx));
        memset(&remoteSendCtx, 0, sizeof(remoteSendCtx));
    }

    ~Tunnel()
    {
        delete(pPkgQueue);
    }



private:
    std::queue<Pkg> *pPkgQueue;
    RecvCtx raspiRecvCtx;
    SendCtx raspiSendCtx;
    RecvCtx remoteRecvCtx;
    SendCtx remoteSendCtx;
};
*/




int setnonblocking(int fd);
int create_and_bind(const char *host, int port);
void accept_cb(EV_P_ ev_io *w, int revents);
void signal_cb(EV_P_ ev_signal *w, int revents);
void accept_cb(EV_P_ ev_io *w, int revents);
void server_recv_cb(EV_P_ ev_io *w, int revents);
void server_send_cb(EV_P_ ev_io *w, int revents);
int setfastopen(int fd);
Channel* get_free_channel();
Bridge* get_free_bridge();
int free_channel(Channel *);
int free_bridge(int fd);
int inithandler();


typedef int(*Handler)(Pkg &pkg);
extern std::map<PKG_CMD_TYPE, Handler> handler_map;
extern std::map<PKG_CMD_TYPE, Handler>::iterator handler_map_it;

extern std::map<int, Channel*> channel_map; //fd->channel
extern struct ev_loop *loop;
extern std::map<int, Bridge*> bridge_map_raspikey;
extern std::map<int, Bridge*> bridge_map_remotekey;

#endif //WAWA_SERVER_H
