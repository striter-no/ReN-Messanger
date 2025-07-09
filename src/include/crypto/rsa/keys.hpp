#pragma once

#include <gmpxx.h>
#include <random>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace crypto::rsa {

    int RANDOM_SEED = time(NULL) * 1000;
    int RANDOM_SEED_COUNTER = 0;

    struct PrivateKey {
        mpz_class d, n;

        std::string saveToString() const {
            std::stringstream str;
            str << d.get_str(16) << std::endl;
            str << n.get_str(16);
            return str.str();
        }

        static PrivateKey loadFromString(std::string data){
            std::istringstream iss(data);
            std::string d_str, n_str;
            std::getline(iss, d_str);
            std::getline(iss, n_str);
            PrivateKey key;
            key.d.set_str(d_str, 16);
            key.n.set_str(n_str, 16);
            return key;
        }

        void saveToFile(const std::string& filename) const {
            std::ofstream ofs(filename);
            ofs << d.get_str(16) << std::endl;
            ofs << n.get_str(16);
        }

        static PrivateKey loadFromFile(const std::string& filename) {
            std::ifstream ifs(filename);
            std::string d_str, n_str;
            std::getline(ifs, d_str);
            std::getline(ifs, n_str);
            PrivateKey key;
            key.d.set_str(d_str, 16);
            key.n.set_str(n_str, 16);
            return key;
        }

        size_t k() const {
            return (mpz_sizeinbase(n.get_mpz_t(), 2) + 7) / 8;
        }

        size_t max_message_size() const {
            size_t key_size = k();
            size_t hLen = 32; // SHA-256
            if (key_size < 2 * hLen + 2) return 0;
            return key_size - 2 * hLen - 2;
        }
    };

    struct PublicKey {
        mpz_class e, n;

        std::string saveToString() const {
            std::stringstream str;
            str << e.get_str(16) << std::endl;
            str << n.get_str(16);
            return str.str();
        }

        static PublicKey loadFromString(std::string data){
            std::istringstream iss(data);
            std::string e_str, n_str;
            std::getline(iss, e_str);
            std::getline(iss, n_str);
            PublicKey key;
            key.e.set_str(e_str, 16);
            key.n.set_str(n_str, 16);
            return key;
        }

        void saveToFile(const std::string& filename) const {
            std::ofstream ofs(filename);
            ofs << e.get_str(16) << std::endl;
            ofs << n.get_str(16);
        }

        static PublicKey loadFromFile(const std::string& filename) {
            std::ifstream ifs(filename);
            std::string e_str, n_str;
            std::getline(ifs, e_str);
            std::getline(ifs, n_str);
            PublicKey key;
            key.e.set_str(e_str, 16);
            key.n.set_str(n_str, 16);
            return key;
        }

        size_t k() const {
            return (mpz_sizeinbase(n.get_mpz_t(), 2) + 7) / 8;
        }

        size_t max_message_size() const {
            size_t key_size = k();
            size_t hLen = 32; // SHA-256
            if (key_size < 2 * hLen + 2) return 0;
            return key_size - 2 * hLen - 2;
        }

    };

    // Генерация большого простого числа
    mpz_class generate_large_prime(int bits) {
        gmp_randclass rng(gmp_randinit_mt);
        rng.seed(RANDOM_SEED + (RANDOM_SEED_COUNTER++));
        mpz_class prime;
        while (true) {
            prime = rng.get_z_bits(bits);
            // Устанавливаем старший бит, чтобы число было точно bits-битным
            mpz_setbit(prime.get_mpz_t(), bits - 1);
            mpz_nextprime(prime.get_mpz_t(), prime.get_mpz_t());
            // Проверка на простоту (опционально: mpz_probab_prime_p)
            if (mpz_probab_prime_p(prime.get_mpz_t(), 25) > 0)
                break;
        }
        return prime;
    }

    // Расширенный алгоритм Евклида для поиска обратного по модулю
    mpz_class modinv(const mpz_class& a, const mpz_class& m) {
        mpz_class g, x, y;
        mpz_gcdext(g.get_mpz_t(), x.get_mpz_t(), y.get_mpz_t(), a.get_mpz_t(), m.get_mpz_t());
        if (g != 1) throw std::runtime_error("No modular inverse");
        return (x % m + m) % m;
    }

    std::pair<PrivateKey, PublicKey> genPair(int bits = 1024) {
        while (true) {
            mpz_class p = generate_large_prime(bits / 2);
            mpz_class q;
            
            do {
                q = generate_large_prime(bits / 2);
            } while (q == p);
            mpz_class n = p * q;
            
            // Проверяем, что n имеет правильный размер в битах
            size_t n_bits = mpz_sizeinbase(n.get_mpz_t(), 2);
            if (n_bits < bits - 1 || n_bits > bits) continue;
            
            mpz_class phi = (p - 1) * (q - 1);
            mpz_class e = 65537;
            
            if (mpz_gcd_ui(nullptr, phi.get_mpz_t(), e.get_ui()) != 1) {
                e = 3;
                while (mpz_gcd_ui(nullptr, phi.get_mpz_t(), e.get_ui()) != 1) {
                    e += 2;
                }
            }

            mpz_class d = modinv(e, phi);
            // Проверка!
            mpz_class check = (e * d) % phi;
            if (check != 1) throw std::runtime_error("e and d are not inverses!");
            return { {d, n}, {e, n} };
        }
    }

}