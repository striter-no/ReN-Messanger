#include <chrono>
#include <stable/server.hpp>

using namespace std::chrono_literals;

int main(){
    
    stable::stable_server serv;
    serv.create({"127.0.0.1", 9000});
    serv.set_threads(2);
    
    serv.on_msg_cli([&](
        nw::address addr, std::vector<uint8_t> &d, std::vector<uint8_t> &a
    ){
        std::cout << "from " << addr.str() << ": " << std::string(d.begin(), d.end()) << std::endl;
        
        int v = rnd::randint(100, 2000);
        std::cout << addr.str() << " is sleeping for " << v << " ms" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(v));
        
        a = nw::getv("echo: " + std::string{d.begin(), d.end()});
        std::cout << "answering to " << addr.str() << " -> " + std::string{a.begin(), a.end()} << std::endl;
    });

    serv.run(true);
    while (true) {;}
}