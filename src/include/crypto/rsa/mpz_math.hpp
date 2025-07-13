#pragma once
#include "../sha256.hpp"

#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <cstdint>
#include <gmpxx.h>
#include <utility/log.hpp>

namespace crypto {
    std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
        std::ostringstream oss;
        for (auto b : bytes) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b) << ' ';
        }
        return oss.str();
    }

    std::string bytes_to_hex(const std::string& bytes) {
        std::ostringstream oss;
        for (auto b : bytes) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        return oss.str();
    }

    // Преобразует mpz_class в массив байтов длины k (big-endian, с ведущими нулями)

    std::vector<uint8_t> legacy_mpz_to_bytes(const mpz_class& num, size_t k) {
        std::vector<uint8_t> result(k, 0);
        size_t count = 0;
        // Временный буфер для экспорта
        std::vector<uint8_t> raw((mpz_sizeinbase(num.get_mpz_t(), 2) + 7) / 8);
        if (!raw.empty()) {
            mpz_export(raw.data(), &count, 1, 1, 1, 0, num.get_mpz_t());
            raw.resize(count);
            if (raw.size() > k) {
                // Обрезаем лишние байты (маловероятно)
                std::copy(raw.end() - k, raw.end(), result.begin());
            } else {
                // Копируем raw в конец result (выравнивание по правому краю)
                std::copy(raw.begin(), raw.end(), result.begin() + (k - raw.size()));
            }
        }
        // Если raw пустой (num == 0), result уже заполнен нулями
        int leading_zeros = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == 0) leading_zeros++;
            else break;
        }
        return result;
    }

    std::vector<uint8_t> mpz_to_bytes(const mpz_class& num, size_t k) {
        std::vector<uint8_t> result(k, 0);
        if (num == 0) return result;
        
        size_t num_bits = mpz_sizeinbase(num.get_mpz_t(), 2);
        size_t num_bytes = (num_bits + 7) / 8;
        
        if (num_bytes > k)
            throw std::runtime_error("Number too large for target size: " + std::to_string(num_bytes) + " > " + std::to_string(k));
        size_t leading_zeros = k - num_bytes;
        size_t count = 0;
        
        mpz_export(result.data() + leading_zeros, &count, 1, 1, 1, 0, num.get_mpz_t());
        
        return result;
    }

    // Преобразует массив байтов (big-endian) в mpz_class
    inline mpz_class bytes_to_mpz(const std::vector<uint8_t>& buf) {
        mpz_class num;
        mpz_import(num.get_mpz_t(), buf.size(), 1, 1, 1, 0, buf.data());
        return num;
    }

    std::vector<uint8_t> mgf1(const std::vector<uint8_t>& seed, size_t maskLen) {
        std::vector<uint8_t> mask;
        size_t hLen = 32; // SHA-256 = 32 байта
        uint32_t counter = 0;
        while (mask.size() < maskLen) {
            std::vector<uint8_t> data(seed);
            // Добавляем счетчик (4 байта, big-endian)
            for (int i = 3; i >= 0; --i)
                data.push_back((counter >> (8 * i)) & 0xFF);

            auto hash = crypto::sha256::hash(data); // hash должен возвращать std::vector<uint8_t>
            mask.insert(mask.end(), hash.begin(), hash.end());
            ++counter;
        }
        mask.resize(maskLen);
        return mask;
    }
}