#include <chrono>
#include <ernet/serv_udp.hpp>

using namespace std::chrono_literals;

int main(){
    
    ernet::ernet_server serv;
    serv.create({"127.0.0.1", 9000});
    serv.set_threads(2);
    serv.on_msg_cli([&](
        nw::address addr, std::vector<uint8_t> &d, std::vector<uint8_t> &a
    ){
        std::cout << "from " << addr.str() << ": " << std::string(d.begin(), d.end()) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(rnd::randint<int>(1, 10)));
        a = nw::getv("echo ok");
    });

    serv.run(true);
    while (true) {;}
}