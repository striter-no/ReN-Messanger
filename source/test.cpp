#include <crypto/rsa/oaep_crypto.hpp>
#include <crypto/aes/aes_crypto.hpp>
#include <utility/strvec.hpp>
#include <rawnet/base_u.hpp>
#include <iostream>

void test_crypto() {
    auto [priv, pub] = crypto::rsa::genPair(4096);
    
    std::cout << "Key size: " << pub.k() << " bytes" << std::endl;
    std::cout << "Key str size: " << pub.saveToString().size() << "/" << priv.saveToString().size() << std::endl;

    std::cout << "Max message size: " << pub.max_message_size() << " bytes" << std::endl;
    
    crypto::rsa::CryptoF crypto;
    crypto.set_key_pair(&priv, &pub);
    
    std::string test_msg = "Hello, World!";
    std::vector<uint8_t> data{test_msg.begin(), test_msg.end()};
    std::cout << "Original: " << test_msg << " size: " << test_msg.size() << std::endl;
    
    try {
        auto encrypted = crypto.encrypt(data);
        std::cout << "Encrypted size: " << encrypted.size() << std::endl;
        
        auto decrypted = crypto.decrypt(encrypted);
        std::cout << "Decrypted: " << std::string(decrypted.begin(), decrypted.end()) << std::endl;
        
        if (data == decrypted) {
            std::cout << "SUCCESS!" << std::endl;
        } else {
            std::cout << "FAILED!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void test_aes(){
    crypto::aes::Key key;
    key.generate(256);
    
    crypto::aes::CryptoF crypto;
    crypto.set_key(&key);
    
    std::string message = "Hello aes!";
    auto data = std::vector<uint8_t>{message.begin(), message.end()};
    
    auto encrypted = crypto.encrypt(data);
    std::cout << "Encrypted size: " << encrypted.size() << std::endl;

    auto decrypted = crypto.decrypt(encrypted);
    std::cout << "Decrypted: " << std::string(decrypted.begin(), decrypted.end()) << std::endl;

    if (decrypted == data){
        std::cout << "SUCCESS!" << std::endl;
    } else {
        std::cout << "FAILED!" << std::endl;
    }
}

void test_ernet_prefix_aes() {
    std::cout << "\n[UNIT] test_ernet_prefix_aes" << std::endl;
    // 1. Генерируем AES ключ
    crypto::aes::Key key;
    key.generate(256);
    crypto::aes::CryptoF aes;
    aes.set_key(&key);

    // 2. Оригинальные данные
    std::string orig = "test_message_123";
    std::vector<uint8_t> data{orig.begin(), orig.end()};

    // 3. Шифруем
    auto encrypted = aes.encrypt(data);

    // 4. Добавляем префикс размера (как в ernet_client)
    auto prefix = nw::getv(std::to_string(encrypted.size()) + ' ');
    encrypted.insert(encrypted.begin(), prefix.begin(), prefix.end());

    // 5. На стороне сервера: снимаем префикс
    int tracker = 0;
    std::string strdata{encrypted.begin(), encrypted.end()};
    ssize_t r_strdata_size = std::stoull(next_simb(strdata, tracker, ' '));
    std::vector<uint8_t> encrypted_no_prefix = {
        encrypted.begin() + tracker,
        encrypted.begin() + tracker + r_strdata_size
    };

    // 6. Дешифруем
    auto decrypted = aes.decrypt(encrypted_no_prefix);
    std::string result{decrypted.begin(), decrypted.end()};

    std::cout << "original: " << orig << std::endl;
    std::cout << "decrypted: " << result << std::endl;
    if (orig == result) {
        std::cout << "SUCCESS!" << std::endl;
    } else {
        std::cout << "FAILED!" << std::endl;
    }
}

int main() {
    // test_crypto();
    // test_aes();
    test_ernet_prefix_aes();
    return 0;
}