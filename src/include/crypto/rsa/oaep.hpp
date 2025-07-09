#pragma once
#include "../sha256.hpp"
#include "keys.hpp"
#include "mpz_math.hpp"

#include <random>
#include <cstring>

namespace crypto::oaep {

    // OAEP Encode
    std::vector<uint8_t> encode(const std::vector<uint8_t>& message, size_t k, const std::vector<uint8_t>& label) {
        size_t hLen = 32; // SHA-256
        
        if (message.size() > k - 2 * hLen - 2)
            throw std::runtime_error("OAEP encode: message too long");

        // 1. lHash = Hash(label)
        auto lHash = crypto::sha256::hash(label);

        // 2. PS (padding string)
        size_t psLen = k - message.size() - 2 * hLen - 2;
        std::vector<uint8_t> PS(psLen, 0x00);

        // 3. DB = lHash || PS || 0x01 || message
        std::vector<uint8_t> DB;
        DB.insert(DB.end(), lHash.begin(), lHash.end());
        DB.insert(DB.end(), PS.begin(), PS.end());
        DB.push_back(0x01);
        DB.insert(DB.end(), message.begin(), message.end());

        // 4. seed (random)
        std::vector<uint8_t> seed(hLen);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, 255);
        for (size_t i = 0; i < hLen; ++i)
            seed[i] = static_cast<uint8_t>(dis(gen));

        // 5. dbMask = MGF1(seed, k - hLen - 1)
        auto dbMask = mgf1(seed, k - hLen - 1);

        // 6. maskedDB = DB XOR dbMask
        std::vector<uint8_t> maskedDB(DB.size());
        for (size_t i = 0; i < DB.size(); ++i)
            maskedDB[i] = DB[i] ^ dbMask[i];

        // 7. seedMask = MGF1(maskedDB, hLen)
        auto seedMask = mgf1(maskedDB, hLen);

        // 8. maskedSeed = seed XOR seedMask
        std::vector<uint8_t> maskedSeed(hLen);
        for (size_t i = 0; i < hLen; ++i)
            maskedSeed[i] = seed[i] ^ seedMask[i];

        // 9. EM = 0x00 || maskedSeed || maskedDB
        std::vector<uint8_t> EM;
        EM.push_back(0x00);
        EM.insert(EM.end(), maskedSeed.begin(), maskedSeed.end());
        EM.insert(EM.end(), maskedDB.begin(), maskedDB.end());
        
        if (EM.size() > k) {
            throw std::runtime_error("OAEP encoded data exceeds target size: " + std::to_string(EM.size()) + " > " + std::to_string(k));
        }
        
        // КРИТИЧНО: Проверяем, что EM как число меньше модуля n
        // Если EM начинается с 0x00, то как число оно будет меньше 2^(k*8-8)
        // Но нам нужно убедиться, что оно меньше n

        return EM;
    }

    // OAEP Decode
    std::vector<uint8_t> decode(const std::vector<uint8_t>& encoded, size_t k, const std::vector<uint8_t>& label) {
        
        size_t hLen = 32; // SHA-256

        if (encoded.size() != k || k < 2 * hLen + 2)
            throw std::runtime_error("OAEP decode: encoded length error");

        // 1. Разбиваем EM
        uint8_t Y = encoded[0];
        std::vector<uint8_t> maskedSeed(encoded.begin() + 1, encoded.begin() + 1 + hLen);
        std::vector<uint8_t> maskedDB(encoded.begin() + 1 + hLen, encoded.end());

        // 2. seedMask = MGF1(maskedDB, hLen)
        auto seedMask = mgf1(maskedDB, hLen);

        // 3. seed = maskedSeed XOR seedMask
        std::vector<uint8_t> seed(hLen);
        for (size_t i = 0; i < hLen; ++i)
            seed[i] = maskedSeed[i] ^ seedMask[i];

        // 4. dbMask = MGF1(seed, k - hLen - 1)
        auto dbMask = mgf1(seed, k - hLen - 1);

        // 5. DB = maskedDB XOR dbMask
        std::vector<uint8_t> DB(maskedDB.size());
        for (size_t i = 0; i < maskedDB.size(); ++i)
            DB[i] = maskedDB[i] ^ dbMask[i];

        // 6. lHash = Hash(label)
        auto lHash = crypto::sha256::hash(label);
        // Логируем lHash
        std::string lHash_str;
        for (auto b : lHash) lHash_str += (b < 16 ? "0" : "") + std::to_string((int)b);

        // Логируем DB
        std::string DB_str;
        for (auto b : DB) DB_str += (b < 16 ? "0" : "") + std::to_string((int)b);

        // 7. Проверка lHash
        if (!std::equal(DB.begin(), DB.begin() + hLen, lHash.begin())){
            throw std::runtime_error("OAEP decode: label hash mismatch");
        }

        // 8. Поиск 0x01 после lHash
        size_t idx = hLen;
        while (idx < DB.size() && DB[idx] == 0x00) ++idx;
        if (idx == DB.size() || DB[idx] != 0x01)
            throw std::runtime_error("OAEP decode: 0x01 separator not found");

        // 9. Всё после 0x01 — сообщение
        std::vector<uint8_t> message(DB.begin() + idx + 1, DB.end());
        return message;
    }

}