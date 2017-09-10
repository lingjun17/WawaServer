//
// Created by junling on 2017/9/1.
//
#include "handler.h"
#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>

#define __DEBUG
#include "util.h"

void get_peer_info(int fd)
{
    sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    memset((void *)&client_addr,0,addr_size);
    getpeername(fd,(sockaddr *)&client_addr,&addr_size);
    log_debug("recv packet from raspi %s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
}

int send_pkg_to_cellphone(Channel *pChannel)
{
    /*int serverfd = pChannel->fd;

    std::map<int, Channel*>::iterator channel_map_it = channel_map.find(reapifd); //patch
    if(channel_map_it == channel_map.end())
    {
        log_debug("send_pkg_to_client can not find channel key1 %d", pChannel->spkg.stHead.fd);
        return -1;
    }
    Channel *raspi_channel = channel_map_it->second;

    int len = pChannel->spkg.stHead.len + sizeof(pChannel->spkg.stHead);
    int s = send(serverfd, (char*)&pChannel->spkg, len ,0);
    log_debug("send pkg to client %d bytes sent pkglen %d", s, len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for
            log_debug("no data wait for send");
            ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
            ev_io_start(EV_A_ & pChannel->sendCtx.io);
        } else {
            free_channel(pChannel);
        }
    } else if (s < len) {
        pChannel->sendlen += s;
        log_debug("send pkg %d bytes sent", s);
        ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
        ev_io_start(EV_A_ & pChannel->sendCtx.io);
    }
    else
    {
        log_debug("send pkg succ in send_pkg_to_client");
        ev_io_stop(EV_A_ & pChannel->sendCtx.io);
        ev_io_start(EV_A_ & raspi_channel->recvCtx.io);
    }*/
    return 0;
}

//recv a msg from A then resp a msg back to A
int response_pkg(Pkg &pkg)
{
    std::map<int, Channel*>::iterator channel_map_it = channel_map.find(pkg.stHead.fd); //patch
    if(channel_map_it == channel_map.end())
    {
        log_debug("send_pkg_to_client can not find channel key %d", pkg.stHead.fd);
        return -1;
    }

    Channel *channel = channel_map_it->second;
    channel->sendCtx.len = 0;
    int len = pkg.stHead.len + sizeof(pkg.stHead);
    int s = send(pkg.stHead.fd, (char*)&pkg, len ,0);
    //log_debug("send pkg to client %d bytes sent pkglen %d", s, len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for
            //log_debug("no data wait for send");
            ev_io_stop(EV_A_ & channel->recvCtx.io);
            ev_io_start(EV_A_ & channel->sendCtx.io);
        } else {
            free_channel(channel);
        }
    } else if (s < len) {
        channel->sendCtx.len += s;
        //log_debug("send pkg %d bytes sent", s);
        ev_io_stop(EV_A_ & channel->recvCtx.io);
        ev_io_start(EV_A_ & channel->sendCtx.io);
    }
    else
    {
        log_debug("response_pkg send pkg succ in send_pkg_to_client fd %d", pkg.stHead.fd);
        ev_io_stop(EV_A_ & channel->sendCtx.io);
        ev_io_start(EV_A_ & channel->recvCtx.io);
    }
    return 0;
}


int dispatchMsgFromRaspiToRemote(Pkg &pkg)
{
    std::map<int, Bridge*>::iterator bridge_it = bridge_map_remotekey.find(pkg.stHead.fd);
    if (bridge_it == bridge_map_remotekey.end())
    {
        log_debug("dispatchMsgFromRaspiToRemote can not find bridge key %d", pkg.stHead.fd);
        return -1;
    }


    Channel *raspi_channel = bridge_it->second->pRaspChannel;
    Channel *remote_channel = bridge_it->second->pRemoteChannel;
    remote_channel->sendCtx.len = 0;
    int len = pkg.stHead.len + sizeof(pkg.stHead);
    int s = send(remote_channel->fd, (char*)&pkg, len ,0);
    //log_debug("dispatchMsgFromRaspiToRemote send pkg to remote %d bytes sent pkglen %d", s, len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for
            //log_debug("dispatchMsgFromRaspiToRemote no data wait for send");
            ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
            ev_io_stop(EV_A_ & remote_channel->recvCtx.io);
            ev_io_start(EV_A_ & remote_channel->sendCtx.io);
        } else {
            free_channel(remote_channel);
        }
    } else if (s < len) {
        remote_channel->sendCtx.len += s;
        //log_debug("dispatchMsgFromRaspiToRemote send pkg %d bytes sent", s);
        ev_io_stop(EV_A_ & remote_channel->recvCtx.io);
        ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
        ev_io_start(EV_A_ & remote_channel->sendCtx.io);
    }
    else
    {
        log_debug("dispatchMsgFromRaspiToRemote send pkg succ in send_pkg_to_client fd %d",pkg.stHead.fd);
        ev_io_stop(EV_A_ & remote_channel->sendCtx.io);
        ev_io_start(EV_A_ & raspi_channel->recvCtx.io);
    }
    return 0;
}


int dispatchMsgFromRemoteToRaspi(Pkg &pkg)
{
    std::map<int, Bridge*>::iterator bridge_it = bridge_map_raspikey.find(pkg.stHead.fd);
    if (bridge_it == bridge_map_raspikey.end())
    {
        log_debug("dispatchMsgFromRemoteToRaspi can not find bridge key %d", pkg.stHead.fd);
        return -1;
    }


    Channel *raspi_channel = bridge_it->second->pRaspChannel;
    Channel *remote_channel = bridge_it->second->pRemoteChannel;
    raspi_channel->sendCtx.len = 0;
    int len = pkg.stHead.len + sizeof(pkg.stHead);
    int s = send(raspi_channel->fd, (char*)&pkg, len ,0);
    log_debug("dispatchMsgFromRemoteToRaspi send pkg to remote %d bytes sent pkglen %d", s, len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for
            //log_debug("dispatchMsgFromRaspiToRemote no data wait for send");
            ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
            ev_io_stop(EV_A_ & remote_channel->recvCtx.io);
            ev_io_start(EV_A_ & raspi_channel->sendCtx.io);
        } else {
            free_channel(remote_channel);
        }
    } else if (s < len) {
        remote_channel->sendCtx.len += s;
        //log_debug("dispatchMsgFromRaspiToRemote send pkg %d bytes sent", s);
        ev_io_stop(EV_A_ & raspi_channel->recvCtx.io);
        ev_io_stop(EV_A_ & remote_channel->recvCtx.io);
        ev_io_start(EV_A_ & raspi_channel->sendCtx.io);
    }
    else
    {
        log_debug("dispatchMsgFromRemoteToRaspi send pkg succ in send_pkg_to_client fd %d",pkg.stHead.fd);
        ev_io_stop(EV_A_ & raspi_channel->sendCtx.io);
        ev_io_start(EV_A_ & remote_channel->recvCtx.io);
    }
    return 0;
}


int handle_register_client_req(Pkg &pkg)
{
    get_peer_info(pkg.stHead.fd);

    //根据fd找到对应的channel
    std::map<int, Channel*>::iterator channel_map_it = channel_map.find(pkg.stHead.fd);
    if(channel_map_it == channel_map.end())
    {
        log_debug("can not find channel key %d", pkg.stHead.fd);
        return -1;
    }

    if (pkg.stBody.stRegisterClientReq.clientType == CTYPE_RASPI)
    {
        Pkg &resPkg = channel_map_it->second->sendCtx.pkg;
        resPkg.stHead.cmd = PKG_REGISTER_CLIENT_RES;
        resPkg.stHead.fd = pkg.stHead.fd; //直接响应
        resPkg.stHead.len = sizeof(resPkg.stBody.stRegisterClientRes);
        resPkg.stBody.stRegisterClientRes.result = 0;
        response_pkg(resPkg);

        //add client to map
        Bridge *bridge = get_free_bridge();
        if (bridge == NULL)
        {
            log_debug("get free bridge error");
            return -1;
        }
        else
        {
            bridge->pRaspChannel = channel_map_it->second;
        }
        bridge_map_raspikey.insert(std::make_pair(pkg.stHead.fd, bridge));
        //reapifd = pkg.stHead.fd; //patch
    }
    else if(pkg.stBody.stRegisterClientReq.clientType == CTYPE_REMOTE)
    {
        Pkg &resPkg = channel_map_it->second->sendCtx.pkg;
        resPkg.stHead.cmd = PKG_REGISTER_CLIENT_RES;
        resPkg.stHead.fd = pkg.stHead.fd;
        resPkg.stHead.len = sizeof(resPkg.stBody.stRegisterClientRes);
        resPkg.stBody.stRegisterClientRes.result = 0;

        int i = 0;
        for (std::map<int, Bridge*>::iterator it = bridge_map_raspikey.begin(); it != bridge_map_raspikey.end(); ++it)
        {
            if(it->second->status == ACTIVE)
            {
                resPkg.stBody.stRegisterClientRes.fd[i] = it->first;
                i ++;
                if(i >= 8)
                {
                    log_debug("too many client");
                    return -1;
                }
            }
        }

        response_pkg(resPkg);
    }
    else
    {
        log_debug("wrong type %d", pkg.stBody.stRegisterClientReq.clientType);
        return -1;
    }

    return 0;
}

int handle_stream_data_ntf(Pkg &pkg)
{
    std::map<int, Bridge*>::iterator bridge_it = bridge_map_raspikey.find(pkg.stHead.fd);
    if (bridge_it == bridge_map_raspikey.end())
    {
        log_debug("dispatchMsgFromRemoteToRaspi can not find bridge key %d", pkg.stHead.fd);
        return -1;
    }

    Channel *raspi_channel = bridge_it->second->pRaspChannel;
    Channel *remote_channel = bridge_it->second->pRemoteChannel;

    if (raspi_channel == NULL || remote_channel == NULL)
    {
        log_debug("bridge data error");
        return -1;
    }

    Pkg &resPkg = remote_channel->sendCtx.pkg;
    memcpy(&resPkg, &pkg, sizeof(pkg.stHead) + pkg.stHead.len);
    resPkg.stHead.fd = remote_channel->fd; //修改fd 以发送到对端
    //log_debug("send pkg to remote cmd %d len %lu channel fd %d", resPkg.stHead.cmd, sizeof(resPkg.stHead) + resPkg.stHead.len, remote_channel->fd);
    dispatchMsgFromRaspiToRemote(resPkg);
    return 0;
}

int handle_remote_to_raspi_req(Pkg &pkg)
{
    std::map<int, Bridge*>::iterator bridge_it = bridge_map_remotekey.find(pkg.stHead.fd);
    if (bridge_it == bridge_map_remotekey.end())
    {
        log_debug("dispatchMsgFromRaspiToRemote can not find bridge key %d", pkg.stHead.fd);
        return -1;
    }

    Channel *raspi_channel = bridge_it->second->pRaspChannel;
    Channel *remote_channel = bridge_it->second->pRemoteChannel;

    if (raspi_channel == NULL || remote_channel == NULL)
    {
        log_debug("bridge data error");
        return -1;
    }

    Pkg &resPkg = raspi_channel->sendCtx.pkg;
    memcpy(&resPkg, &pkg, sizeof(pkg.stHead) + pkg.stHead.len);
    resPkg.stHead.fd = raspi_channel->fd; //修改fd 以发送到对端
    log_debug("send pkg to raspi cmd %d len %lu channel fd %d", resPkg.stHead.cmd, sizeof(resPkg.stHead) + resPkg.stHead.len, raspi_channel->fd);
    dispatchMsgFromRemoteToRaspi(resPkg);
    return 0;
}

int handle_choose_server_req(Pkg &pkg)
{
    int remotefd = pkg.stHead.fd;
    int raspifd = pkg.stBody.stChooseServerReq.choose_fd;
    std::map<int, Bridge*>::iterator it = bridge_map_raspikey.find(raspifd);
    if (it == bridge_map_raspikey.end())
    {
        log_debug("can not find bridge clientfd %d remotefd %d", raspifd, remotefd);
        return -1;
    }
    std::map<int, Channel*>::iterator remote_channel_it = channel_map.find(remotefd);
    if (remote_channel_it == channel_map.end())
    {
        log_debug("can not find remote channel remotefd %d", remotefd);
        return -1;
    }
    it->second->pRemoteChannel = remote_channel_it->second;
    bridge_map_remotekey.insert(std::make_pair(remotefd, it->second));

    log_debug("bridge from clientfd %d to remotefd %d estimated", raspifd, remotefd);

    Pkg &resPkg = it->second->pRemoteChannel->sendCtx.pkg;
    resPkg.stHead.cmd = PKG_CHOOSE_SERVER_RES;
    resPkg.stHead.fd = remotefd; // mast assign
    resPkg.stHead.len = sizeof(resPkg.stBody.stChooseServerRes);

    //告诉remote可以recv数据
    std::map<int, Channel*>::iterator channel_map_it_rasp = channel_map.find(raspifd);
    if(channel_map_it_rasp == channel_map.end())
    {
        log_debug("can not find client channel key %d", raspifd);
        resPkg.stBody.stChooseServerRes.result = -1;
        response_pkg(resPkg);
        return -1;
    }
    else
    {
        resPkg.stBody.stChooseServerRes.result = 0;
        response_pkg(resPkg);
    }

    Pkg &clientPkg = channel_map_it_rasp->second->sendCtx.pkg;
    clientPkg.stHead.cmd = PKG_START_SEND_DATA_NTF;
    clientPkg.stHead.len = sizeof(clientPkg.stBody.stStartSendDataNtf);
    clientPkg.stBody.stStartSendDataNtf.reserve = 0;
    response_pkg(clientPkg);

    return 0;
}