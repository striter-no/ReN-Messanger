#pragma once

#include "utility/strvec.hpp"
#include <rawnet/base_u.hpp>

#ifndef ENCRYPTED_RNET
#include <rawnet/serv_udp.hpp>
using base_server_t = nw::server_udp;
#else
#include <ernet/serv_udp.hpp>
using base_server_t = ernet::ernet_server;
#endif

#include <shared_mutex>
#include <iostream>
#include <map>

namespace mirror {

    struct Argument {
        bool is_mono = true;
    };

    class Server {
        std::shared_mutex clients_mutex;
        std::thread cleanup_thread;
        std::atomic<bool> stop_cleanup{false};
        base_server_t raw_serv;

        std::map<nw::uid_t, int64_t> clients;
        std::map<std::string, std::function<
            std::vector<uint8_t>(
                nw::address &cli_addr,
                nw::uid_t &cli_uid,
                nw::uid_t &tun_uid,
                std::vector<std::vector<uint8_t>> arguments
            )
        >> message_callbacks;
        std::map<std::string, std::vector<Argument>> args_per_type;

        void cleanup_inactive_clients(int timeout_sec = 300) {
            int64_t now = nw::timestamp<std::chrono::seconds>();
            std::vector<nw::uid_t> to_del;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                for (const auto& [uid, last] : clients) {
                    if (now - last > timeout_sec) {
                        to_del.push_back(uid);
                    }
                }
            }
            for (auto uid : to_del) {
                std::lock_guard<std::shared_mutex> lock(clients_mutex);
                clients.erase(uid);
            }
        }

        void deflect(nw::address cliaddr, std::vector<uint8_t> &data, std::vector<uint8_t> &ans){
            // std::cout << "[MIRROR] deflect: received " << data.size() << " bytes" << std::endl;
            // std::cout << "[MIRROR] data hex: ";
            // for (auto b : data) {
            //     printf("%02x ", b);
            // }
            // std::cout << std::endl;
            
            int tracker = 0;

            std::string strdata = std::string(data.begin(), data.end());
            // std::cout << "[MIRROR] data as string: '" << strdata << "'" << std::endl;

            try {
                std::string type = next_simb(strdata, tracker, ' ');
                // std::cout << "[MIRROR] parsed type: '" << type << "'" << std::endl;
                
                nw::uid_t uid = std::stoll(next_simb(strdata, tracker, ' '));
                // std::cout << "[MIRROR] parsed uid: " << uid << std::endl;
                
                // std::cout << "[" << cliaddr.str() << "] " << '"' << strdata << '"' << std::endl;
                // std::cout << "\tuid: " << uid << std::endl;

                {
                    std::lock_guard<std::shared_mutex> lock(clients_mutex);
                    clients[uid] = nw::timestamp<std::chrono::seconds>();
                }
                
                nw::uid_t tunnel_uid = -1;
                if (count(strdata, ' ') > 1) {
                    tunnel_uid = std::stoll(next_simb(strdata, tracker, ' '));
                    // std::cout << "[MIRROR] parsed tunnel_uid: " << tunnel_uid << std::endl;
                }

                if (message_callbacks.find(type) == message_callbacks.end()){
                    // std::cout << "[MIRROR] ERROR: unregistered message type '" << type << "'" << std::endl;
                    ans = nw::getv("unreg-msg-type");
                    return;
                }

                auto &cb   = message_callbacks[type];
                auto &args = args_per_type[type];
                
                std::vector<std::vector<uint8_t>> str_args;
                for (auto &a: args){
                    if (a.is_mono){
                        auto d = next_simb(strdata, tracker, ' ');
                        str_args.push_back({d.begin(), d.end()});
                    } else {
                        str_args.push_back(std::vector<uint8_t>(strdata.begin() + tracker, strdata.end()));
                        break;
                    }
                }

                std::vector<uint8_t> strans = cb(cliaddr, uid, tunnel_uid, str_args);
                if (!(type == "check-tunnel" || type == "discovery") ){
                    // std::cout << "\ttun_uid: " << tunnel_uid << std::endl;
                    // std::cout << "-> " << std::string(strans.begin(), strans.end()) << std::endl << std::endl;
                }

                ans = strans;
            } catch (const std::exception& e) {
                std::cout << "[MIRROR] ERROR parsing message: " << e.what() << std::endl;
                std::cout << "[MIRROR] tracker: " << tracker << ", strdata length: " << strdata.length() << std::endl;
                std::cout << "---------------" << std::endl;
                std::cout << strdata << std::endl << std::endl;
                ans = nw::getv("parse-error");
            }
        }

        public:

        void new_msg(
            std::string type, 
            std::vector<Argument> arguments,
            std::function<std::vector<uint8_t>(nw::address&, nw::uid_t&, nw::uid_t&, std::vector<std::vector<uint8_t>>)> callback
        ){
            message_callbacks[type] = callback;
            args_per_type[type] = arguments;
        }

        void run(){ raw_serv.run(true); }
        void set_threads(int n){ raw_serv.set_threads(n); }

        Server(
            nw::address bind_address
        ){ 
            raw_serv.create(bind_address); 
            // stop_cleanup = false;
            // cleanup_thread = std::thread([this]{
            //     while (!stop_cleanup) {
            //         std::this_thread::sleep_for(std::chrono::seconds(60));
            //         cleanup_inactive_clients(300); // 5 минут таймаут
            //     }
            // });
            raw_serv.on_msg_cli([&](nw::address a, std::vector<uint8_t> &b, std::vector<uint8_t> &c){
                deflect(a, b, c);
            });
        }

        Server(){}
        ~Server(){
            raw_serv.stop();
            
            // stop_cleanup = true;
            // if (cleanup_thread.joinable()) cleanup_thread.join();
        }
    };
}