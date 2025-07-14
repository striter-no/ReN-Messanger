#pragma once
#define RSA_KEY_LENGTH 4096
#define AES_KEY_LENGTH 256

#include <rawnet/cli_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>
#include <utility/log.hpp>

#include <crypto/aes/aes_crypto.hpp>
#include <crypto/rsa/oaep_crypto.hpp>
#include <map>

namespace ernet {
    void check_serv(std::string data){
        if (data == "ernet-datasize-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server sent an invalid data size (possible packet or protocol corruption)");
        } else if (data == "data-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not extract payload (size does not match content)");
        } else if (data == "ernet-uid-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not parse client uid (format error or corruption)");
        } else if (data == "ernet-rsa_pubkey-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not load client RSA public key (format error or corruption)");
        } else if (data == "ernet-aes_key_data-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server received corrupted AES key data (format error)");
        } else if (data == "ernet-rsa_data-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not decrypt RSA data (key or format error)");
        } else if (data == "ernet-aes_key-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not load AES key (format error or corruption)");
        } else if (data == "ernet-aes_data-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server could not decrypt AES data (key error or corruption)");
        } else if (data == "ernet-uplevel-failure"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server callback function failed");
        } else if (data == "ernet-encr-failure"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server failed to encrypt response to client (encryption error)");
        } else if (data == "ernet-control_sum-corrupted"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server detected data corruption (network failure, control sum mismatch)");
        } else if (data == "ernet-control_sum-proto"){
            throw std::runtime_error("<ernet> [udp/cli] fatal failure: server protocol mismatch (control sum positioning mismatch)");
        }
    }

    template<class T, class... Args>
    T do_while(std::function<T()> f, T v) {
        while (true) {
            T r = f(); 
            if (r != v)
                return r;
        }
    }

    template<class T, class... Args>
    T do_while(std::function<T()> f, std::vector<T> v) {
        while (true) {
            T r = f(); 
            if (std::find(v.begin(), v.end(), r) == v.end())
                return r;
        }
    }

    bool fail_safe_sr(
        nw::client_udp *cli,
        std::vector<uint8_t> req,
        std::vector<uint8_t> &resp,
        int retries = 1,
        float delay = 1.f
    ){
        std::vector<uint8_t> vresp;
        while (retries-- > 0 && vresp.size() == 0){
            cli->send(req);
            cli->recv(vresp);
            
            if (vresp.size() < 10) {
                LOG_M("[retry] received data too small, retrying...");
                vresp.clear();
                usleep(delay * 1'000'000);
                continue;
            }

            std::string resp_str(vresp.begin(), vresp.end());
            if (resp_str == "ernet-control_sum-corrupted") {
                LOG_M("[retry] server detected control sum mismatch, retrying...");
                vresp.clear();
                usleep(delay * 1'000'000);
                continue;
            }

            if (vresp.size() == 0) usleep(delay * 1'000'000);
        }
        resp = vresp;
        return resp.size() != 0;
    };

    class ernet_client {
        nw::uid_t uid;
        nw::client_udp rawcli;
        crypto::rsa::PrivateKey pr_key;
        crypto::rsa::PublicKey  pb_key;
        crypto::rsa::PublicKey  serv_pb;
        crypto::aes::Key        serv_aes_k;

        crypto::aes::CryptoF aes_cf;

        int __send(
            nw::client_udp *cli,
            std::vector<uint8_t> req,
            std::vector<uint8_t> &resp,
            int retries = 3,
            float delay = 1.f
        ){
            LOG_M("[>] sending " << req.size() << " bytes");
            
            auto csum = crypto::sha256::hexidigest(req) + " ";
            req.insert(req.begin(), csum.begin(), csum.end());
            LOG_M('"' << std::string{req.begin(), req.end()} << '"');

            auto s = nw::getv(std::to_string(req.size()) + ' ');
            req.insert(req.begin(), s.begin(), s.end());
            
            bool status = fail_safe_sr(cli, req, resp, retries, delay);
            if (!status) return 1;

            // Validate response size
            if (resp.size() < 10) {
                LOG_M("<ernet_cli> [__send] response too small for parsing");
                return 2;
            }

            int tracker = 0;
            std::string strdata{resp.begin(), resp.end()};
            
            // Parse size safely
            try {
                size_t space_pos = strdata.find(' ');
                if (space_pos == std::string::npos) {
                    std::cerr << "<ernet_cli> [__send] no space found for size parsing" << std::endl;
                    return 3;
                }
                
                std::string size_str = strdata.substr(0, space_pos);
                ssize_t r_strdata_size = std::stoull(size_str);
                tracker = space_pos + 1;
                
                // Validate size
                if (r_strdata_size <= 0 || r_strdata_size > 1024 * 1024) {
                    std::cerr << "<ernet_cli> [__send] invalid data size: " << r_strdata_size << std::endl;
                    return 4;
                }
                
                // Check if we have enough data
                if (tracker + r_strdata_size > resp.size()) {
                    std::cerr << "<ernet_cli> [__send] data size mismatch. Expected: " << r_strdata_size << ", Available: " << (resp.size() - tracker) << std::endl;
                    return 5;
                }
                
                LOG_M("[<] recevied " << r_strdata_size << " bytes ");
                resp = {
                    resp.begin() + tracker,
                    resp.begin() + tracker + r_strdata_size
                };

                tracker = 0;
                std::string control_sum = next_simb({resp.begin(), resp.end()}, tracker, ' ');
                resp = {
                    resp.begin() + tracker,
                    resp.end()
                };

                LOG_M("recv hash: " << crypto::sha256::hexidigest(resp));

                if (crypto::sha256::hexidigest(resp) != control_sum){
                    std::cerr << "<ernet_cli> [__send] control sums mismatch: " + control_sum + " != " + crypto::sha256::hexidigest(resp);
                    return 6;
                }

                check_serv({resp.begin(), resp.end()});
                LOG_M(" (" << resp.size() << ')');
                
            } catch (const std::exception &ex) {
                std::cerr << "<ernet_cli> [__send] error parsing response size: " << ex.what() << std::endl;
                return 7;
            }

            return 0;
        }

        void ern_hello(bool recreate_uid = true){
            std::string str_resp;
            std::vector<uint8_t> resp;
            int track;
            if (recreate_uid) uid = nw::get_uid();
            LOG_M("gen uid: " << uid);

            // 1. RSA
            crypto::rsa::CryptoF rsa_cf;
            auto pair = crypto::rsa::genPair(RSA_KEY_LENGTH);
            pr_key = pair.first;
            pb_key = pair.second;

            LOG_M("rsa keys, gen pair");
            LOG_M("pubhash: " << crypto::sha256::hexidigest(pb_key.saveToString()));

            resp.clear();
            if (0 != do_while<int>(
                [&](){ return __send(
                    &rawcli,
                    nw::getv(std::to_string(uid) + " " + pb_key.saveToString()),
                    resp
                ); },
                {5, 6, 7}
            ))
                throw std::runtime_error("<ernet_cli> [ern_hello/rsa] server down");
            
            str_resp = {resp.begin(), resp.end()};
            track = 0;
            if (next_simb(str_resp, track, ' ') == "rsa-reg-ok"){
                LOG_M("pubkey: " << std::string(str_resp.begin() + track, str_resp.end()));

                serv_pb = crypto::rsa::PublicKey::loadFromString({
                    str_resp.begin() + track,
                    str_resp.end()
                });
                LOG_M("got server pubkey");
            } else {
                throw std::runtime_error("<ernet_cli> [ern_hello/rsa-resp] cannot register my rsa pubkey");
            }
            
            LOG_M("server pubhash: " << crypto::sha256::hexidigest(serv_pb.saveToString()));
            rsa_cf.set_key_pair(
                &pr_key, &serv_pb
            );

            // 2. AES
            LOG_M("aes step ahead");
            serv_aes_k.generate(AES_KEY_LENGTH);
            aes_cf.set_key(serv_aes_k);

            std::vector<uint8_t> aes_data = nw::getv(std::to_string(uid) + ' ');
            LOG_M("key legnth: " << serv_aes_k.save_to_string().size());
            auto encr = rsa_cf.encrypt(serv_aes_k.get_data());
            aes_data.insert(
                aes_data.end(), 
                encr.begin(), encr.end()
            );
            LOG_M("encryped size: " << encr.size());
            LOG_M("sending aes key over rsa: " << aes_data.size() << " bytes");
            LOG_M(std::string{aes_data.begin(), aes_data.end()});
            LOG_M("aes key hash is: " << crypto::sha256::hexidigest(serv_aes_k.get_data()));
            
            resp.clear();
            if (0 != do_while<int>(
                [&](){ return __send(
                    &rawcli,
                    aes_data,
                    resp
                ); },
                {5, 6, 7}
            ))
                throw std::runtime_error("<ernet_cli> [ern_hello/aes] server down");
            
            resp = aes_cf.decrypt(resp);
            str_resp = {resp.begin(), resp.end()};
            track = 0;
            if (str_resp != "aes-reg-ok")
                throw std::runtime_error("<ernet_cli> [ern_hello/aes-resp] cannot register my aes key");
            LOG_M("everything is ok");

            // 3. Hello-echo

            LOG_M("echo-hello sending");
            auto req = nw::getv(std::to_string(uid) + ' ');
            auto hello_vec = aes_cf.encrypt(nw::getv("hello"));
            req.insert(req.end(), hello_vec.begin(), hello_vec.end());

            resp.clear();
            if (0 != do_while<int>(
                [&](){ return __send(
                    &rawcli,
                    req,
                    resp
                ); },
                {5, 6, 7}
            ))
                throw std::runtime_error("<ernet_cli> [ern_hello/hello] server down");
            
            resp = aes_cf.decrypt(resp);
            str_resp = {resp.begin(), resp.end()};
            if (str_resp != "echo hello"){
                throw std::runtime_error("<ernet_cli> [ern_hello/echo] server rejected ern hello: " + str_resp);
            }

            LOG_M("everything is ok");
            LOG_M("ERN handshake happend");
        }

        public:

        nw::address get_serv_addr(){
            return rawcli.get_serv_addr();
        }
        void set_timeout(int seconds){
            rawcli.set_timeout(seconds);
        }
        
        int send(std::vector<uint8_t> &data){
            LOG_M("[<] sending: " << std::string(data.begin(), data.end()));

            auto encr = aes_cf.encrypt(data);
            auto uid_v = nw::getv(std::to_string(uid) + " ");
            encr.insert(encr.begin(), uid_v.begin(), uid_v.end());

            auto csum = crypto::sha256::hexidigest(encr) + " ";
            encr.insert(encr.begin(), csum.begin(), csum.end());

            LOG_M("[^] sent: " << encr.size() << " bytes");
            auto s = nw::getv(std::to_string(encr.size()) + ' ');
            encr.insert(encr.begin(), s.begin(), s.end());

            return rawcli.send(encr);
        }

        int recv(std::vector<uint8_t> &data, bool clear_buffer = false, int max_buff_size = 4096){
            int n = rawcli.recv(data, clear_buffer, max_buff_size);
            if (n <= 0) return n;
            
            // Validate data size
            if (data.size() < 10) {
                LOG_M("<ernet_cli> [recv] received data too small for parsing");
                return -5;
            }

            // Parse size safely
            try {
                std::string strdata{data.begin(), data.end()};
                size_t space_pos = strdata.find(' ');
                if (space_pos == std::string::npos) {
                    LOG_M("<ernet_cli> [recv] no space found for size parsing");
                    return -4;
                }
                
                std::string size_str = strdata.substr(0, space_pos);
                ssize_t r_strdata_size = std::stoull(size_str);
                int tracker = space_pos + 1;
                
                // Validate size
                if (r_strdata_size <= 0 || r_strdata_size > 1024 * 1024) {
                    LOG_M("<ernet_cli> [recv] invalid data size: " << r_strdata_size);
                    return -3;
                }
                
                // Check if we have enough data
                if (tracker + r_strdata_size > data.size()) {
                    LOG_M("<ernet_cli> [recv] data size mismatch. Expected: " << r_strdata_size << ", Available: " << (data.size() - tracker));
                    return -2;
                }

                data = {
                    data.begin() + tracker,
                    data.begin() + tracker + r_strdata_size
                };
                LOG_M("[>] got: " << data.size() << " bytes");
                
                tracker = 0;
                std::string control_sum = next_simb({data.begin(), data.end()}, tracker, ' ');
                data = {
                    data.begin() + tracker,
                    data.end()
                };
                
                LOG_M("recv hash: " << crypto::sha256::hexidigest(data));

                if (crypto::sha256::hexidigest(data) != control_sum){
                    LOG_M("<ernet_cli> [recv] control sums mismatch: " << control_sum << " != " << crypto::sha256::hexidigest(data));
                    return -1;
                }
                
                check_serv({data.begin(), data.end()});
                
                data = aes_cf.decrypt(data);
                LOG_M("[^] " << std::string(data.begin(), data.end()));
                
            } catch (const std::exception &ex) {
                LOG_M("<ernet_cli> [recv] error parsing data size: " << ex.what());
                return -99;
            }
            
            return n;
        }
        
        void change_serv_ip(nw::address addr, nw::uid_t uid = -1){
            rawcli.change_serv_ip(addr);
            
            if (uid != -1)
                this->uid = uid;

            for (int i = 0; i < 3; i++){
                try {
                    ern_hello(uid == -1);
                    break;
                } catch (const std::exception &ex){
                    std::cerr << "<ernet_cli> [change_serv_ip/ern_hello] " << i << " failed ern_hello: " << ex.what() << std::endl;
                }
            }
        }

        void create(){rawcli.create();}
        void stop(){rawcli.stop();}

        ~ernet_client(){}
        ernet_client(){}
    };
}

// 7f c3 08 70 d1 c2 83 d6 4a 2a ea ab af 3d 25 b5 fb 62 f9 f1 1a d6 bc 45 5f fe 81 75 39 36 36 33
// 7f c3 08 70 d1 c2 83 d6 4a 2a ea ab af 3d 25 b5 fb 62 f9 f1 1a d6 bc 45 5f fe 81 75 00 5a 11 83

// b4 9a de 7a 22 50 ec 7b fe 30 7b f9 2c 6f 63 f6 d9 12 f1 c0 25 f8 be c3 34 0b 1c 71 e2 6d b3 45
// b4 9a de 7a 22 50 ec 7b fe 30 7b f9 2c 6f 63 f6 d9 12 f1 c0 25 f8 be c3 34 0b 1c 71 e2 6d b3 45

// af f0 ec 02 90 5e a1 5a 36 1c bd ac 18 2e 2e 97 83 43 50 bb 71 81 cf e3 85 af 98 2b fd 91 ed 29
// dataaf f0 ec 02 90 5e a1 5a 36 1c bd ac 18 2e 2e 97 83 43 50 bb 71 81 cf e3 85 af 98 2b fd 91 ed 29
