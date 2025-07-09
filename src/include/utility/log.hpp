#pragma once

#include <string>
#include <fstream>

bool first_log = true;

void log_to_file(
    std::string filename,
    std::string message
){
    if (first_log){
        first_log = false;
        std::ofstream ofs(filename, std::ios::trunc);
        ofs << message << std::endl;
    }
    else{
        std::ofstream ofs(filename, std::ios::app);
        ofs << message << std::endl;
    }
}