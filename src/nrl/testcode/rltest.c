/* vim: set ts=8 sw=8 sts=8 et: */
#include "bind.h"
#include "cursor.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;
BIND_NODE_T * root = NULL;

void input_loop(void) {
        while (1) {
                void * nullable addr = NULL;
                while (1) {
                        char c;
                        int i         = read(STDIN_FILENO, &c, 1);
                        SUCCESS_T ret = further_match(root, &addr, c);
                        if (ret == 0) {
                                if (addr) {
                                        // 这里应该是行编辑函数之类的，到后期改.
                                        ((void (*)(void))addr)();
                                }
                                break;
                        } else if (ret == -1) {
                                // 这里应该是默认函数之类的，到后期改.
                                write(STDOUT_FILENO, &c, 1);
                                break;
                        }
                }
        }
}

int main(int argc, char * argv[]) {
        root = initialize_keymap();

        tcgetattr(STDIN_FILENO, &orig_termios);
        orig_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        setvbuf(stdout, NULL, _IONBF, 0);

        input_loop();

        return 0;
}
