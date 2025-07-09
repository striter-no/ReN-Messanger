#pragma once

#include <functional>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <map>

#include "common.hpp"
#include "common_tcp.hpp"

namespace nw {
    class client_tcp {

        nw::address addr;
        int sockfd;
        struct sockaddr_in servaddr, cli;

        public:
        
        void disconn(){
            close(sockfd);
        }

        void recv(std::vector<char> &data, bool clear_buffer = true, int max_buffer_size = 4096){
            data.resize(max_buffer_size);
            int bytes_read = read(sockfd, &data[0], max_buffer_size);
            
            if (bytes_read < 0) {
                data.clear();
                return;
            }
            
            data.resize(bytes_read);

            if (clear_buffer && bytes_read > 0){
                for (int i = 0; i < data.size(); i++){
                    if (data[i] == '\0'){
                        data.resize(i);
                        break;
                    }
                }
            }
        }

        int send(std::vector<char> &data){
            int bytes_sent = write(sockfd, &data[0], data.size());
            if (bytes_sent < 0) {
                throw std::runtime_error("[r-e] failed to send data");
            }
            return bytes_sent;
        }

        void conn(nw::address addr){
            this->addr = addr;

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr(addr.ip.c_str());
            servaddr.sin_port = htons(addr.port);

            if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
                throw std::runtime_error("[r-e] failed to connect to the server: " + addr.str());
            }
        }

        void create(){
            if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
                throw std::runtime_error("[r-e] failed to create socket");
            }
        }

        client_tcp(){}
        ~client_tcp(){}
    };
};