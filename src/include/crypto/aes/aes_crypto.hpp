#pragma once

#include "aes.hpp"
#include "keys.hpp"
#include <cstdint>
#include <cmath>

namespace crypto::aes {
    class CryptoF {
        Key key;
        
        std::vector<uint8_t> iv;
        AES aes = AES(AESKeyLength::AES_256);

        std::vector<uint8_t> pkcs7_pad(const std::vector<uint8_t>& data, size_t block_size) {
            size_t pad_len = block_size - (data.size() % block_size);
            std::vector<uint8_t> padded = data;
            padded.insert(padded.end(), pad_len, static_cast<uint8_t>(pad_len));
            return padded;
        }

        std::vector<uint8_t> pkcs7_unpad(const std::vector<uint8_t>& data) {
            if (data.empty()) throw std::runtime_error("Data is empty");
            uint8_t pad_len = data.back();
            if (pad_len == 0) throw std::runtime_error("Invalid padding: pad_len == 0");
            if (pad_len > data.size()) throw std::runtime_error("Invalid padding: pad_len > data.size()");
            for (size_t i = data.size() - pad_len; i < data.size(); ++i) {
                if (data[i] != pad_len) throw std::runtime_error("Invalid PKCS#7 padding");
            }
            return std::vector<uint8_t>(data.begin(), data.end() - pad_len);
        }

        std::vector<uint8_t> generate_iv(){
            std::vector<uint8_t> key(16);
            std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
            if (!urandom) 
                throw std::runtime_error("Cannot open /dev/urandom");
            
            urandom.read(reinterpret_cast<char*>(key.data()), 16);
            
            if (!urandom) 
                throw std::runtime_error("Failed to read from /dev/urandom");
            
            return key;
        }

        public:

        std::vector<uint8_t> encrypt(const std::vector<uint8_t> &bytes) {
            // if (!key) throw std::runtime_error("<aes> [encrypt] cannot encrypt bytes with no key");
            std::vector<uint8_t> iv = generate_iv();
            auto padded = pkcs7_pad(bytes, 16);
            auto ciphertext = aes.EncryptCBC(padded, key.get_data(), iv);
            ciphertext.insert(ciphertext.begin(), iv.begin(), iv.end());
            return ciphertext;
        }

        std::vector<uint8_t> decrypt(const std::vector<uint8_t> &bytes){
            // if (!key) throw std::runtime_error("<aes> [decrypt] cannot decrypt bytes with no key");
            if (bytes.size() < 16) throw std::runtime_error("Ciphertext too short");
            std::vector<uint8_t> iv(bytes.begin(), bytes.begin() + 16);
            std::vector<uint8_t> ciphertext(bytes.begin() + 16, bytes.end());
            auto decrypted = aes.DecryptCBC(ciphertext, key.get_data(), iv);
            return pkcs7_unpad(decrypted);
        }

        void set_key(Key key){ this->key = key; }

        CryptoF(){}
        ~CryptoF(){}
    };
}