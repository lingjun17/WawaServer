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
    PKG_REGISTER_SERVER_REQ = 1,
    PKG_REGISTER_SERVER_RES = 2,
    PKG_REGISTER_CLIENT_REQ = 3,
    PKG_REGISTER_CLIENT_RES = 4,
    PKG_STREAM_DATA_REQ = 5,
    PKG_STREAM_DATA_RES = 6,
    PKG_STREAM_DATA_NTF = 7,
    PKG_CHOOSE_SERVER_REQ = 8,
    PKG_CHOOSE_SERVER_RES = 9,
    PKG_START_SEND_DATA_NTF = 10,
    PKG_REMOTE_TO_RASPI_NTF = 11,
    PKG_RESTART_CLIENT_NTF = 12,
};

struct RestartClientNtf
{
    int Reserve;
};

struct RegisterServerReq
{
    int clientType;
};

struct RegisterServerRes
{
    int result;
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

struct StreamDataReq
{
    char buf[BUF_SIZE];
};

struct StreamDataRes
{
    int result;
};

struct StreamDataNtf
{
    char buf[BUF_SIZE];;
};

struct DataFromRemoteToRaspiNtf
{
    int op; //操作码
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
    int fd; //find Channel by fd
};

union Body
{
    RegisterServerReq stRegisterServerReq;
    RegisterServerRes stRegisterServerRes;
    RegisterClientReq stRegisterClientReq;
    RegisterClientRes stRegisterClientRes;
    StreamDataReq stStreamDataReq;
    StreamDataRes stStreamDataRes;
    StreamDataNtf stStreamDataNtf;
    ChooseServerReq stChooseServerReq;
    ChooseServerRes stChooseServerRes;
    StartSendDataNtf stStartSendDataNtf;
    DataFromRemoteToRaspiNtf stDataFromRemoteToRaspiNtf;
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
