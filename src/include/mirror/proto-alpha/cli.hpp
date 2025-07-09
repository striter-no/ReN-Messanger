#pragma once

#include <optional>
#include "protocol.hpp"

namespace mirror {

    struct NetRequest {
        std::vector<uint8_t> raw_data;
        std::string type;

        void add(nw::uid_t u){
            auto s = std::to_string(u) + " ";
            raw_data.insert(raw_data.end(), s.begin(), s.end());
        }
        void add(int64_t t){
            auto s = std::to_string(t) + " ";
            raw_data.insert(raw_data.end(), s.begin(), s.end());
        }
        void add(const std::vector<uint8_t>& v){
            raw_data.insert(raw_data.end(), v.begin(), v.end());
            raw_data.push_back(' ');
        }
        void add(const std::string& s){
            raw_data.insert(raw_data.end(), s.begin(), s.end());
            raw_data.push_back(' ');
        }

        std::vector<uint8_t> get(){
            if (!raw_data.empty() && raw_data.back() == ' ')
                raw_data.pop_back();
            std::vector<uint8_t> out(type.begin(), type.end());
            out.push_back(' ');
            out.insert(out.end(), raw_data.begin(), raw_data.end());
            return out;
        }
    };

    class ClientProto {
        nw::address mirror_addr;
        nw::uid_t my_uid;

        public:
        
        nw::uid_t token(){return my_uid;}

        void change_mirror(nw::address addr){ mirror_addr = addr; }
        void token_reg(){
            my_uid = nw::get_uid();
        }

        nw::uid_t create_tunnel(NetRequest &r){
            nw::uid_t tuid = nw::get_uid();
            r.type = "open-tunnel";
            r.add(my_uid);
            r.add(tuid);

            return tuid;
        }

        void connect_tunnel(nw::uid_t &tun_uid, NetRequest &r){
            r.type = "conn-tunnel";
            r.add(my_uid);
            r.add(tun_uid);
        }
        
        void close_tunnel(nw::uid_t &tun_uid, NetRequest &r){
            r.type = "close-tunnel";
            r.add(my_uid);
            r.add(tun_uid);
        }

        void check_tunnel(nw::uid_t &tun_uid, NetRequest &r){
            r.type = "check-tunnel";
            r.add(my_uid);
            r.add(tun_uid);
        }

        void send(const std::vector<uint8_t>& data, nw::uid_t &tun_uid, NetRequest &r){
            r.type = "send";
            r.add(my_uid);
            r.add(tun_uid);
            r.add(data);
        }
        void send(const std::string& data, nw::uid_t &tun_uid, NetRequest &r){
            r.type = "send";
            r.add(my_uid);
            r.add(tun_uid);
            r.add(data);
        }
        
        void recv(nw::uid_t &tun_uid, NetRequest &r){
            r.type = "recv";
            r.add(my_uid);
            r.add(tun_uid);
        }

        void discovery(NetRequest &r){
            r.type = "discovery";
            r.add(my_uid);
        }

        // Десериализация ответа из байтового массива
        std::string recv(const std::vector<uint8_t>& data){
            std::string str(data.begin(), data.end());
            if (str == "no-data"){
                return {};
            } else if (str == "not-exists"){
                throw std::runtime_error("tunnel does not exists");
            } else {
                return str;
            }
        }

        std::vector<nw::uid_t> discovery(const std::vector<uint8_t>& data, nw::uid_t my_tunnel = -1){
            std::string str(data.begin(), data.end());
            if (str == "nothing"){
                return {};
            }
            int tracker = 0;
            std::vector<nw::uid_t> out;
            for (int i = 0; i < count(str, ' '); i++){
                std::string s = next_simb(str, tracker, ' ');

                auto uid = std::stoll(s);
                if (uid == my_tunnel) continue;

                out.push_back(uid);
            }

            return out;
        }

        std::optional<nw::uid_t> check_tunnel(const std::vector<uint8_t>& data){
            std::string str(data.begin(), data.end());
            if (str == "no"){
                return std::nullopt;
            } else if(str == "not-exists" || str == "alr-conn" || str == "only-one"){
                throw std::runtime_error("<proto> [check-tunnel] server disagree: " + str);
            }

            return std::stoll(str);
        }
        
        ClientProto(){}
        ~ClientProto(){}
    };
}