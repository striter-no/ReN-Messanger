#pragma once

#include <iostream>

#include <rawnet/serv_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>

#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>

namespace stable {
    class stable_server {
        nw::server_udp raws;

        std::function<void(
            nw::address,
            std::vector<uint8_t>&, 
            std::vector<uint8_t>&
        )> cb;

        void deflect(nw::address ca, std::vector<uint8_t> &d, std::vector<uint8_t> &a){
            if (cb) {
                cb(ca, d, a);
            } else {
                // Fallback behavior if no callback is set
                a = nw::getv("internal-error");
            }
        }

        public:

        nw::address get_addr(){return raws.get_addr();}
        int get_threads_num(){return raws.get_threads_num();}

        void set_threads(int n){ raws.set_threads(n); }
        void create(nw::address addr){ raws.create(addr); }

        void on_msg_cli (
            std::function<void(
                nw::address,
                std::vector<uint8_t>&, 
                std::vector<uint8_t>&
            )> v
        ){ cb = v; }
        
        void run(bool detached = false, bool clear_buffer = false, int max_buff_size = 4096){
            raws.on_msg_cli([this](nw::address ca, std::vector<uint8_t> &d, std::vector<uint8_t> &a){
                deflect(ca, d, a);
            });
            
            raws.run(detached, clear_buffer, max_buff_size);
        }

        void stop(){ raws.stop(); }

        stable_server(){}
        ~stable_server(){}
    };
}