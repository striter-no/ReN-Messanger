#pragma once
#include <string>

namespace clrs{
    struct foregroundDataset{
        const std::string reset = "\033[39m";
        const std::string black = "\033[30m";
        const std::string red = "\033[31m";
        const std::string green = "\033[32m";
        const std::string yellow = "\033[33m";
        const std::string blue = "\033[34m";
        const std::string purple = "\033[35m";
        const std::string cyan = "\033[36m";
        const std::string white = "\033[37m";
    } Fore;

    struct backgroundDataset{
        const std::string reset = "\033[49m";
        const std::string black = "\033[40m";
        const std::string red = "\033[41m";
        const std::string green = "\033[42m";
        const std::string yellow = "\033[43m";
        const std::string blue = "\033[44m";
        const std::string purple = "\033[45m";
        const std::string cyan = "\033[46m";
        const std::string white = "\033[47m";
    } Back;

    struct lightForegroundDataset{
        const std::string reset = "\033[39m";
        const std::string black = "\033[90m";
        const std::string red = "\033[91m";
        const std::string green = "\033[92m";
        const std::string yellow = "\033[93m";
        const std::string blue = "\033[94m";
        const std::string purple = "\033[95m";
        const std::string cyan = "\033[96m";
        const std::string gray = "\033[97m";
    } LightFore;

    struct lightBackgroundDataset{
        const std::string reset = "\033[49m";
        const std::string black = "\033[100m";
        const std::string red = "\033[101m";
        const std::string green = "\033[102m";
        const std::string yellow = "\033[103m";
        const std::string blue = "\033[104m";
        const std::string purple = "\033[105m";
        const std::string cyan = "\033[106m";
        const std::string gray = "\033[107m";
    } LightBack;

    /*
    "\033[0m" // Reset all text attributes to default
    "\033[1m" // Bold on
    "\033[2m" // Faint off
    "\033[3m" // Italic on
    "\033[4m" // Underline on
    "\033[5m" // Slow blink on
    "\033[6m" // Rapid blink on
    "\033[7m" // Reverse video on
    "\033[8m" // Conceal on
    "\033[9m" // Crossed-out on
    "\033[22m" // Bold off
    "\033[23m" // Italic off
    "\033[24m" // Underline off
    "\033[25m" // Blink off
    "\033[27m" // Reverse video off
    "\033[28m" // Conceal off
    "\033[29m" // Crossed-out off
    */

    // Effect | Reset
    struct style{
        const std::pair<std::string, std::string> bold      = {"\033[1m", "\033[22m"};
        const std::pair<std::string, std::string> dim       = {"\033[2m", "\033[22m"};
        const std::pair<std::string, std::string> cursive   = {"\033[3m", "\033[23m"};
        const std::pair<std::string, std::string> underline = {"\033[4m", "\033[24m"};
        const std::pair<std::string, std::string> blink     = {"\033[5m", "\033[25m"};
        const std::pair<std::string, std::string> inverse   = {"\033[7m", "\033[27m"};
        const std::pair<std::string, std::string> strike    = {"\033[9m", "\033[29m"};

        const std::pair<std::string, std::string> allreset  = {"\033[0m", ""};

        std::pair<std::string, std::string> conc(
            std::pair<std::string, std::string> s1,
            std::pair<std::string, std::string> s2
        ){
            return {
                s1.first + s2.first,
                s2.second + s1.second
            };
        }
    } Style;

    std::string rgb_fore(int r, int g, int b){
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    std::string rgb_back(int r, int g, int b){
        return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    std::string fore_to_back(std::string fore){
        return "\033[48;2;" + fore.substr(7);
    }

    std::string back_to_fore(std::string back){
        return "\033[38;2;" + back.substr(7);
    }

    std::wstring fore_to_back(std::wstring fore){
        return L"\033[48;2;" + fore.substr(7);
    }

    std::wstring back_to_fore(std::wstring back){
        return L"\033[38;2;" + back.substr(7);
    }

    std::wstring wcv(std::string s){return std::wstring(s.begin(), s.end());};
    std::pair<std::wstring, std::wstring> wcv(std::pair<std::string, std::string> s){
        return {std::wstring(s.first.begin(), s.first.end()), std::wstring(s.second.begin(), s.second.end())};
    };
}