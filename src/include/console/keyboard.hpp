#pragma once

#include <vector>
#include <string>
#include <map>
#include <codecvt>
#include <locale>

namespace cns {

    bool starts_with(std::string from, std::string d){
        return std::string(from.begin(), from.begin() + d.size()) == d;
    }

    template<class T>
    bool in(std::vector<T> &v, T el) {
        for (auto &e: v) {if (e == el) return true;}
        return false;
    }

    template<class K, class V>
    std::vector<K> keys(std::map<K, V> m){
        std::vector<K> out;
        for (const auto &[k, v]: m) out.push_back(k);
        return out;
    }

    
    struct Key {
        std::string escs;
        std::wstring name;
        int int_code = 0;

        bool shifted = false;
        bool ctrled  = false;
        bool alted   = false;
    };
    
    struct KeyState {
        bool pressed = false;
        bool shifted = false;
        bool ctrled  = false;
        bool alted   = false;

        KeyState(bool p, bool s, bool c, bool a):
            pressed(p), shifted(s), ctrled(c), alted(a)
        {}

        KeyState(Key k):
            pressed(true), shifted(k.shifted), ctrled(k.ctrled), alted(k.alted)
        {}

        KeyState(){}
    };
    
    const int offset = 255;

    const std::vector<Key> keymap {
        {"\x1d", L"ctrl + ]", offset + 43},
        {"\x1b", L"ctrl + [", offset + 42},
        {"\x09", L"tab", offset + 41},
        {"\x0a", L"nl", offset + 40},
        {"\x7f", L"bs", offset + 39},
        {"\x08", L"ctrl + bs", offset + 38},
        {"\x19", L"ctrl + y", offset + 37},
        {"\x12", L"ctrl + r", offset + 36},
        {"\x14", L"ctrl + t", offset + 35},
        {"\x0c", L"ctrl + l", offset + 33},
        {"\x0b", L"ctrl + k", offset + 32},
        {"\x0f", L"ctrl + o", offset + 31},
        {"\x10", L"ctrl + p", offset + 30},
        {"\x02", L"ctrl + b", offset + 29},
        {"\x18", L"ctrl + x", offset + 28},
        {"\x06", L"ctrl + f", offset + 27},
        {"\x01", L"ctrl + a", offset + 26},
        {"\x16", L"ctrl + v", offset + 25},
        {"\033", L"esc",     offset + 24},
        {"\033[A", L"up",    offset + 23},
        {"\033[B", L"down",  offset + 22},
        {"\033[C", L"right", offset + 21},
        {"\033[D", L"left",  offset + 20},
        {"\x1b[H", L"home",  offset + 19},
        {"\033[F", L"end",   offset + 18},
        {"\033[2~", L"insert",  offset + 17},
        {"\033[3~", L"delete",  offset + 16},
        {"\033[5~", L"page_up", offset + 15},
        {"\033[6~", L"page_down", offset + 14},
        {"\033[Z", L"shift + tab",  offset + 13},
        {"\033OP", L"f1", offset + 12},
        {"\033OQ", L"f2", offset + 11},
        {"\033OR", L"f3", offset + 10},
        {"\033OS", L"f4", offset + 9},
        {"\033[15~", L"f5", offset + 8},
        {"\033[17~", L"f6", offset + 7},
        {"\033[18~", L"f7", offset + 6},
        {"\033[19~", L"f8", offset + 5},
        {"\033[20~", L"f9", offset + 4},
        {"\033[21~", L"f10", offset + 3},
        {"\033[23~", L"f11", offset + 2},
        {"\033[24~", L"f12", offset + 1}
    };

    class Keyboard {
        int counter = 0;
        std::vector<char> first_bytes;
        std::map<int, Key> pressed_keys;
        public:

        Keyboard(){
            for (auto &k: keymap){
                if (!in(first_bytes, k.escs[0])){
                    first_bytes.push_back(k.escs[0]);
                }
            }
        }

        void poll(std::vector<char> inp_bytes){
            pressed_keys.clear();
            if (inp_bytes.size() == 0) return;

            std::string seq{inp_bytes.begin(), inp_bytes.end()};
            
            // Сначала проверяем на escape-последовательности
            if (in(first_bytes, inp_bytes[0])){
                bool shifted = starts_with(seq, "\033[1;2"),
                     ctrled  = starts_with(seq, "\033[1;5"), 
                     alted   = starts_with(seq, "\033[1;3");
                
                if (shifted || ctrled || alted){
                    seq = "\033[" + std::string(seq.begin() + 5, seq.end());
                }

                for (auto &k: keymap){
                    if (k.escs == seq) {
                        pressed_keys[++counter] = k;
                        auto & cv = pressed_keys[counter];
                        cv.alted = alted;
                        cv.shifted = shifted;
                        cv.ctrled = ctrled;

                        break;
                    }
                }
            } else {
                // Обрабатываем все остальные символы как обычные (включая кириллицу)
                std::string utf8_str(inp_bytes.begin(), inp_bytes.end());
                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                std::wstring wstr = conv.from_bytes(utf8_str);
                if (!wstr.empty()) {
                    pressed_keys[++counter] = {
                        "", wstr, (int)wstr[0]
                    };
                }
            }
        }

        KeyState isKeyPressed(std::wstring name){
            int inx = -1;
            Key ck;
            for (auto &[ii, k]: pressed_keys){
                if (k.name == name) {
                    inx = ii;
                    ck = k;
                    break;
                }
            }
            if (inx == -1) return {};
            
            pressed_keys.erase(inx);
            return {
                true,
                ck.shifted,
                ck.ctrled,
                ck.alted
            };
        }

        std::wstring uniPressed(){
            int inx = -1;
            Key ck;
            for (auto &[ii, k]: pressed_keys){
                if (k.name.size() == 1) {
                    inx = ii;
                    ck = k;
                    break;
                }
            }

            if (inx == -1) return {};
            
            pressed_keys.erase(inx);
            return ck.name;
        }

        Key getKeyPressed(){
            if (pressed_keys.size() == 0)
                return {};
            
            int inx = *(keys(pressed_keys).end() - 1);
            Key k = pressed_keys[inx];
            pressed_keys.erase(inx);

            return k;
        }
    };
}