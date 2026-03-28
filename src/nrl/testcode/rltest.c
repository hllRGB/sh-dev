/* vim: set ts=8 sw=8 sts=8 et: */
#include "bind.h"
#include "cursor.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void input_loop(void) {
        while (1) {
                void * nullable addr = NULL;
                while (1) {
                        char c;
                        int i         = read(STDIN_FILENO, &c, 1);
                        SUCCESS_T ret = further_match(&addr, c);
                        if (ret == 0) {
                                if (addr) {
                                        ((void (*)(void))addr)();
                                }
                                break;
                        } else if (ret == -1) {
                                write(STDOUT_FILENO, &c, 1);
                                break;
                        }
                }
        }
}
void red(void) { printf("\033[31m"); }

void green(void) { printf("\033[32m"); }

void blue(void) { printf("\033[34m"); }

int main(int argc, char * argv[]) {
        initialize_keymap();
        add_bind(0, "h", cursor_left);
        add_bind(0, "j", cursor_down);
        add_bind(0, "k", cursor_up);
        add_bind(0, "l", cursor_right);
        add_bind(0, "r", red);
        add_bind(0, "g", green);
        add_bind(0, "b", blue);
        rm_bind("h");
        tcgetattr(STDIN_FILENO, &orig_termios);
        orig_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        setvbuf(stdout, NULL, _IONBF, 0);

        input_loop();

        return 0;
}
