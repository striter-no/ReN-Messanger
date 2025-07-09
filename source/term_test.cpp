#include <console/term.hpp>
#include <console/keyboard.hpp>
#include <console/console.hpp>

cns::Terminal term;
cns::Keyboard kboard;

int main(){
    term.setup();
    term.hide_cursor();

    cns::Console cnsl(&term);
    while (!(term.is_ctrl_c_pressed() || term.is_ctrl_z_pressed())){

        cnsl.clear();
        kboard.poll(term.inp_bytes(true));

        cnsl.draw();
    }

    term.show_cursor();
    term.finish();
}