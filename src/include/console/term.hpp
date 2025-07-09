#pragma once

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <csignal>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <iconv.h>

namespace cns {

    // Конвертер wchar_t → UTF-8 с использованием iconv
    std::string wideToUTF8(const std::wstring& wstr) {
        // std::cout << 1 << std::endl;
        iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
        if (cd == (iconv_t)-1) {
            throw std::runtime_error("iconv_open failed");
        }

        size_t inbytes = wstr.size() * sizeof(wchar_t);
        char* inbuf = (char*)wstr.data();
        std::string result(wstr.size() * 4, '\0');
        char* outbuf = result.data();
        size_t outbytes = result.size();

        if (iconv(cd, &inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
            iconv_close(cd);
            throw std::runtime_error("Conversion error");
        }

        if (result.size() - outbytes < 0){
            throw std::runtime_error("Conversion output error");
        }

        iconv_close(cd);
        result.resize(result.size() - outbytes);


        return result;
    }

    std::atomic<bool> ctrl_c{false};
    std::atomic<bool> ctrl_z{false};

    void ctrl_z_handler(int){
        ctrl_z.store(true);
    }

    void ctrl_c_handler(int){
        ctrl_c.store(true);
    }

    class Terminal {
        struct termios old_term, new_term;

        public:

        int curr_width(){
            struct winsize ws{};
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
            return ws.ws_col;
        }

        int curr_height(){
            struct winsize ws{};
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
            return ws.ws_row;
        }

        int num_bytes(){
            int bytes_avalaible;
            if (ioctl(STDIN_FILENO, FIONREAD, &bytes_avalaible) == -1){
                throw std::runtime_error("[term][c-f] ioctl failed");
            }
            return bytes_avalaible;
        }

        std::vector<char> inp_bytes(bool nonblock = false){
            std::vector<char> out; char c;
            // wait for input
            while (1){
                int s = read(STDIN_FILENO, &c, 1);
                if (s == -1 && errno == EAGAIN){
                    if (nonblock) return {};
                    continue; // waiting
                } else {
                    out.push_back(c);
                    break;
                }
            }

            while (1){
                int s = read(STDIN_FILENO,  &c, 1);

                // end of input
                if (s == -1 && errno == EAGAIN){ break; }
                out.push_back(c);
            }

            return out;
        }

        void setup(){
            tcgetattr(0, &old_term);
            new_term = old_term;
            new_term.c_lflag &= ~ICANON;
            new_term.c_lflag &= ~ECHO;
            tcsetattr(0, TCSANOW, &new_term);
            
            int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

            signal(SIGINT,  ctrl_c_handler);
            signal(SIGTSTP, ctrl_z_handler);
        }

        void rwrite(std::string data){
            while (1){
                ssize_t result = write(STDOUT_FILENO, data.c_str(), data.size());
                if (result == -1 && errno != EAGAIN){
                    throw std::runtime_error("Error while writing to console");
                } else if (result >= 0) {
                    break;
                }
            }
        }
        void rwrite(std::wstring data){
            std::string ndata = wideToUTF8(data);
            while (1){
                ssize_t result = write(STDOUT_FILENO, ndata.c_str(), ndata.size());
                if (result == -1 && errno != EAGAIN){
                    throw std::runtime_error("Error while writing to console " + std::to_string(errno));
                } else if (result >= 0){
                    break;
                }
            }
        }

        void hide_cursor(){rwrite("\033[?25l");}
        void show_cursor(){rwrite("\033[?25h");}

        void finish(){ tcsetattr(0, TCSANOW, &old_term); }

        bool is_ctrl_c_pressed(){ return ctrl_c; }
        bool is_ctrl_z_pressed(){ return ctrl_z; }
    };
}