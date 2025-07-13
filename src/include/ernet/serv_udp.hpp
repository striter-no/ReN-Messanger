#pragma once
#define RSA_KEY_LENGTH 4096
#define AES_KEY_LENGTH 256

#include <rawnet/serv_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>
#include <utility/log.hpp>

#include <crypto/aes/aes_crypto.hpp>
#include <crypto/rsa/oaep_crypto.hpp>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>

namespace ernet {
    class ernet_server {
        crypto::rsa::PrivateKey serv_key;
        crypto::rsa::PublicKey  serv_pb;
        
        std::map<nw::uid_t, crypto::rsa::PublicKey> cli_publics;
        std::shared_mutex cli_publics_mutex;
        
        std::map<nw::uid_t, crypto::aes::Key>       cli_aes_keys;
        std::shared_mutex cli_aes_keys_mutex;
        
        std::map<nw::uid_t, bool>                   cli_echo_hello;
        std::shared_mutex cli_echo_hello_mutex;

        std::map<nw::uid_t, int64_t> cli_last_active;
        std::shared_mutex cli_last_active_mutex;

        std::thread cleanup_thread;
        std::atomic<bool> stop_cleanup{false};

        nw::server_udp raws;
        std::function<void(
            nw::address,
            std::vector<uint8_t>&, 
            std::vector<uint8_t>&
        )> encr_cb;

        std::vector<uint8_t> rsa_getv(std::string &&data, crypto::rsa::PublicKey &encr_key) {
            crypto::rsa::CryptoF crypt;
            crypt.set_key_pair(&serv_key, &encr_key);
            
            return crypt.encrypt({data.begin(), data.end()}); 
        };

        std::vector<uint8_t> aes_getv(std::string &&data, crypto::aes::Key &key) {
            crypto::aes::CryptoF crypt;
            crypt.set_key(key);

            return crypt.encrypt({data.begin(), data.end()});
        }

        void cleanup_inactive_clients(int timeout_sec = 300) {
            int64_t now = nw::timestamp();
            std::vector<nw::uid_t> to_del;

            {
                std::shared_lock<std::shared_mutex> lock(cli_last_active_mutex);
                for (const auto& [uid, last] : cli_last_active) {
                    if (now - last > timeout_sec) {
                        to_del.push_back(uid);
                    }
                }
            }

            for (auto uid : to_del) {
                { std::lock_guard<std::shared_mutex> lock(cli_publics_mutex);
                  cli_publics.erase(uid);
                }
                { std::lock_guard<std::shared_mutex> lock(cli_aes_keys_mutex);
                  cli_aes_keys.erase(uid);
                }
                { std::lock_guard<std::shared_mutex> lock(cli_echo_hello_mutex);
                  cli_echo_hello.erase(uid);
                }
                { std::lock_guard<std::shared_mutex> lock(cli_last_active_mutex);
                  cli_last_active.erase(uid);
                }
            }
        }

        void deflect(nw::address cliaddr, std::vector<uint8_t> data, std::vector<uint8_t>& ans, int uid){
            int tracker = 0;
            std::string strdata = std::string(data.begin(), data.end());
            std::vector<uint8_t> first_data;

            ssize_t r_strdata_size;
            try {
                r_strdata_size = std::stoull(next_simb(strdata, tracker, ' '));
            } catch (const std::exception &ex) {
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: protocol mismatch: first word has to be R_STRDATA_SIZE\n\t" << ex.what());
                ans = nw::getv("ernet-datasize-corrupted");
                return;
            }
            
            // (uid == 1 ? cli1: cli2) << "[>] receveing " << r_strdata_size << " bytes" << std::endl;
            try {
                strdata = std::string(
                    strdata.begin() + tracker,
                    strdata.begin() + tracker + r_strdata_size
                );
                data = std::vector<uint8_t>{
                    data.begin() + tracker,
                    data.begin() + tracker + r_strdata_size
                };
                first_data = std::vector<uint8_t>{
                    data.begin(),
                    data.begin() + tracker + r_strdata_size
                };
            } catch (const std::exception &ex) {
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: data corruption: there is no data for this R_STRDATA_SIZE\n\t" << ex.what());
                ans = nw::getv("data-corrupted");
                return;
            }

            tracker = 0;
            
            std::string control_sum;
            try{
                control_sum = next_simb(strdata, tracker, ' ');
            } catch (const std::exception &ex) {
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: protocol mismatch: second word has to be CONTROL_SUM\n\t" << ex.what());
                ans = nw::getv("ernet-control_sum-proto");
                return;
            }

            strdata = std::string( strdata.begin() + tracker, strdata.end() );
            data = std::vector<uint8_t>{ data.begin() + tracker, data.end() };
            tracker = 0;

            LOG_M("data is:\n" << strdata);

            if (control_sum != crypto::sha256::hexidigest(data)){
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: control sum mismatch:\n\t" << control_sum << " != " << crypto::sha256::hexidigest(data));
                ans = nw::getv("ernet-control_sum-corrupted");
                return;
            }

            try{
                uid = std::stoll(next_simb(strdata, tracker, ' '));
            } catch (const std::exception &ex) {
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: protocol mismatch: third word has to be ERNET_UID\n\t" << ex.what());
                ans = nw::getv("ernet-uid-corrupted");
                return;
            }

            {
                std::lock_guard<std::shared_mutex> lock(cli_last_active_mutex);
                cli_last_active[uid] = nw::timestamp();
            }

            // 1. RSA handshake
            // Сначала пробуем shared_lock для чтения
            {
                std::shared_lock<std::shared_mutex> lock(cli_publics_mutex);
                if (cli_publics.find(uid) == cli_publics.end()) {
                    // Нужно добавить — выходим из shared_lock
                    lock.unlock();
                    std::lock_guard<std::shared_mutex> wlock(cli_publics_mutex);
                    crypto::rsa::PublicKey key;
                    try{
                        key = crypto::rsa::PublicKey::loadFromString(std::string{
                            data.begin() + tracker, data.end()
                        });
                    } catch (const std::exception &ex){
                        LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: rsa public key: key seems to be corrupted\n\t" << ex.what());
                        ans = nw::getv("ernet-rsa_pubkey-corrupted");
                        return;
                    }
                    
                    cli_publics[uid] = key;
                    ans = nw::getv("rsa-reg-ok " + serv_pb.saveToString());
                    return;
                }
            }

            // 2. AES handshake
            {
                std::shared_lock<std::shared_mutex> lock(cli_aes_keys_mutex);
                if (cli_aes_keys.find(uid) == cli_aes_keys.end()) {
                    lock.unlock();
                    std::lock_guard<std::shared_mutex> wlock(cli_aes_keys_mutex);
                    crypto::rsa::CryptoF rcrypt;
                    rcrypt.set_key_pair(&serv_key, &cli_publics[uid]);
                    try {
                        data = {data.begin() + tracker, data.end()};
                    } catch (const std::exception &ex){
                        LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: aes key: key seems to be corrupted\n\t" << ex.what());
                        ans = nw::getv("ernet-aes_key_data-corrupted");
                        return;
                    }
                    try {
                        data = rcrypt.decrypt(data);
                    } catch (const std::exception &ex) {
                        LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: rsa encr data/key: data/key seems to be corrupted. Check server pubkey/privkey/client pubkey\n\t" << ex.what());
                        ans = nw::getv("ernet-rsa_data-corrupted");
                        return;
                    }

                    // (uid == 1 ? cli1: cli2) << "aes key hash is: " << crypto::sha256::hexidigest(data) << std::endl;
                    // (uid == 1 ? cli1: cli2).flush();

                    try {
                        cli_aes_keys[uid] = crypto::aes::Key(data);
                    } catch (const std::exception &ex) {
                        LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: aes key: key seems to be corrupted\n\t" << ex.what());
                        ans = nw::getv("ernet-aes_key-corrupted");
                        return;
                    }

                    ans = aes_getv("aes-reg-ok", cli_aes_keys[uid]);
                    return;
                }
            }

            crypto::aes::CryptoF acrypt;
            {
                std::shared_lock<std::shared_mutex> lock(cli_aes_keys_mutex);
                acrypt.set_key(cli_aes_keys[uid]);
            }

            try {
                data = acrypt.decrypt({
                    data.begin() + tracker, data.end()
                });
            } catch (const std::exception &ex) {
                LOG_M("<ernet> [udp/serv] -> " << cliaddr.str() << " -> fatal error: aes data decryption: data seems to be corrupted\n\t" << ex.what());
                ans = nw::getv("ernet-aes_data-corrupted");
                return;
            }

            // 3. Hello-echo
            {
                std::shared_lock<std::shared_mutex> lock(cli_echo_hello_mutex);
                if (cli_echo_hello.find(uid) == cli_echo_hello.end()) {
                    lock.unlock();
                    std::lock_guard<std::shared_mutex> wlock(cli_echo_hello_mutex);
                    if (std::string{data.begin(), data.end()} != "hello"){
                        ans = acrypt.encrypt(nw::getv("rejected"));
                        cli_echo_hello[uid] = false;
                    } else {
                        cli_echo_hello[uid] = true;
                        ans = acrypt.encrypt(nw::getv("echo hello"));
                    }
                    return;
                }
                if (!cli_echo_hello[uid]){
                    ans = acrypt.encrypt(nw::getv("rejected"));
                    return;
                }
            }

            std::vector<uint8_t> plain_ans;
            try{
                encr_cb(cliaddr, data, plain_ans);
            } catch (const std::exception &ex){
                LOG_M("<ernet/uplevel> [udp/serv] -> " << cliaddr.str() << " -> fatal error: callback failure\n\t" << ex.what());
                ans = nw::getv("ernet-uplevel-failure");
                return;
            }

            // std::cerr << "test of stderr 1" << std::endl;
            try{
                ans = acrypt.encrypt(plain_ans);
                
            } catch (const std::exception &ex){
                LOG_M("<ernet/uplevel/encr> [udp/serv] -> " << cliaddr.str() << " -> fatal error: encryption failure: cannot encrypt callback data\n\t" << ex.what());
                ans = nw::getv("ernet-encr-failure");
                return;
            }
        }

        public:

        void on_msg_cli (
            std::function<void(
                nw::address,
                std::vector<uint8_t>&, 
                std::vector<uint8_t>&
            )> v
        ){ encr_cb = v; }

        
        void run(bool detached = false, bool clear_buffer = false, int max_buff_size = 4096){
            raws.on_msg_cli(
                [&](nw::address a, std::vector<uint8_t>& b, std::vector<uint8_t>& c){
                    std::vector<uint8_t> ans;
                    
                    int uid;
                    deflect(a, b, ans, uid);
                    LOG_M("send " << ans.size() << " bytes, hash: " << crypto::sha256::hexidigest(ans) << "\n-> data: " << crypto::bytes_to_hex(ans));
                    
                    auto csum = nw::getv(crypto::sha256::hexidigest(ans) + ' ');
                    ans.insert(ans.begin(), csum.begin(), csum.end());

                    auto size = nw::getv(std::to_string(ans.size()) + ' ');
                    ans.insert(ans.begin(), size.begin(), size.end());

                    
                    c = ans;
                }
            );
            raws.run(detached, clear_buffer, max_buff_size);
        }
        
        nw::address get_addr(){return raws.get_addr();}
        int get_threads_num(){return raws.get_threads_num();}

        void set_threads(int n){ raws.set_threads(n); }
        void create(nw::address addr){ 
            raws.create(addr);
            auto p = crypto::rsa::genPair(RSA_KEY_LENGTH);
            serv_key = p.first;
            serv_pb = p.second;
            
            stop_cleanup = false;
            cleanup_thread = std::thread([this]{
                while (!stop_cleanup) {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    cleanup_inactive_clients(300); // 5 минут таймаут
                }
            });
            // (uid == 1 ? cli1: cli2) << "pubhash: " << crypto::sha256::hexidigest(serv_pb.saveToString()) << std::endl;
        }
        void stop(){ 
            stop_cleanup = true;
            if (cleanup_thread.joinable()) cleanup_thread.join();

            raws.stop(); 
        }

        ernet_server(){}
        ~ernet_server(){}
    };
}