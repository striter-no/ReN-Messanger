#pragma once

#include "common.hpp"
#include <sys/time.h>
#include <cerrno>
#include <cstring>
#include <iostream>

namespace nw {
    class client_udp {
            struct sockaddr_in servaddr; 
            int sockfd;
        
            address server_address;

        public:

            address get_serv_addr(){
                return server_address;
            }

            void set_timeout(int seconds) {
                struct timeval timeout={seconds,0};

                if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(uint8_t*)&timeout,sizeof(struct timeval)) < 0) {
                    throw std::runtime_error("[r-e] cannot set socket rcv timeout");
                }
            }

            int send(std::vector<uint8_t> &data){
                return sendto(
                    sockfd, 
                    &data[0], data.size(), 
                    MSG_CONFIRM, 
                    (const struct sockaddr *) &servaddr, sizeof(servaddr)
                ); 
            }
            
            int recv(std::vector<uint8_t> &data, bool clear_buffer = true, int max_buff_size = 4096){
                data.resize(max_buff_size);
                socklen_t len = sizeof(servaddr);
                int n = recvfrom(
                    sockfd, &data[0], 
                    max_buff_size,  
                    0, 
                    (struct sockaddr *) &servaddr, &len
                );

                if (n < 0) {
                    std::cerr << "recvfrom failed: " << strerror(errno) << std::endl;
                }
                
                if (n >= 0) {
                    if (clear_buffer){
                        for (int i = 0; i < data.size(); i++){
                            if (data[i] == '\0'){
                                data.resize(i);
                                break;
                            }
                        }
                    } else {
                        data.resize(n);
                    }
                } else {
                    data.clear();
                }
                return n;
            }

            void change_serv_ip(address addr){
                server_address = addr;

                memset(&servaddr, 0, sizeof(servaddr)); 
                  
                // Filling server information 
                servaddr.sin_family = AF_INET; 
                servaddr.sin_port = htons(server_address.port); 
                servaddr.sin_addr.s_addr = inet_addr(server_address.ip.c_str());
            }

            void create(){
                if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
                    throw std::runtime_error("[r-e] cannot create a client socket");
                }
            }
            
            void stop(){ close(sockfd); }

            client_udp(){}
            ~client_udp(){}
    };
}