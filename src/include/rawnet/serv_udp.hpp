#pragma once

#include <stdexcept>
#include <functional>
#include <thread>
#include "common.hpp"

namespace nw {
    class server_udp {
            bool stop_event = false;
            std::function<void(address, std::vector<uint8_t>&, std::vector<uint8_t>&)> v_on_msg_cli;

            int buf_len;
            int threads;
            address addr;

            int sockfd;
            struct sockaddr_in servaddr;
            std::vector<struct sockaddr> clients;
            std::vector<std::thread> client_threads;

            void process_client(struct sockaddr &clisock, int cli_inx, bool clear_buffer = true){
                while (not stop_event) {
                    socklen_t len = sizeof(clisock);  //len is value/result 
    
                    std::vector<uint8_t> buffer;
                    buffer.resize(buf_len);
                    int n = recvfrom(
                        sockfd, &buffer[0], buf_len,  
                        MSG_WAITALL, 
                        (struct sockaddr *) &clisock, &len
                    );
                    
                    if (n < 0) {
                        if(stop_event) break;
                        else continue;
                    }

                    if (n >= 0) {
                        if (clear_buffer){
                            for (int i = 0; i < buffer.size(); i++){
                                if (buffer[i] == '\0'){
                                    buffer.resize(i);
                                    break;
                                }
                            }
                        } else {
                            buffer.resize(n);
                        }
                    } else {
                        buffer.clear();
                    }
                    
                    struct sockaddr_in* cli_in_addr = (struct sockaddr_in*) &clisock;
                    address cli_addr(
                        inet_ntoa(cli_in_addr->sin_addr),
                        ntohs(cli_in_addr->sin_port)
                    );
                    
                    std::vector<uint8_t> ans_buffer;
                    v_on_msg_cli(cli_addr, buffer, ans_buffer);
                    
                    sendto(
                        sockfd, 
                        &ans_buffer[0], ans_buffer.size(), 
                        MSG_CONFIRM, 
                        (const struct sockaddr *) &clisock, len
                    );
                }
            }

        public:
            address get_addr(){ return addr; }
            int get_threads_num(){ return threads; }

            void on_msg_cli (
                std::function<void(address, std::vector<uint8_t>&, std::vector<uint8_t>&)> v
            ){ v_on_msg_cli = v; }

            void set_threads(int num){ threads = num; }
            
            void run(bool detached = false, bool clear_buffer = true, int max_buff_size = 4096){
                buf_len = max_buff_size;
                clients.resize(threads);

                for(auto &cs: clients)
                    memset(&cs, 0, sizeof(cs));
                
                client_threads.resize(threads);

                for (int i = 0; i < threads; i++) {
                    client_threads[i] = std::thread(
                        [&](){process_client(clients[i], i, clear_buffer);}
                    );
                }

                for(auto &t: client_threads)
                    if(t.joinable() && !detached) t.join();
                    else t.detach();
            }

            void create(address addr){
                if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
                    throw std::runtime_error("[r-e] server socket creation failed");
                }

                int opt = 1;
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
                    close(sockfd);
                    throw std::runtime_error("[r-e] server setsockopt(SO_REUSEADDR) failed");
                }

                memset(&servaddr, 0, sizeof(servaddr));
                this->addr = addr;

                servaddr.sin_family    = AF_INET; // IPv4 
                servaddr.sin_addr.s_addr = inet_addr(addr.ip.c_str()); 
                servaddr.sin_port = htons(addr.port);

                if(bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ){ 
                    throw std::invalid_argument("[i-a] server socket binding failed to address " + addr.str());
                }
            }
            
            void stop(){
                stop_event = true;
                close(sockfd);

                for(auto &t: client_threads)
                    if(t.joinable()) t.join();
            }

            server_udp(){}
            ~server_udp(){}
    };
}