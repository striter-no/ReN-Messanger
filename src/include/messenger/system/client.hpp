#pragma once
#define ENCRYPTED_RNET
#include <mirror/client.hpp>

#include <crypto/rsa/oaep_crypto.hpp>
#include <crypto/aes/aes_crypto.hpp>
#include <crypto/sha256.hpp>

namespace msg {
    using namespace crypto;

    class ReNClient {
        FutureContext context;
        mirror::Client mcli;

        nw::uid_t curr_cuid;

        nw::uid_t my_tunnel = -1;
        nw::uid_t other_tunnel = -1;
        nw::uid_t curr_tunnel = -1;

        rsa::CryptoF    rs_cf;
        aes::CryptoF    ae_encf, ae_decf;

        rsa::PrivateKey pr_key;
        rsa::PublicKey  pb_key, other_pb;
        aes::Key        aes_key, other_aes;

        public:

        void send_msg(std::string msg){
            if (curr_tunnel == -1) 
                throw std::runtime_error("<msg/ReNClient> [send_msg] Not connected to any tunnel");
            if (msg.size() > 3000)
                throw std::runtime_error("<msg/ReNClient> [send_msg] Message size is too big: " + std::to_string(msg.size()) + " > " + std::to_string(3000));  

            // mcli.send(msg, curr_tunnel);
            auto encr = ae_encf.encrypt({msg.begin(), msg.end()});
            mcli.send(encr, curr_tunnel);
        }

        std::string check_msg(){
            if (curr_tunnel == -1) 
                throw std::runtime_error("<msg/ReNClient> [check_msg] Not connected to any tunnel");
            
            std::vector<uint8_t> data;
            if (!mcli.recv(data, curr_tunnel)) return "";

            // return std::string{data.begin(), data.end()};
            auto decr = ae_decf.decrypt(data);
            return {decr.begin(), decr.end()};
        }

        void crypto_hello(){
            if (curr_tunnel == -1) 
                throw std::runtime_error("<msg/ReNClient> [crypto_hello] Not connected to any tunnel");
            
            std::vector<uint8_t> encr;
            mcli.send(pb_key.saveToString(), curr_tunnel);
            
            Future(context.context(), emptl, [&]() -> bool {
                std::string resp;
                if (mcli.recv(resp, curr_tunnel)){
                    other_pb = rsa::PublicKey::loadFromString(resp);
                    return true;
                }
                return false;
            }).await(context, 0.1f);

            rs_cf.set_key_pair(&pr_key, &other_pb);

            try{
                encr = rs_cf.encrypt(aes_key.get_data());
            } catch (const std::exception &ex){
                throw std::runtime_error("{re} <msg/ReNClient> [crypto_hello:rsa(encr: aes-key)] crypto-errror: \n\t" + std::string{ex.what()});
            }
            mcli.send(encr, curr_tunnel);

            Future(context.context(), emptl, [&]() -> bool {
                std::vector<uint8_t> data;
                if (mcli.recv(data, curr_tunnel)){
                    data = rs_cf.decrypt(data);
                    other_aes = aes::Key(data);
                    return true;
                }
                return false;
            }).await(context, 0.1f);

            std::cout << "aes (de)key hash: " << sha256::hexidigest(other_aes.get_data()) << std::endl;
            ae_decf.set_key(other_aes);

            try{
                encr = ae_encf.encrypt(nw::getv("hello"));
            } catch (const std::exception &ex){
                throw std::runtime_error("{re} <msg/ReNClient> [crypto_hello:aes(encr)] crypto-errror: \n\t" + std::string{ex.what()});
            }
            mcli.send(encr, curr_tunnel);

            std::string echo;
            Future(context.context(), emptl, [&]() -> bool {
                std::vector<uint8_t> data;
                if (mcli.recv(data, curr_tunnel)){
                    std::vector<uint8_t> ld;

                    try {
                        ld = ae_decf.decrypt(data);
                    } catch (const std::exception &ex){
                        throw std::runtime_error("{re} <msg/ReNClient> [crypto_hello:future(echo-recv)] crypto-errror: \n\t" + std::string{ex.what()});
                    }

                    echo = {ld.begin(), ld.end()};
                    return true;
                }
                return false;
            }).await(context, 0.1f);

            if (echo != "hello"){
                throw std::runtime_error("<msg/ReNClient> [crypto_hello] echo is not `hello`. Failed to establish connection");
            }
        }

        void connect(nw::uid_t other_tunnel){
            this->other_tunnel = other_tunnel;
            mcli.create_tunnel(my_tunnel, my_tunnel);

            auto ot = context.concr(mcli.wait_exist(other_tunnel, context, [&](){
                mcli.close_tunnel(my_tunnel);
                mcli.connect_tunnel(other_tunnel);
            }));
            auto mt = context.concr(mcli.wait_exist(my_tunnel, context));
            auto winner = context.wait_first(0.1f);

            if (winner == mt){ // My tunnel is current
                curr_tunnel = my_tunnel;
            } else if (winner == ot) { // Others tunnel is current
                curr_tunnel = other_tunnel;
            }
        }

        void regme(std::string &info){
            if (info.empty()){
                curr_cuid = nw::get_uid();
                my_tunnel = nw::get_uid();
                
                info = dump();
            } else {
                auto words = split(info);
                curr_cuid = std::stoll(words[0]);
                my_tunnel = std::stoll(words[1]);
                other_tunnel = std::stoll(words[2]);
                // if (words.size() > 1){
                //     for (int i = 1; i < words.size(); i++){
                //         my_tunnels.push_back(
                //             std::stoll(words[i])
                //         );
                //     }
                // }
            }
        }

        std::string dump(){
            std::string o;
            o += std::to_string(curr_cuid) + ' ';
            o += std::to_string(my_tunnel) + ' ';
            o += std::to_string(other_tunnel) + ' ';
            // for (auto &t: my_tunnels)
            //     o += std::to_string(t) + '\n';
            o.pop_back();
            return o;
        }

        nw::uid_t get_mtun(){ return my_tunnel; }
        nw::uid_t get_ctun(){ return curr_tunnel; }
        void swap_mirrors(nw::address adr){
            mcli.swap_serv(adr);
        }

        ReNClient(nw::address adr): mcli(adr) {
            auto p = rsa::genPair(RSA_KEY_LENGTH);
            pr_key = p.first;
            pb_key = p.second;

            aes_key.generate(AES_KEY_LENGTH);
            ae_encf.set_key(aes_key);

            std::cout << "aes (en)key hash: " << sha256::hexidigest(aes_key.get_data()) << std::endl;
        }
        ~ReNClient(){}
    };
}