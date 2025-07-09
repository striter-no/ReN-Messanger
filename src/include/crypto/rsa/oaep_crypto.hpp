#pragma once
#include "keys.hpp"
#include "oaep.hpp"

namespace crypto::rsa {
    class CryptoF {

        rsa::PrivateKey *pr_key;
        rsa::PublicKey *pb_key;

        size_t k;
        size_t hLen;
        size_t max_msg_size;

        std::vector<uint8_t> _encrypt(const std::vector<uint8_t>& rdata) {
            
            if (rdata.size() > max_msg_size)
                throw std::invalid_argument("Message too long for OAEP");
            
            std::vector<uint8_t> encoded;
            mpz_class m;
            
            int attempts = 0;
            do {
                encoded = crypto::oaep::encode(rdata, k, {});
                m = crypto::bytes_to_mpz(encoded);
                attempts++;
                    
                if (attempts > 100)
                    throw std::runtime_error("Too many OAEP encoding attempts");
            } while (mpz_cmp(m.get_mpz_t(), pb_key->n.get_mpz_t()) >= 0);
            
            mpz_class c;
            mpz_powm(c.get_mpz_t(), m.get_mpz_t(), pb_key->e.get_mpz_t(), pb_key->n.get_mpz_t());
            
            return crypto::mpz_to_bytes(c, k);
        }

        std::vector<uint8_t> _decrypt(std::vector<uint8_t> &to_dec) {
            mpz_class c = crypto::bytes_to_mpz(to_dec);
            mpz_class m;
            mpz_powm(m.get_mpz_t(), c.get_mpz_t(), pr_key->d.get_mpz_t(), pr_key->n.get_mpz_t());
            
            std::vector<uint8_t> em = crypto::mpz_to_bytes(m, k);
            
            if (em.size() != k)
                throw std::runtime_error("Decrypted message size mismatch: " + std::to_string(em.size()) + " != " + std::to_string(k));
            
            if (em[0] != 0x00) {
                size_t m_bits = mpz_sizeinbase(m.get_mpz_t(), 2);
                
                if (m_bits < k * 8) {
                    
                    size_t actual_bytes = (m_bits + 7) / 8;
                    size_t leading_zeros = k - actual_bytes;
                    
                    std::vector<uint8_t> em_corrected(k, 0);
                    
                    size_t count = 0;
                    mpz_export(em_corrected.data() + leading_zeros, &count, 1, 1, 1, 0, m.get_mpz_t());
                    
                    em = em_corrected;
                }
            }
            
            if (em[0] != 0x00) {
                throw std::runtime_error("[_decrypt] leading zeros are missing in em. First byte: " + std::to_string((int)em[0]));
            }
            
            return crypto::oaep::decode(em, k, {});
        }

        public:

        std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) {
            // std::cout << " max encr size: " << max_msg_size << std::endl;
            if (data.size() > max_msg_size)
                throw std::invalid_argument("Message too long for OAEP");
            return _encrypt(data);
        }

        std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encrypted) {
            auto decr = _decrypt(const_cast<std::vector<uint8_t>&>(encrypted));
            return decr;
        }

        void set_key_pair(rsa::PrivateKey *pr_key, rsa::PublicKey *pb_key){
            this->pr_key = pr_key;
            this->pb_key = pb_key;

            k = pb_key->k();
            // std::cout << " k is: " << k << std::endl;
            hLen = 32; // SHA-256
            max_msg_size = k - 2 * hLen - 2;
        }

        CryptoF(){}
        ~CryptoF(){}
    };
}