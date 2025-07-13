#pragma once

#include "proto-alpha/cli.hpp"
#include "utility/strvec.hpp"
#include "utility/sfuture.hpp"

#include <rawnet/base_u.hpp>

#ifndef ENCRYPTED_RNET
#include <rawnet/cli_udp.hpp>
using base_client_t = nw::client_udp;
#else
#include <ernet/cli_udp.hpp>
using base_client_t = ernet::ernet_client;
#endif

#include <iostream>
#include <map>

namespace mirror {

    bool fail_safe_sr(
        base_client_t *cli,
        std::vector<uint8_t> &&req,
        std::vector<uint8_t> &resp,
        int retries = 1,
        float delay = 1.f
    ){
        std::vector<uint8_t> vresp;
        auto vreq = std::move(req);
        while (retries-- > 0 && vresp.size() == 0){
            cli->send(vreq);
            if (cli->recv(vresp) == -1){
                usleep(delay * 0.5 * 1'000'000);
                continue;
            }
            
            if (vresp.size() == 0) usleep(delay * 1'000'000);
        }
        resp = std::move(vresp);
        return resp.size() != 0;
    };

    class Client {

        base_client_t raw_cli;
        ClientProto proto;

        nw::uid_t c_tunnel = -1;
        nw::uid_t peer_uid = -1;
        int retries = 1;

        void check_subproto(std::string resp){
            if (resp == "unreg-msg-type"){
                throw std::runtime_error("<subproto> Unregistred message type");
            }
        }

        public:

        nw::uid_t curr_tunnel(){ return c_tunnel; }
        nw::uid_t conn_uid(){ return peer_uid; }

        Future create_tunnel(nw::uid_t &tun_uid, FutureContext &fcontext){
            NetRequest req;
            auto tuid = proto.create_tunnel(req);
            tun_uid = tuid;
            c_tunnel = tuid;

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [create-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} != "ok") throw std::runtime_error("<mirror> [create-tunnel] server disagree: " + std::string{resp.begin(), resp.end()});

            Future f(fcontext.context(), emptl, [&]() mutable {
                NetRequest req;
                proto.check_tunnel(c_tunnel, req);

                std::vector<uint8_t> resp;
                if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                    throw std::runtime_error("<mirror> [create-tunnel:future(check_tunnel)] server down");
                check_subproto(std::string{resp.begin(), resp.end()});
                
                auto res = proto.check_tunnel(resp);
                if (res.has_value()){
                    peer_uid = res.value();
                    return true;
                }

                return false;
            });

            return f;
        }

        bool connect_tunnel(nw::uid_t uid){
            NetRequest req;
            proto.connect_tunnel(uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [conn-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            c_tunnel = uid;
            return std::string{resp.begin(), resp.end()} == "ok";
        }
        
        bool close_tunnel(){
            if (c_tunnel == -1){
                std::cerr << "<cerr:mirror> [close-tunnel] cannot close tunnel with uid == -1";
                return false;
            }

            NetRequest req;
            proto.close_tunnel(c_tunnel, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [close-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }

        bool send(const std::vector<uint8_t>& data){
            NetRequest req;
            proto.send(data, c_tunnel, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [send] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }
        bool send(const std::string& data){
            NetRequest req;
            proto.send(data, c_tunnel, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [send] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }
        std::optional<std::string> recv(){
            NetRequest req;
            proto.recv(c_tunnel, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [recv] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} == "no-data") return std::nullopt;
            return proto.recv(resp);
        }
        
        std::vector<nw::uid_t> discovery(){
            NetRequest req;
            proto.discovery(req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [discovery] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            return proto.discovery(resp, c_tunnel);
        }

        nw::uid_t uid(){
            return proto.token();
        }

        void set_retries(int n){
            retries = n;
        }

        Client(nw::address serv_address){
            raw_cli.create();
            raw_cli.change_serv_ip(serv_address);
            raw_cli.set_timeout(3);
            
            proto.token_reg();
            proto.change_mirror(serv_address);
        }

        Client(){}
        ~Client(){}
    };
};