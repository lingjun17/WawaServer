//
// Created by junling on 2017/9/25.
//

#ifndef WAWA_RASPI_H
#define WAWA_RASPI_H

extern "C"
{
#include "camkit/config.h"
#include "camkit/capture.h"
#include "camkit/convert.h"
#include "camkit/encode.h"
#include "camkit/pack.h"
#include "camkit/network.h"
#include "camkit/timestamp.h"
};

#include "protocol.h"
#include <map>

typedef int(*Handler)(Pkg &pkg);
extern std::map<PKG_CMD_TYPE, Handler> handler_map;
extern std::map<PKG_CMD_TYPE, Handler>::iterator handler_map_it;

int send_pkg_2_server();

enum CAMERA_STATUS
{
    CAMERA_START,
    CAMERA_STOP
};

#endif //WAWA_RASPI_H
