#include <stable/client.hpp>

int main(){
    stable::stable_cli cli;
    cli.create();
    int uid; std::cout << "enter uid: "; std::cin >> uid;
    cli.change_serv_ip({"127.0.0.1", 9000}, uid);
    
    for (int i = 0; i < 100; i++){
        std::string prefix = std::to_string(rnd::randint(0, 10000));
        std::string msg = "hello " + std::to_string(uid) + " " + prefix;
        std::cout << "sending -> " << msg << std::endl;

        auto r = nw::getv(msg);
        cli.send(r);
        cli.recv(r);
        std::string got = std::string{r.begin(), r.end()};

        std::cout << "recv -> " << got << std::endl;
        if ("echo: " + msg != got){
            std::cout << "error on " << i << " iteration" << std::endl;
            break;
        }
        std::cout << std::endl;
    }

    cli.stop();
}