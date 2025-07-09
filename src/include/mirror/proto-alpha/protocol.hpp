#pragma once

#include "utility/strvec.hpp"
#include <rawnet/base_u.hpp>
#include <rawnet/common.hpp>

#include <functional>
#include <iostream>
#include <map>
#include <queue>

namespace mirror {

    #define emptl [](){}

    enum class STATUS_CODE {
        OK,
        INVALID_UID,
        UNREG_UID,
        UNDEF_MSG_TYPE,
        TUNNEL_NOT_EXISTS,
        TUNNEL_ALR_CONN,
        TUNNEL_NOT_CONN,
        UNKNOWN_ERROR_CODE
    };

    struct Tunnel {
        bool connected = false;
        nw::uid_t author, peer;

        std::queue<std::vector<uint8_t>> from_author;
        std::queue<std::vector<uint8_t>> to_author;

        Tunnel(nw::uid_t a): author(a) {}
        Tunnel(){}
    };
};