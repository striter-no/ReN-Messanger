#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>

#include <functional>
#include <stdexcept>
#include <thread>
#include <memory>
#include <mutex>
#include <map>

#include "common.hpp"
#include "common_tcp.hpp"
#include "base_u.hpp"

#ifndef SO_ORIGINAL_DST
#define SO_ORIGINAL_DST 80
#endif

namespace nw {
    class server_tcp {
            nw::address addr;

            int clinum = 1, max_buff_size = 4096;
            int sockfd;
            struct sockaddr_in servaddr;
        
            std::function<void(nw::address &addrm, nw::uid_t, std::vector<char> &inp, std::vector<char> &out)> on_msg_cli_cb;
            std::function<void(nw::address &addrm, nw::uid_t, nw::address &target_addr)> on_msg_conn_cb, on_msg_disc_cb;
        public:

        void set_clients(int clinum){ this->clinum = clinum; }
        void set_max_buff_size(int v){ max_buff_size = v; }

        void on_msg_cli (std::function<void(nw::address &addr, nw::uid_t, std::vector<char> &inp, std::vector<char> &out)> v){on_msg_cli_cb = v;}
        void on_msg_conn(std::function<void(nw::address &addrm, nw::uid_t, nw::address &target_addr)> v){on_msg_conn_cb = v;}
        void on_msg_disc(std::function<void(nw::address &addrm, nw::uid_t, nw::address &target_addr)> v){on_msg_disc_cb = v;}
        
        void run(bool get_original_dst = false, bool clear_buffer = true){
            while (true) {
                struct sockaddr_in cli;
                unsigned int len = sizeof(cli);
                int connfd = accept(sockfd, (struct sockaddr*)&cli, &len);
                if (connfd < 0) {
                    std::cerr << "accept error" << std::endl;
                    continue;
                }

                nw::address target_addr;
                struct sockaddr_in orig_dst;
                socklen_t orig_dst_len = sizeof(orig_dst);
                if (get_original_dst && getsockopt(connfd, SOL_IP, SO_ORIGINAL_DST, &orig_dst, &orig_dst_len) == 0) {
                    target_addr.ip = inet_ntoa(orig_dst.sin_addr);
                    target_addr.port = ntohs(orig_dst.sin_port);
                } else if (get_original_dst) {
                    std::cerr << "error getting original dst" << std::endl;
                }

                nw::address cliaddr;
                cliaddr.ip = inet_ntoa(cli.sin_addr);
                cliaddr.port = ntohs(cli.sin_port);

                // Новый поток для клиента
                std::thread([=, *this]() mutable {
                    nw::uid_t inx = nw::get_uid();
                    if (on_msg_conn_cb) on_msg_conn_cb(cliaddr, inx, target_addr);

                    while (true) {
                        std::vector<char> buffer, ans;
                        buffer.resize(max_buff_size);
                        ans.resize(max_buff_size);

                        int n = read(connfd, &buffer[0], buffer.size());
                        if (n > 0) {
                            buffer.resize(n);
                            if (clear_buffer){
                                for (int i = 0; i < buffer.size(); i++){
                                    if (buffer[i] == '\0'){
                                        buffer.resize(i);
                                        break;
                                    }
                                }
                            }
                            if (on_msg_cli_cb) on_msg_cli_cb(cliaddr, inx, buffer, ans);
                            if (ans.size() > 0) {
                                int written = write(connfd, &ans[0], ans.size());
                                if (written < 0) {
                                    std::cerr << "Failed to write response to client" << std::endl;
                                }
                            }
                        } else {
                            if (on_msg_disc_cb) on_msg_disc_cb(cliaddr, inx, target_addr);
                            break;
                        }
                    }
                    close(connfd);
                }).detach();
            }
        }

        void stop(){
            close(sockfd);
        }

        void create(){
            if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { 
                throw std::runtime_error("[r-e] server socket creation failed");
            }
            bzero(&servaddr, sizeof(servaddr));

            // assign IP, PORT 
            servaddr.sin_family = AF_INET; 
            servaddr.sin_addr.s_addr = inet_addr(addr.ip.c_str()); 
            servaddr.sin_port = htons(addr.port);

            if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0){
                throw std::runtime_error("[r-e] binding failed");
            }

            if ((listen(sockfd, clinum)) != 0){
                throw std::runtime_error("[r-e] listening failed");
            }
        }

        server_tcp(
            nw::address addr
        ): addr(addr) {}

        server_tcp(){}
        ~server_tcp(){}
    };
}