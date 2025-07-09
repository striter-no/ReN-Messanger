#pragma once

#include <console/keyboard.hpp>
#include <console/term.hpp>
#include <console/colors.hpp>

#include <iostream>
#include <fstream>

void log_to_file(const std::string& msg) {
    std::ofstream log("log.txt", std::ios::app);
    log << msg << std::endl;
}

namespace cns {

    struct Pixel {
        std::wstring bg = L"";
        std::wstring clr  = L"";
        std::wstring txt  = L"";
        std::pair<std::wstring, std::wstring> style = {L"", L""};

        Pixel(std::wstring clr, std::wstring txt, std::wstring bg = L""):
            clr(clr), txt(txt), bg(bg) 
        {}

        Pixel(wchar_t txt):
            txt(std::wstring{} + txt)
        {}
    };

    class Console {
        Terminal *term;
        std::vector<std::vector<Pixel>> displ;

        int s_width, s_height;

        public:

        int width(){return s_width;}
        int height(){return s_height;}

        void clear(){
            displ = std::vector<std::vector<Pixel>>(
                s_height,
                std::vector<Pixel>(
                    s_width,
                    {L"", L" "}
                )
            );
        }
        
        Pixel &gpix(int x, int y){
            return displ[y][x];
        }

        void pix(int x, int y, Pixel pix){ 
            if (x >= 0 && x < s_width && y >= 0 && y < s_height) {
                displ[y][x] = pix;
            }
        }

        void draw(){
            std::wstring buff = L"\033[H";

            for (auto &line: displ) {
                for (auto &c: line) {
                    buff += c.style.first + c.bg + c.clr + c.txt + (c.bg.size() != 0 ? clrs::wcv(clrs::Back.reset) : L"") + c.style.second;
                }
                buff += L"\n";
            }

            buff += clrs::wcv(clrs::Style.allreset.first);
            term->rwrite(buff);
        }

        Console(
            Terminal *term
        ): term(term), 
           s_width(term->curr_width() - 1), 
           s_height(term->curr_height() - 1) 
        {}

        Console(){}
        ~Console(){}
    };
}