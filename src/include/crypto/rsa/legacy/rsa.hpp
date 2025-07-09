#pragma once
#include "keys.hpp"
#include "sha256.hpp"
#include <vector>
#include <string>
#include <random>
#include <cstring>
#include <utility/log.hpp>
#include <iomanip>
#include "mpz_math.hpp"

namespace crypto::rsa {
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data, const rsa::PublicKey& public_key) {
        size_t k = public_key.k();
        if (data.size() > k)
            throw std::runtime_error("RSA: message too long");
        // Преобразуем данные в число
        mpz_class m = crypto::bytes_to_mpz(data);
        if (m >= public_key.n)
            throw std::runtime_error("RSA: message representative out of range");
        // c = m^e mod n
        mpz_class c;
        mpz_powm(c.get_mpz_t(), m.get_mpz_t(), public_key.e.get_mpz_t(), public_key.n.get_mpz_t());
        // Преобразуем результат в байты длиной k
        return crypto::mpz_to_bytes(c, k);
    }

    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data, const rsa::PrivateKey& private_key) {
        size_t k = private_key.k();
        if (data.size() != k)
            throw std::runtime_error("RSA: ciphertext has wrong length");
        // Преобразуем данные в число
        mpz_class c = crypto::bytes_to_mpz(data);
        if (c >= private_key.n)
            throw std::runtime_error("RSA: ciphertext representative out of range");
        // m = c^d mod n
        mpz_class m;
        mpz_powm(m.get_mpz_t(), c.get_mpz_t(), private_key.d.get_mpz_t(), private_key.n.get_mpz_t());
        // Преобразуем результат в байты длиной k
        return crypto::mpz_to_bytes(m, k);
    }
}