#define ENCRYPTED_RNET
// #define NETDEBUG

#include <mirror/server.hpp>
#include <mirror/proto-alpha/serv.hpp>

int main(){
    mirror::Server serv({"127.0.0.1", 9000});
    serv.set_threads(5);

    mirror::ServerProto proto;
    serv.new_msg("open-tunnel", {}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.open_tunnel(a, b, c, d);
    });
    serv.new_msg("close-tunnel", {}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.close_tunnel(a, b, c, d);
    });
    serv.new_msg("conn-tunnel", {}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.conn_tunnel(a, b, c, d);
    });
    serv.new_msg("check-tunnel", {}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.check_tunnel(a, b, c, d);
    });
    serv.new_msg("send", {{false}}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.send_bytes(a, b, c, d);
    });
    serv.new_msg("recv", {{true}}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.recv_bytes(a, b, c, d);
    });
    serv.new_msg("discovery", {{true}}, [&](nw::address& a, nw::uid_t& b, nw::uid_t& c, std::vector<std::vector<uint8_t>> d){
        return proto.discovery(a, b, c, d);
    });

    serv.run();
    while (true) {;}
}