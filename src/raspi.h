//
// Created by junling on 2017/9/25.
//

#ifndef WAWA_RASPI_H
#define WAWA_RASPI_H

#include "protocol.h"
#include <map>

typedef int(*Handler)(Pkg &pkg);
extern std::map<PKG_CMD_TYPE, Handler> handler_map;
extern std::map<PKG_CMD_TYPE, Handler>::iterator handler_map_it;

int send_pkg_2_server();


#endif //WAWA_RASPI_H
