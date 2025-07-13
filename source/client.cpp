#define ENCRYPTED_RNET
#include <mirror/client.hpp>

int main(){
    FutureContext context;
    mirror::Client cli(
        {"127.0.0.1", 9000}
    );
    cli.set_retries(3);
    
    std::cout << "my uid: " << cli.uid() << std::endl;
    
    nw::uid_t cr_tunnel;
    nw::uid_t to_conn = -1;
    auto tun_found = context.concr(
        Future(context.context(), emptl, [&]() mutable {
            auto tuns = cli.discovery();
            if (tuns.size() == 0) return false;
            to_conn = tuns[0];
            cli.close_tunnel();
            return true;
        })
    );

    auto tun_cr = context.concr(
        cli.create_tunnel(cr_tunnel, context)
    );
    
    if (context.wait_first(0.1f) == tun_found){
        if (!cli.connect_tunnel(to_conn)){
            std::cerr << "Cannot connect to the tunnel\n";
            return -1;
        }
    }

    cli.send("Hello from " + std::to_string(cli.uid()));
    Future recvf(context.context(), emptl, [&](){
        auto resp = cli.recv();
        if (resp.has_value()){
            std::cout << resp.value() << std::endl;
            return true;
        }
        return false;
    });
    recvf.await(context, 0.1f);
}