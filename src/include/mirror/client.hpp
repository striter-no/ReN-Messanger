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
#include <algorithm>
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

        std::vector<std::pair<nw::uid_t, nw::uid_t>> c_tunnels;
        std::vector<nw::uid_t> connected_to;
        int retries = 1;

        void check_subproto(std::string resp){
            if (resp == "unreg-msg-type"){
                throw std::runtime_error("<subproto> Unregistred message type");
            }
        }

        void upd_peer(nw::uid_t &tun_uid, nw::uid_t &up){
            for (auto &[t, p]: c_tunnels){
                if (t == tun_uid){
                    p = up;
                }
            }
        }

        public:

        std::vector<nw::uid_t> conn_tunnels(){ 
            std::vector<nw::uid_t> o;
            for (auto &t: connected_to){
                o.push_back(t);
            }
            return o;
        }

        std::vector<nw::uid_t> opened_tunnels(){ 
            std::vector<nw::uid_t> o;
            for (auto &[t, p]: c_tunnels){
                o.push_back(t);
            }
            return o;
        }

        nw::uid_t conn_uid(nw::uid_t &tun_uid){ 
            std::vector<nw::uid_t> o;
            for (auto &[t, p]: c_tunnels){
                if (t == tun_uid) return p;
            }
            return -1;
        }

        void create_tunnel(nw::uid_t &tun_uid, nw::uid_t force_uid = -1){
            NetRequest req;
            auto tuid = proto.create_tunnel(req, force_uid);
            c_tunnels.push_back({tuid, -1});

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [create-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} != "ok") throw std::runtime_error("<mirror> [create-tunnel] server disagree: " + std::string{resp.begin(), resp.end()});
        }

        Future wait_tunnel(nw::uid_t tuid, FutureContext &fcontext, std::function<void()> onchecktrue = [](){}){
            return {fcontext.context(), emptl, [this, tuid, onchecktrue]() mutable {
                NetRequest req;
                proto.check_tunnel(tuid, req);

                std::vector<uint8_t> resp;
                if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                    throw std::runtime_error("<mirror> [future(check_tunnel)] server down");
                check_subproto(std::string{resp.begin(), resp.end()});
                
                auto res = proto.check_tunnel(resp);
                if (res.has_value()){
                    onchecktrue();
                    upd_peer(tuid, res.value());
                    return true;
                }

                return false;
            }};
        }

        Future wait_exist(nw::uid_t tuid, FutureContext &fcontext, std::function<void()> onchecktrue = [](){}){
            return {fcontext.context(), emptl, [this, tuid, onchecktrue]() mutable {
                NetRequest req;
                proto.check_exist(tuid, req);

                std::vector<uint8_t> resp;
                if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                    throw std::runtime_error("<mirror> [future(wait_exist)] server down");
                check_subproto(std::string{resp.begin(), resp.end()});
                
                if (proto.check_exist(resp)){
                    onchecktrue();
                    return true;
                }

                return false;
            }};
        }

        bool connect_tunnel(nw::uid_t uid){
            NetRequest req;
            proto.connect_tunnel(uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [conn-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} == "ok"){
                connected_to.push_back(uid);
                return true;
            }

            return false;
        }
        
        bool close_tunnel(nw::uid_t uid){
            if (in(connected_to, uid)){
                connected_to.erase(std::remove_if(
                    connected_to.begin(), connected_to.end(),
                    [uid](nw::uid_t tuid){
                        return tuid == uid;
                    }
                ));
            } else if(in(keys(c_tunnels), uid)){
                c_tunnels.erase(std::remove_if(
                    c_tunnels.begin(), c_tunnels.end(),
                    [uid](std::pair<nw::uid_t, nw::uid_t> p){
                        return p.first == uid;
                    }
                ));
            } else {
                std::cerr << "<cerr:mirror> [close-tunnel] tunnel with uid " << uid << " does not exists" << std::endl;
                return false;
            }


            NetRequest req;
            proto.close_tunnel(uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [close-tunnel] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }

        bool send(const std::vector<uint8_t>& data, nw::uid_t uid){
            if (!in(connected_to, uid) && !in(keys(c_tunnels), uid)){
                std::cerr << "<cerr:mirror> [send] tunnel with uid " << uid << " does not exists" << std::endl;
                return false;
            }
            
            NetRequest req;
            proto.send(data, uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [send] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }
        bool send(const std::string& data, nw::uid_t uid){
            if (!in(connected_to, uid) && !in(keys(c_tunnels), uid)){
                std::cerr << "<cerr:mirror> [send] tunnel with uid " << uid << " does not exists" << std::endl;
                return false;
            }

            NetRequest req;
            proto.send(data, uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [send] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            return std::string{resp.begin(), resp.end()} == "ok";
        }

        bool recv(std::string &data, nw::uid_t uid){
            if (!in(connected_to, uid) && !in(keys(c_tunnels), uid)){
                std::cerr << "<cerr:mirror> [send] tunnel with uid " << uid << " does not exists" << std::endl;
                return false;
            }

            NetRequest req;
            proto.recv(uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [recv] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} == "no-data") return false;
            data = proto.recv(resp);
            return true;
        }

        bool recv(std::vector<uint8_t> &data, nw::uid_t uid){
            if (!in(connected_to, uid) && !in(keys(c_tunnels), uid)){
                std::cerr << "<cerr:mirror> [send] tunnel with uid " << uid << " does not exists" << std::endl;
                return false;
            }

            NetRequest req;
            proto.recv(uid, req);

            std::vector<uint8_t> resp;
            if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
                throw std::runtime_error("<mirror> [recv] server down");
            check_subproto(std::string{resp.begin(), resp.end()});
            
            if (std::string{resp.begin(), resp.end()} == "no-data") return false;
            data = proto.brecv(resp);
            return true;
        }
        
        // std::vector<nw::uid_t> discovery(){
        //     NetRequest req;
        //     proto.discovery(req);
        //     std::vector<uint8_t> resp;
        //     if(!fail_safe_sr(&raw_cli, req.get(), resp, retries))
        //         throw std::runtime_error("<mirror> [discovery] server down");
        //     check_subproto(std::string{resp.begin(), resp.end()});
        //     return proto.discovery(resp, uid);
        // }

        nw::uid_t get_uid(){
            return proto.token();
        }

        void set_retries(int n){
            retries = n;
        }

        void swap_serv(nw::address serv_address){
            raw_cli.change_serv_ip(serv_address);
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