#include <bits/types/struct_timeval.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define FUNCTION 1
#define NODE 2
#define NONE 0
#define auto(x, y) typeof(y) x = y

#define nonnull _Nonnull
#define nullable _Nullable

struct bind_node_str {
    uint16_t timeout_us;
    char type; // 暂时那三个define
    void *ptr;
};

typedef struct bind_node_str BIND_NODE_T;
fd_set set;
struct timeval timeout;

struct termios orig_termios;

BIND_NODE_T root = {0, NONE, 0};

void initialize_keymap(void) {
    // root array has 256 elements.
    root.type = NODE;
    root.ptr  = (BIND_NODE_T *)malloc(256 * sizeof(BIND_NODE_T));
    memset(root.ptr, 0, 256 * sizeof(BIND_NODE_T));
}

int add_bind(long timeout, char *nonnull seq, void *nonnull func) {
    auto(node, &root);
    // 目前seq不可能为0
    // 目前func不可能为0
    for (; *seq; seq++) {

        node = &(((BIND_NODE_T *)node->ptr)[*seq]);
        if (*(seq + 1) != 0) {
            if (node->type == NONE) {
                node->type = NODE;
                node->ptr  = (BIND_NODE_T *)malloc(256 * sizeof(BIND_NODE_T));
                memset(node->ptr, 0, 256 * sizeof(BIND_NODE_T));
            } else if (node->type == FUNCTION) {
                node->type       = NODE;
                node->timeout_us = timeout;
                void *tmp        = node->ptr;
                node->ptr        = (BIND_NODE_T *)malloc(256 * sizeof(BIND_NODE_T));
                ((BIND_NODE_T *)node->ptr)[0].type = FUNCTION;
                ((BIND_NODE_T *)node->ptr)[0].ptr  = tmp;
            }
        } else if (*(seq + 1) == 0) {
            if (node->type == NONE || node->type == FUNCTION) {
                node->ptr  = func;
                node->type = FUNCTION;
            } else if (node->type == NODE) {
                ((BIND_NODE_T *)node->ptr)[0].type = FUNCTION;
                ((BIND_NODE_T *)node->ptr)[0].ptr  = func;
            }
        }
    }
    return 0;
}

void a(void) { printf("aaa\n"); }
void b(void) { printf("bbb\n"); }
void fuck(void) { printf("fuck\n"); }

void input_loop(void) {
    while (1) {
        auto(node, &root);
        while (1) {
            char c;
            int i = read(STDIN_FILENO, &c, 1);
            node  = &(((BIND_NODE_T *)node->ptr)[c]);
            if (node->type == FUNCTION) {
                printf("matched\n");
                ((void (*)(void))node->ptr)();
                break;
            } else if (node->type == NONE) {
                printf("none,");
                break;
            } else if (node->type == NODE && ((BIND_NODE_T *)node->ptr)[0].type != FUNCTION) {
                printf("waiting,");
            } else if (node->type == NODE && ((BIND_NODE_T *)node->ptr)[0].type == FUNCTION) {
                printf("fork,");
                timeout.tv_sec  = 0;
                timeout.tv_usec = (long)node->timeout_us;
                FD_ZERO(&set);
                FD_SET(1, &set);

                if (select(2, &set, NULL, NULL, &timeout) == 0) {
                    printf("timeout\n");
                    ((void (*)(void))((BIND_NODE_T *)node->ptr)[0].ptr)();
                    break;
                } else {
                    printf("notimeout,");
                    continue;
                }
            }
        }
    }
}
void enable_raw(void) {
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int main(void) {
    initialize_keymap();

    // 初始化终端
    tcgetattr(STDIN_FILENO, &orig_termios);
    setvbuf(stdout, NULL, _IONBF, 0);
    enable_raw();

    add_bind(1, "\033", a);
    add_bind(0, "\033[A", b);
    add_bind(100000, "fuck", fuck);
    input_loop();
    return 0;
}
