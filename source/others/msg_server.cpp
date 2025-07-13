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

// 56 46 e2 a1 b4 e7 a3 00 d9 04 10 70 d1 fe 82 4e d6 a8 d8 12 a1 5b a3 f4 39 9b 55 90 15 e3 a0 9f
// 56 46 e2 a1 b4 e7 a3 00 00 00 00 00 00 66 36 33 33 61 63 37 37 21 00 00 00 00 00 00 00 d6 9d 34