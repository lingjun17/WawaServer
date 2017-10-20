//
// Created by junling on 2017/8/29.
//

#ifndef WAWA_PROTOCOL_H
#define WAWA_PROTOCOL_H
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

#define BUF_SIZE 1400
#define CHANNEL_MAX 256
#define IP "122.152.213.73"
//"114.94.126.35"
//"10.105.208.201"
//"122.152.213.73"
//192.168.3.4"
//"122.152.213.73"
//"114.94.126.35"
//IP "192.168.3.4"
//"127.0.0.1"
//"192.168.3.4"
//"10.105.208.201"
#define PORT 8888


enum CLIENT_TYPE
{
    CTYPE_RASPI,
    CTYPE_REMOTE,
};

enum PKG_CMD_TYPE
{
    PKG_REGISTER_CLIENT_REQ = 1,
    PKG_REGISTER_CLIENT_RES = 2,
    PKG_STREAM_DATA_NTF = 3,
    PKG_CHOOSE_SERVER_REQ = 4,
    PKG_CHOOSE_SERVER_RES = 5,
    PKG_START_SEND_DATA_NTF = 6,
    PKG_RESTART_CLIENT_NTF = 7,
};

struct RestartClientNtf
{
    int Reserve;
};

struct RegisterClientReq
{
    int clientType;
};

struct RegisterClientRes
{
    int result;
    int fd[8];
};

struct StreamDataNtf
{
    char buf[BUF_SIZE];;
};


struct ChooseServerReq
{
    int choose_fd;
};

struct ChooseServerRes
{
    int result;
};

struct StartSendDataNtf
{
    int reserve;
};


struct Head
{
    PKG_CMD_TYPE cmd;
    unsigned int len;
    int fd; //fd = 0 代表发给wawasvr fd != 0 代表需要转发到对应的fd
};

union Body
{
    RegisterClientReq stRegisterClientReq;
    RegisterClientRes stRegisterClientRes;
    StreamDataNtf stStreamDataNtf;
    ChooseServerReq stChooseServerReq;
    ChooseServerRes stChooseServerRes;
    StartSendDataNtf stStartSendDataNtf;
    RestartClientNtf stRestartClientNtf;
};

struct Pkg
{
    Head stHead;
    Body stBody;
    void construct()
    {
        memset(&stHead, 0, sizeof(Head));
        memset(&stBody, 0, sizeof(Body));
    }
};


#endif //WAWA_PROTOCOL_H
