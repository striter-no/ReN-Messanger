#pragma once

#include <rawnet/cli_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>

#include <map>

namespace stable {
    class stable_cli {
        nw::uid_t uid;
        nw::client_udp rawcl; 

        public:

        nw::address get_serv_addr(){
            return rawcl.get_serv_addr();
        }

        void set_timeout(int seconds){
            rawcl.set_timeout(seconds);
        }
        
        int send(std::vector<uint8_t> &data){
            int i = rawcl.send(data);

            return i;
        }

        int recv(std::vector<uint8_t> &data, bool clear_buffer = false, int max_buff_size = 4096){
            int i = rawcl.recv(data, clear_buffer, max_buff_size);

            return i;
        }

        void change_serv_ip(nw::address addr, nw::uid_t uid = -1){
            rawcl.change_serv_ip(addr);
            
            if (uid != -1)
                this->uid = uid;
            else
                this->uid = nw::get_uid();
        }

        void create(){rawcl.create();}
        void stop(){rawcl.stop();}
    };
}