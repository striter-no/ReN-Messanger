#pragma once

#include <map>
#include <functional>
#include <rawnet/base_u.hpp>
#include "strvec.hpp"

struct Future {
    nw::uid_t uid;
    bool done = false;

    std::function<void()> callback;
    std::function<bool()> await_function;

    template<class FF>
    void await(FF &future_fabric, float latency = 0.f){
        if (future_fabric.futures.find(uid) == future_fabric.futures.end()){
            throw std::runtime_error("Future error: future with UID " + std::to_string(uid) + " is not exists in future fabric");
        }

        while (!future_fabric.futures[uid].done && !await_function()){
            if (latency > 0.f) usleep(latency * 1'000'000);
        }
        future_fabric.futures[uid].done = true;

        if (this->callback) 
            this->callback();
    }

    Future(
        std::map<nw::uid_t, Future> &futures, 
        std::function<void()> callback = [](){},
        std::function<bool()> await_fun = [](){return false;}
    ): uid(nw::get_uid()), 
        callback(callback), 
        await_function(await_fun) 
    {
        futures[uid] = *this;
    }

    Future(){}
};

class FutureContext{
    friend Future;
    std::map<nw::uid_t, Future> futures;
    std::vector<Future> concurents;

    public:

    nw::uid_t concr(Future &&fut){
        concurents.push_back(fut);
        return fut.uid;
    }

    nw::uid_t wait_first(float latency = 0){
        std::vector<nw::uid_t> stopped;
        while (true){
            for (auto &f: concurents){
                if (in(stopped, f.uid)) continue;

                try{
                    if (f.await_function()) {
                        auto uid = f.uid;
                        concurents.clear();
                        return uid;
                    }
                } catch (const std::exception &ex){
                    std::cerr << "<sfuture> [wait_first] future " << f.uid << " stopped: " << ex.what() << std::endl;
                    stopped.push_back(f.uid);
                }
            }
            if (latency > 0)
                usleep(latency * 1'000'000);
        }
    }

    std::map<nw::uid_t, Future> &context(){
        return futures;
    }
};