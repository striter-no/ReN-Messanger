#pragma once

#include "protocol.hpp"

#include <mutex>
#include <thread>
#include <atomic>

namespace mirror {

    class ServerProto {
        std::map<nw::uid_t, Tunnel> tunnels;
        std::map<nw::uid_t, int64_t> tun_time;
        std::mutex tunnels_mtx;

        std::thread cleanup_thread;
        std::atomic<bool> stop_cleanup{false};

        void cleanup_inactive_tunnels(int timeout_sec = 300) {
            int64_t now = nw::timestamp();
            std::vector<nw::uid_t> to_del;
            tunnels_mtx.lock();
            for (const auto& [uid, last] : tun_time) {
                if (now - last > timeout_sec) {
                    to_del.push_back(uid);
                }
            }
            for (auto uid : to_del) {
                tunnels.erase(uid);
                tun_time.erase(uid);
            }
            tunnels_mtx.unlock();
        }

        public:

        ServerProto(){
            stop_cleanup = false;
            cleanup_thread = std::thread([this]{
                while (!stop_cleanup) {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    cleanup_inactive_tunnels(300); // 5 минут таймаут
                }
            });
        }

        ~ServerProto(){
            stop_cleanup = true;
            if (cleanup_thread.joinable()) cleanup_thread.join();
        }

        std::vector<uint8_t> open_tunnel( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) != tunnels.end()) {
                tunnels_mtx.unlock(); 
                std::cout << tun_uid << std::endl;
                std::string msg = "alr-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            tunnels.try_emplace(tun_uid, cli_uid);

            tunnels_mtx.unlock();

            std::string msg = "ok";
            return std::vector<uint8_t>(msg.begin(), msg.end());
        }

        std::vector<uint8_t> close_tunnel( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) == tunnels.end()) {
                tunnels_mtx.unlock(); 
                std::string msg = "not-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            tunnels.erase(tun_uid);

            tunnels_mtx.unlock();

            std::string msg = "ok";
            return std::vector<uint8_t>(msg.begin(), msg.end());
        }

        std::vector<uint8_t> conn_tunnel( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments ){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) == tunnels.end()) {
                tunnels_mtx.unlock(); 
                std::string msg = "not-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            if (tunnels[tun_uid].connected) {
                tunnels_mtx.unlock(); 
                std::string msg = "alr-conn";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }

            std::string ab_msg;
            for (auto &[u, t]: tunnels){
                if (t.author == cli_uid){
                    ab_msg = "only-one";
                }
            }
            if (!ab_msg.empty()){
                tunnels_mtx.unlock();
                return std::vector<uint8_t>(ab_msg.begin(), ab_msg.end());
            }

            tunnels[tun_uid].connected = true;
            tunnels[tun_uid].peer = cli_uid;

            tunnels_mtx.unlock();
            std::string msg = "ok";
            return std::vector<uint8_t>(msg.begin(), msg.end());
        }

        std::vector<uint8_t> check_tunnel(nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) == tunnels.end()) {
                std::cout << tun_uid << std::endl; 
                tunnels_mtx.unlock(); 
                std::string msg = "not-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            
            bool conn = tunnels[tun_uid].connected;
            tunnels_mtx.unlock();
            if (conn) {
                std::string msg = std::to_string(tunnels[tun_uid].peer);
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            std::string msg = "no";
            return std::vector<uint8_t>(msg.begin(), msg.end());
        }

        std::vector<uint8_t> send_bytes( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) == tunnels.end()) {
                tunnels_mtx.unlock(); 
                std::string msg = "not-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            
            auto &tun = tunnels[tun_uid];
            
            if (tun.author != cli_uid) tun.to_author.push(arguments[0]);
            else tun.from_author.push(arguments[0]);

            tunnels_mtx.unlock();
            std::string msg = "ok";
            return std::vector<uint8_t>(msg.begin(), msg.end());
        }

        std::vector<uint8_t> recv_bytes( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            if (tunnels.find(tun_uid) == tunnels.end()) {
                tunnels_mtx.unlock(); 
                std::string msg = "not-exists";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            if (tun_uid != -1) tun_time[tun_uid] = nw::timestamp();
            
            auto &tun = tunnels[tun_uid];
            
            std::vector<uint8_t> data;
            if (tun.author == cli_uid){
                if (tun.to_author.empty()) {
                    tunnels_mtx.unlock(); 
                    std::string msg = "no-data";
                    return std::vector<uint8_t>(msg.begin(), msg.end());
                }

                data = tun.to_author.front();
                tun.to_author.pop();
            } else {
                if (tun.from_author.empty()) {
                    tunnels_mtx.unlock(); 
                    std::string msg = "no-data";
                    return std::vector<uint8_t>(msg.begin(), msg.end());
                }

                data = tun.from_author.front();
                tun.from_author.pop();
            }
            
            tunnels_mtx.unlock();
            return data;
        }

        std::vector<uint8_t> discovery( nw::address &cli_addr, nw::uid_t &cli_uid, nw::uid_t &tun_uid, std::vector<std::vector<uint8_t>> arguments){
            tunnels_mtx.lock();
            
            int64_t time = nw::timestamp();
            std::vector<uint8_t> out;
            for (auto &[u,t]: tunnels){
                if (!t.connected && (time - tun_time[u] < 60)) {
                    std::string s = std::to_string(u) + " ";
                    out.insert(out.end(), s.begin(), s.end());
                }
            }

            tunnels_mtx.unlock();

            if (out.empty()) {
                std::string msg = "nothing";
                return std::vector<uint8_t>(msg.begin(), msg.end());
            }
            return out;
        }
    };

    
};