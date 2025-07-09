#pragma once

#include <random>
#include <chrono>
#include <vector>
#include <string>

std::random_device RANDOM_DEVICE;
std::mt19937 gen(RANDOM_DEVICE());

namespace rnd{
    template<class T = int>
    T randint(T a, T b){
        std::uniform_int_distribution<> dist(a,b);
        return dist(gen);
    }

    float uniform(float a, float b){
        std::uniform_real_distribution<> dist(a,b);
        return dist(gen);
    }

    template<class T>
    T choice(std::vector<T> vc){
        return vc[rnd::randint<int>(0, (int)vc.size()-1)];
    }

    template<class T>
    std::vector<T> vecrand(int size, T min_ampl, T max_ampl){
        std::vector<T> out;
        for(int i = 0; i < size; i++){
            if(min_ampl != max_ampl) out.push_back(rnd::uniform(min_ampl, max_ampl));
            else {out.push_back(0);}
        }
        return {out};
    }
};

namespace nw {
    using uid_t = long long;

    std::vector<uint8_t> getv(std::string str){
        std::vector<uint8_t> v;
        for (auto &ch: str){
            v.push_back(ch);
        }
        return v;
    }

    template<class T=std::chrono::milliseconds>
    int64_t timestamp(){
        using namespace std::chrono;
        return duration_cast<T>(system_clock::now().time_since_epoch()).count();
    }

    uid_t get_uid(){
        return rnd::randint<long long>((long long)10000, 92233720368LL);
    }
}