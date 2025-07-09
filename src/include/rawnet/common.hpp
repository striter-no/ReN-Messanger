#pragma once

#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <cstring> 

#include <string>
#include <vector>

namespace nw {
    class address {
        public:

        std::string ip;
        int port;

        std::string str() const {
            return ip + ':' + std::to_string(port);
        }

        static address from_str(std::string data){
            return {
                std::string(data.begin(), data.begin() + data.find_first_of(':')),
                std::stoi(std::string(data.begin() + data.find_first_of(':') + 1, data.end()))
            };
        }

        const bool operator==(address other) const {
            return ip == other.ip && port == other.port;
        }

        const bool operator!=(address other) const {
            return ip != other.ip && port != other.port;
        }

        const bool operator<(address other) const {
            return ip < other.ip || port < other.port;
        }

        address(
            std::string ip,
            int port
        ): ip(ip), port(port) {}

        address(){}
        ~address(){}
    };
    
}