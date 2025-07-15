#pragma once
#include <iostream>
#include <array>

#include <rawnet/serv_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>

namespace stable {
    struct packet {
        short     bsum_even;
        short     bsum_odd;
        uint8_t   flag;
        long long con_uid;
        long long pack_uid;
        
        std::vector<uint8_t> body;

        void count_bytes(short &even, short &odd){
            even = 0; odd = 0;
            for (auto &b: body){
                odd += b % 2;
                even += !(b % 2);
            }
        }

        std::vector<uint8_t> create() {
            std::vector<uint8_t> data;

            data.push_back((bsum_even >> 8) & 0xFF);
            data.push_back(bsum_even & 0xFF);

            data.push_back((bsum_odd >> 8) & 0xFF);
            data.push_back(bsum_odd & 0xFF);

            data.push_back(flag);

            for (int i = 7; i >= 0; --i)
                data.push_back((con_uid >> (i * 8)) & 0xFF);

            for (int i = 7; i >= 0; --i)
                data.push_back((pack_uid >> (i * 8)) & 0xFF);

            uint32_t body_size = body.size();
            for (int i = 3; i >= 0; --i)
                data.push_back((body_size >> (i * 8)) & 0xFF);

            data.insert(data.end(), body.begin(), body.end());

            return data;
        }

        static packet create(std::vector<uint8_t> &data) {
            packet p;
            size_t offset = 0;

            // bsum_even
            p.bsum_even = (data[offset] << 8) | data[offset + 1];
            offset += 2;

            // bsum_odd
            p.bsum_odd = (data[offset] << 8) | data[offset + 1];
            offset += 2;

            // flag
            p.flag = data[offset];
            offset += 1;

            // con_uid
            p.con_uid = 0;
            for (int i = 0; i < 8; ++i) {
                p.con_uid = (p.con_uid << 8) | data[offset + i];
            }
            offset += 8;

            // pack_uid
            p.pack_uid = 0;
            for (int i = 0; i < 8; ++i) {
                p.pack_uid = (p.pack_uid << 8) | data[offset + i];
            }
            offset += 8;

            // body size
            uint32_t body_size = 0;
            for (int i = 0; i < 4; ++i) {
                body_size = (body_size << 8) | data[offset + i];
            }
            offset += 4;

            // body
            if (offset + body_size > data.size()) {
                // Ошибка: данных не хватает
                p.body.clear();
                std::cerr << "<stable/packet> [static:create] error: requested data size is bigger, than actually exists: " << offset + body_size << " > " << data.size() << std::endl;
                return p;
            }
            p.body = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + body_size);

            return p;
        }
    };
}