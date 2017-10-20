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

//recv a msg from A then resp a msg back to A
int response(Pkg &pkg)
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
            //ev_io_stop(EV_A_ & channel->recvCtx.io);
            ev_io_start(EV_A_ & channel->sendCtx.io);
        } else {
            free_channel(channel);
        }
    } else if (s < len) {
        channel->sendCtx.len += s;
        //log_debug("send pkg %d bytes sent", s);
        //ev_io_stop(EV_A_ & channel->recvCtx.io);
        ev_io_start(EV_A_ & channel->sendCtx.io);
    }
    else
    {
        log_debug("response_pkg send pkg succ in send_pkg_to_client fd %d", pkg.stHead.fd);
        //ev_io_stop(EV_A_ & channel->sendCtx.io);
        ev_io_start(EV_A_ & channel->recvCtx.io);
    }
    return 0;
}

int dispatch(Pkg &pkg, int srcfd, int dstfd)
{
    std::map<int, Channel*>::iterator src_it = channel_map.find(srcfd);
    if (src_it == channel_map.end())
    {
        log_debug("look for channel srcfd %d error", srcfd);
        return -1;
    }
    std::map<int, Channel*>::iterator dst_it = channel_map.find(dstfd);
    if (dst_it == channel_map.end())
    {
        log_debug("look for channel dstfd %d error", dstfd);
        return -2;
    }

    if(pkg.stHead.len > BUF_SIZE)
    {
        log_debug("dispatch pkg data error pkg.stHead.len %d > BUF_SIZE", pkg.stHead.len);
        return -3;
    }

    memcpy(&dst_it->second->sendCtx.pkg, &pkg, pkg.stHead.len + sizeof(pkg.stHead));
    dst_it->second->sendCtx.len = 0;

    int len = pkg.stHead.len + sizeof(pkg.stHead);
    int s = send(dstfd, (char*)&pkg, len ,0);
    //log_debug("dispatchMsgFromRaspiToRemote send pkg to remote %d bytes sent pkglen %d", s, len);
    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for
            //log_debug("dispatchMsgFromRaspiToRemote no data wait for send");
            //ev_io_stop(EV_A_ & dst_it->recvCtx.io);
            ev_io_stop(EV_A_ & src_it->second->recvCtx.io); //没发送完的情况下 禁止src端继续接收数据
            ev_io_start(EV_A_ & dst_it->second->sendCtx.io);
        } else {
            free_channel(dst_it->second);
        }
    } else if (s < len) {
        dst_it->second->sendCtx.len += s;
        //log_debug("dispatchMsgFromRaspiToRemote send pkg %d bytes sent", s);
        //ev_io_stop(EV_A_ & remote_channel->recvCtx.io);
        ev_io_stop(EV_A_ & src_it->second->recvCtx.io);
        ev_io_start(EV_A_ & dst_it->second->sendCtx.io);
    }
    else
    {
        //log_debug("dispatchMsgFromRaspiToRemote send pkg succ in send_pkg_to_client fd %d",pkg.stHead.fd);
        ev_io_stop(EV_A_ & dst_it->second->sendCtx.io);
        ev_io_start(EV_A_ & src_it->second->recvCtx.io);
        //ev_io_start(EV_A_ & remote_channel->recvCtx.io);
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
        channel_map_it->second->type = CTYPE_RASPI;
        Pkg &resPkg = channel_map_it->second->sendCtx.pkg;
        resPkg.stHead.cmd = PKG_REGISTER_CLIENT_RES;
        resPkg.stHead.fd = pkg.stHead.fd; //直接响应
        resPkg.stHead.len = sizeof(resPkg.stBody.stRegisterClientRes);
        resPkg.stBody.stRegisterClientRes.result = 0;
        response(resPkg);
    }
    else if(pkg.stBody.stRegisterClientReq.clientType == CTYPE_REMOTE)
    {
        channel_map_it->second->type = CTYPE_REMOTE;
        Pkg &resPkg = channel_map_it->second->sendCtx.pkg;
        resPkg.stHead.cmd = PKG_REGISTER_CLIENT_RES;
        resPkg.stHead.fd = pkg.stHead.fd;
        resPkg.stHead.len = sizeof(resPkg.stBody.stRegisterClientRes);
        resPkg.stBody.stRegisterClientRes.result = 0;

        int i = 0;
        for (std::map<int, Channel*>::iterator it = channel_map.begin(); it != channel_map.end(); ++it)
        {
            if(it->second->status == ACTIVE && it->second->type == CTYPE_RASPI)
            {
                resPkg.stBody.stRegisterClientRes.fd[i] = it->second->fd;
                i ++;
                if(i >= 8)
                {
                    log_debug("too many raspi");
                    return -1;
                }
            }
        }

        response(resPkg);
    }
    else
    {
        log_debug("wrong type %d", pkg.stBody.stRegisterClientReq.clientType);
        return -1;
    }

    return 0;
}

/*int handle_remote_to_raspi_req(Pkg &pkg)
{
    std::map<int, Bridge*>::iterator bridge_it = bridge_map_remotekey.find(pkg.stHead.fd);
    if (bridge_it == bridge_map_remotekey.end())
    {
        //log_debug("dispatchMsgFromRaspiToRemote can not find bridge key %d", pkg.stHead.fd);
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
    resPkg.stHead.fd = raspi_channel->fd; //修改fd 以发送到对端
    resPkg.stHead.cmd = PKG_RESTART_CLIENT_NTF;
    log_debug("send pkg to raspi cmd %d len %lu channel fd %d", resPkg.stHead.cmd, sizeof(resPkg.stHead) + resPkg.stHead.len, raspi_channel->fd);
    response_pkg(resPkg);

    //clear buffer
    int r = 0;
    char buf[1024];
    while(1)
    {
        r = recv(raspi_channel->fd, buf, 1024, 0);
        if(r > 0)
        {
            log_debug("clear socket buf fd %d size %d", raspi_channel->fd, r);
        }
        else if(r == 0)
        {
            log_debug("socket is cleared");
            break;
        } else
        {
            log_debug("clear socket buf error %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}
*/
int handle_choose_server_req(Pkg &pkg)
{
    int remotefd = pkg.stHead.fd;
    int raspifd = pkg.stBody.stChooseServerReq.choose_fd;

    std::map<int, Channel*>::iterator remote_channel_it = channel_map.find(remotefd);
    if (remote_channel_it == channel_map.end())
    {
        log_debug("can not find remote channel remotefd %d", remotefd);
        return -1;
    }

    std::map<int, Channel*>::iterator raspi_channel_it = channel_map.find(raspifd);
    if (raspi_channel_it == channel_map.end())
    {
        log_debug("can not find raspi channel raspifd %d", raspifd);
        return -1;
    }

    raspi_channel_it->second->status = BUSY;

    //告诉remote可以recv数据
    Pkg &resPkg = remote_channel_it->second->sendCtx.pkg;
    resPkg.stHead.cmd = PKG_CHOOSE_SERVER_RES;
    resPkg.stHead.fd = remotefd; // mast assign
    resPkg.stHead.len = sizeof(resPkg.stBody.stChooseServerRes);
    resPkg.stBody.stChooseServerRes.result = 0;
    response(resPkg);


    Pkg &clientPkg = raspi_channel_it->second->sendCtx.pkg;
    clientPkg.stHead.cmd = PKG_START_SEND_DATA_NTF;
    clientPkg.stHead.len = sizeof(clientPkg.stBody.stStartSendDataNtf);
    clientPkg.stBody.stStartSendDataNtf.reserve = remotefd;
    response(clientPkg);

    return 0;
}