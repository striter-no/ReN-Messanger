#pragma once

#include "rsa.hpp"

#include <vector>
#include <string>
#include <stdexcept>

namespace crypto {
    class CryptoF {
        public:

        std::vector<uint8_t> encrypt(const std::string& data) {
            std::vector<uint8_t> rdata(data.begin(), data.end());
            if (rdata.size() > max_msg_size)
                throw std::invalid_argument("Message too long for OAEP");
            return _encrypt(rdata);
        }

        std::string decrypt(const std::vector<uint8_t>& encrypted) {
            auto decr = _decrypt(const_cast<std::vector<uint8_t>&>(encrypted));
            return std::string(decr.begin(), decr.end());
        }

        void set_key_pair(rsa::PrivateKey *pr_key, rsa::PublicKey *pb_key){
            this->pr_key = pr_key;
            this->pb_key = pb_key;

            k = pb_key->k();
            hLen = 32; // SHA-256
            max_msg_size = k - 1;
            
            log_to_file("crypto.log", " ");
            log_to_file("crypto.log", "set_key_pair:k: " + std::to_string(k));
            log_to_file("crypto.log", "set_key_pair:hLen: " + std::to_string(hLen));
            log_to_file("crypto.log", "set_key_pair:max_msg_size: " + std::to_string(max_msg_size));
        }

        CryptoF(){}
        ~CryptoF(){}

        private:
        crypto::rsa::PrivateKey *pr_key;
        crypto::rsa::PublicKey *pb_key;

        size_t k;
        size_t hLen;
        size_t max_msg_size;

        std::vector<uint8_t> _encrypt(const std::vector<uint8_t>& rdata) {
            return crypto::rsa::encrypt(rdata, *pb_key);
        }

        std::vector<uint8_t> _decrypt(std::vector<uint8_t> &to_dec) {
            return crypto::rsa::decrypt(to_dec, *pr_key);
        }
    };
}