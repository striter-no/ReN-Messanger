// #define NETDEBUG
#include <messenger/system/client.hpp>

int main(){
    std::string info;
    nw::uid_t other_tun;

    msg::ReNClient cli({"127.0.0.1", 9000});
    cli.regme(info);
    
    std::cout << "my tunuid: " << cli.get_mtun() << std::endl;
    std::cout << "enter other's tunuid: ";
    std::cin  >> other_tun;
    
    std::string fy = "";
    for (int i = 0; i < 300; i++){
        // fy += std::to_string(i) + " ";
        fy += "00000 ";
    }

    cli.connect(other_tun);
    cli.crypto_hello();

    for (int i = 0; i < 10; i++){
        cli.send_msg(std::to_string(i) + " Hello from origtun: " + std::to_string(cli.get_mtun()) + " fy: " + fy);

        std::string resp;
        while ((resp = cli.check_msg()).empty()){
            ;
        }

        std::cout << '"' << resp << '"' << std::endl;
    }
}