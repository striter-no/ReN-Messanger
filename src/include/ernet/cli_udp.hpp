#pragma once
#define RSA_KEY_LENGTH 4096
#define AES_KEY_LENGTH 256

#include <rawnet/cli_udp.hpp>
#include <rawnet/base_u.hpp>
#include <utility/strvec.hpp>

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

        bool __send(
            nw::client_udp *cli,
            std::vector<uint8_t> req,
            std::vector<uint8_t> &resp,
            int retries = 3,
            float delay = 1.f
        ){
            std::cout << "[>] sending " << req.size() << " bytes" << std::endl;
            auto s = nw::getv(std::to_string(req.size()) + ' ');
            req.insert(req.begin(), s.begin(), s.end());
            bool status = fail_safe_sr(cli, req, resp, retries, delay);
            if (!status) return false;

            int tracker = 0;
            std::string strdata{resp.begin(), resp.end()};
            ssize_t r_strdata_size = std::stoull(next_simb(strdata, tracker, ' '));
            
            std::cout << "[<] recevied " << r_strdata_size << " bytes ";
            resp = {
                resp.begin() + tracker,
                resp.begin() + tracker + r_strdata_size
            };
            std::cout << "recv hash: " << crypto::sha256::hexidigest(resp) << std::endl;
            check_serv({resp.begin(), resp.end()});
            std::cout << " (" << resp.size() << ')' << std::endl;

            return true;
        }

        void ern_hello(bool recreate_uid = true){
            std::string str_resp;
            std::vector<uint8_t> resp;
            int track;
            if (recreate_uid) uid = nw::get_uid();
            std::cout << "gen uid: " << uid << std::endl;

            // 1. RSA
            crypto::rsa::CryptoF rsa_cf;
            auto pair = crypto::rsa::genPair(RSA_KEY_LENGTH);
            pr_key = pair.first;
            pb_key = pair.second;

            std::cout << "rsa keys, gen pair" << std::endl;
            std::cout << "pubhash: " << crypto::sha256::hexidigest(pb_key.saveToString()) << std::endl;

            resp.clear();
            if (!__send(&rawcli, nw::getv(std::to_string(uid) + " " + pb_key.saveToString()), resp))
                throw std::runtime_error("<ernet_cli> [ern_hello/rsa] server down");
            
            str_resp = {resp.begin(), resp.end()};
            track = 0;
            if (next_simb(str_resp, track, ' ') == "rsa-reg-ok"){
                std::cout << "pubkey: " << std::string(str_resp.begin() + track, str_resp.end()) << std::endl;

                serv_pb = crypto::rsa::PublicKey::loadFromString({
                    str_resp.begin() + track,
                    str_resp.end()
                });
                std::cout << "got server pubkey" << std::endl;
            } else {
                throw std::runtime_error("<ernet_cli> [ern_hello/rsa-resp] cannot register my rsa pubkey");
            }
            
            std::cout << "server pubhash: " << crypto::sha256::hexidigest(serv_pb.saveToString()) << std::endl;
            rsa_cf.set_key_pair(
                &pr_key, &serv_pb
            );

            // 2. AES
            std::cout << "aes step ahead" << std::endl;
            serv_aes_k.generate(AES_KEY_LENGTH);
            aes_cf.set_key(serv_aes_k);

            std::vector<uint8_t> aes_data = nw::getv(std::to_string(uid) + ' ');
            std::cout << "key legnth: " << serv_aes_k.save_to_string().size() << std::endl;
            auto encr = rsa_cf.encrypt(serv_aes_k.get_data());
            aes_data.insert(
                aes_data.end(), 
                encr.begin(), encr.end()
            );
            std::cout << "encryped size: " << encr.size() << std::endl;
            std::cout << "sending aes key over rsa: " << aes_data.size() << " bytes" << std::endl;
            std::cout << std::string{aes_data.begin(), aes_data.end()} << std::endl;
            std::cout << "aes key hash is: " << crypto::sha256::hexidigest(serv_aes_k.get_data()) << std::endl;
            
            resp.clear();
            if (!__send(&rawcli, aes_data, resp))
                throw std::runtime_error("<ernet_cli> [ern_hello/aes] server down");
            
            resp = aes_cf.decrypt(resp);
            str_resp = {resp.begin(), resp.end()};
            track = 0;
            if (str_resp != "aes-reg-ok")
                throw std::runtime_error("<ernet_cli> [ern_hello/aes-resp] cannot register my aes key");
            std::cout << "everything is ok" << std::endl;

            // 3. Hello-echo

            std::cout << "echo-hello sending" << std::endl;
            auto req = nw::getv(std::to_string(uid) + ' ');
            auto hello_vec = aes_cf.encrypt(nw::getv("hello"));
            req.insert(req.end(), hello_vec.begin(), hello_vec.end());

            resp.clear();
            if (!__send(&rawcli, req, resp))
                throw std::runtime_error("<ernet_cli> [ern_hello/hello] server down");
            
            resp = aes_cf.decrypt(resp);
            str_resp = {resp.begin(), resp.end()};
            if (str_resp != "echo hello"){
                throw std::runtime_error("<ernet_cli> [ern_hello/echo] server rejected ern hello: " + str_resp);
            }

            std::cout << "everything is ok" << std::endl;
            std::cout << "ERN handshake happend" << std::endl;
        }

        public:

        nw::address get_serv_addr(){
            return rawcli.get_serv_addr();
        }
        void set_timeout(int seconds){
            rawcli.set_timeout(seconds);
        }
        
        int send(std::vector<uint8_t> &data){
            std::cout << "[<] sending: " << std::string(data.begin(), data.end()) << std::endl;

            auto encr = aes_cf.encrypt(data);
            auto uid_v = nw::getv(std::to_string(uid) + " ");
            encr.insert(encr.begin(), uid_v.begin(), uid_v.end());

            std::cout << "[^] sent: " << encr.size() << " bytes" << std::endl;
            auto s = nw::getv(std::to_string(encr.size()) + ' ');
            encr.insert(encr.begin(), s.begin(), s.end());

            return rawcli.send(encr);
        }

        int recv(std::vector<uint8_t> &data, bool clear_buffer = false, int max_buff_size = 4096){
            int n = rawcli.recv(data, clear_buffer, max_buff_size);
            if (n <= 0) return n;
            

            int tracker = 0;
            std::string strdata{data.begin(), data.end()};
            ssize_t r_strdata_size = std::stoull(next_simb(strdata, tracker, ' '));

            data = {
                data.begin() + tracker,
                data.begin() + tracker + r_strdata_size
            };
            std::cout << "[>] got: " << data.size() << " bytes" << std::endl;
            check_serv({data.begin(), data.end()});
            
            data = aes_cf.decrypt(data);
            std::cout << "[^] " << std::string(data.begin(), data.end()) << std::endl;
            return n;
        }
        
        void change_serv_ip(nw::address addr, nw::uid_t uid = -1){
            rawcli.change_serv_ip(addr);
            
            if (uid != -1)
                this->uid = uid;

            ern_hello(uid == -1);
        }

        void create(){rawcli.create();}
        void stop(){rawcli.stop();}

        ~ernet_client(){}
        ernet_client(){}
    };
}