#include <ernet/cli_udp.hpp>

int main(){
    ernet::ernet_client cli;
    cli.create();
    int uid; std::cout << "enter uid: "; std::cin >> uid;
    cli.change_serv_ip({"127.0.0.1", 9000}, uid);
    
    auto r = nw::getv("hello");
    cli.send(r);
    cli.recv(r);
    std::cout << std::string{r.begin(), r.end()} << std::endl;
    
    cli.stop();
}