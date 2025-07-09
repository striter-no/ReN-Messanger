#pragma once

#include <string>
#include <vector>

std::string next_simb(
    std::string data,
    int &prev_line,
    char simb
){
    int n = data.find_first_of(simb, prev_line + 1);

    if (n == std::string::npos){
        return std::string(
            data.begin() + prev_line,
            data.end()
        );
    }

    std::string d = std::string(
        data.begin() + prev_line,
        data.begin() + n
    );
    
    prev_line = n + 1;
    return d;
}

int count(
    std::string data,
    char find
){
    int i = 0;
    for (auto &ch: data){
        if (ch == find) i++;
    }
    return i;
}

template<class T>
bool in(std::vector<T> &v, T e){
    for (auto &el: v) {if (el == e) return true; }
    return false;
}