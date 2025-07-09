#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <stdint.h>

namespace crypto::aes {
    class Key {
        std::vector<uint8_t> data;

        public:
        
        std::vector<uint8_t> &generate(int len_bytes = 256){
            std::vector<uint8_t> key(len_bytes);
            std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
            if (!urandom) 
                throw std::runtime_error("Cannot open /dev/urandom");
            
            urandom.read(reinterpret_cast<char*>(key.data()), len_bytes);
            
            if (!urandom) 
                throw std::runtime_error("Failed to read from /dev/urandom");
            
            data = key;
            return data;
        }

        std::vector<uint8_t> &get_data(){ return data; }

        std::string save_to_string() const {
            std::stringstream str;
            for (auto &b: data){
                str << b << ' ';
            }
            auto s = str.str();
            s.pop_back();
            return s;
        }

        void save_to_file(const std::string &path) const {
            std::ofstream ofs(path);
            for (auto &b: data){
                ofs << b << ' ';
            }
            ofs.close();
        }

        static Key load_from_str(std::string strdata){
            std::vector<uint8_t> data;
            
            std::string rdata;
            std::istringstream ifs(strdata);
            while (!ifs.eof()){
                ifs >> rdata;
                data.push_back(std::stoi(rdata));
            }

            return {data};
        }

        static Key load_from_file(const std::string &path) {
            std::vector<uint8_t> data;
            
            std::string rdata;
            std::ifstream ifs(path);
            while (!ifs.eof()){
                ifs >> rdata;
                data.push_back(std::stoi(rdata));
            }

            return {data};
        }

        Key(std::vector<uint8_t> data): 
            data(data) 
        {}

        Key(){}
        ~Key(){}
    };
}