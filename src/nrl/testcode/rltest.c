#include "bind.h" // 提供 initialize_keymap, add_bind, further_match
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* ========== 原有结构定义 ========== */

typedef struct char_meta_st {
    uint8_t len;    // for utf8
    uint16_t color; // unused
} CHAR_META_T;

typedef struct line_unit_st {
    struct line_unit_st * lastline;
    char * buffer;
    uint16_t lineno;
    struct line_unit_st * nextline;
} LINE_UNIT_T;

typedef struct le_root_st {
    char * ps0;
    char * ps1;
    char * ps2;
    char * ps3;
    char * ps4;
    LINE_UNIT_T * firstline;
    uint16_t linenum;
    uint8_t state;
} LE_ROOT_T;

#define MULTILINE (1 << 0)
#define VIM_MODE (1 << 1)

/* ========== 全局编辑状态 ========== */
static LE_ROOT_T * global_root = NULL;
static char * current_line     = NULL; // 当前编辑行（动态字符串）
static size_t current_len      = 0;    // 当前行长度（字节数，暂只支持 ASCII）
static size_t cursor_pos       = 0;    // 光标位置（字节索引）
static char last_input_char    = 0;    // 供 self-insert 使用
static struct termios orig_termios;

/* ========== 终端控制 ========== */
static void set_raw_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void reset_terminal_mode(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

static char get_raw_char(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;
    return 0;
}

/* ========== 界面绘制 ========== */
static void redisplay(void) {
    const char * prompt = global_root->ps0;
    if (global_root->state & MULTILINE)
        prompt = global_root->ps1; // 可扩展 ps2/ps3/ps4

    printf("\r\033[K"); // 清除当前行
    printf("%s%s", prompt, current_line ? current_line : "");
    int prompt_len = strlen(prompt);
    printf("\033[%dG", prompt_len + cursor_pos + 1); // 移动光标到指定列（1-based）
    fflush(stdout);
}

/* ========== 编辑操作 ========== */
static void insert_char(char ch) {
    size_t new_len  = current_len + 1;
    char * new_line = realloc(current_line, new_len + 1);
    if (!new_line) {
        perror("realloc");
        return;
    }
    current_line = new_line;
    memmove(current_line + cursor_pos + 1, current_line + cursor_pos, current_len - cursor_pos + 1);
    current_line[cursor_pos] = ch;
    current_len++;
    cursor_pos++;
    redisplay();
}

static void delete_backward(void) {
    if (cursor_pos == 0)
        return;
    memmove(current_line + cursor_pos - 1, current_line + cursor_pos, current_len - cursor_pos + 1);
    current_len--;
    cursor_pos--;
    redisplay();
}

static void move_left(void) {
    if (cursor_pos > 0) {
        cursor_pos--;
        redisplay();
    }
}

static void move_right(void) {
    if (cursor_pos < current_len) {
        cursor_pos++;
        redisplay();
    }
}

static void move_to_start(void) {
    if (cursor_pos != 0) {
        cursor_pos = 0;
        redisplay();
    }
}

static void move_to_end(void) {
    if (cursor_pos != current_len) {
        cursor_pos = current_len;
        redisplay();
    }
}

/* 保存当前行并准备下一行 */
static void accept_line(void) {
    LINE_UNIT_T * newline = calloc(1, sizeof(LINE_UNIT_T));
    if (!newline) {
        perror("calloc");
        return;
    }
    newline->buffer = strdup(current_line ? current_line : "");
    if (!newline->buffer) {
        free(newline);
        return;
    }
    newline->lineno = global_root->linenum + 1;

    if (global_root->firstline == NULL) {
        global_root->firstline = newline;
        newline->lastline      = NULL;
        newline->nextline      = NULL;
    } else {
        LINE_UNIT_T * last = global_root->firstline;
        while (last->nextline)
            last = last->nextline;
        last->nextline    = newline;
        newline->lastline = last;
    }
    global_root->linenum++;

    free(current_line);
    current_line = strdup("");
    if (!current_line)
        exit(1);
    current_len = 0;
    cursor_pos  = 0;
    redisplay();
}

/* 退出编辑器 */
static void exit_editor(void) {
    free(current_line);
    current_line = NULL; // 标记退出
}

/* 插入最后输入的字符（供普通字符使用） */
static void self_insert(void) {
    if (last_input_char >= 32 && last_input_char <= 126)
        insert_char(last_input_char);
}

/* ========== 主编辑循环（集成 bind.c 新接口） ========== */
char * le_main(LE_ROOT_T * rootunit) {
    global_root  = rootunit;
    current_line = strdup("");
    if (!current_line)
        return NULL;
    current_len = 0;
    cursor_pos  = 0;

    set_raw_mode();

    // 初始化 bind.c 内部数据结构
    initialize_keymap();

    // 注册常用按键绑定（超时 500 毫秒 = 500000 微秒）
    add_bind(500000, "\x01", move_to_start);   // Ctrl+A
    add_bind(500000, "\x05", move_to_end);     // Ctrl+E
    add_bind(500000, "\x02", move_left);       // Ctrl+B
    add_bind(500000, "\x06", move_right);      // Ctrl+F
    add_bind(500000, "\x08", delete_backward); // 退格
    add_bind(500000, "\x7f", delete_backward); // DEL
    add_bind(500000, "\x04", exit_editor);     // Ctrl+D
    // 方向键（常见的转义序列）
    add_bind(500000, "\033[A", move_left);  // 上箭头（示例）
    add_bind(500000, "\033[B", move_right); // 下箭头
    add_bind(500000, "\033[D", move_left);  // 左箭头
    add_bind(500000, "\033[C", move_right); // 右箭头
    add_bind(500000, "\n", accept_line);    // 回车

    int done = 0;
    while (!done) {
        redisplay();

        char c          = get_raw_char();
        last_input_char = c;

        void * matched_func = NULL; // 存储匹配结果
        SUCCESS_T ret       = further_match(&matched_func, c);

        if (ret == -1) {
            // 匹配失败：丢弃该字节，但如果是可打印字符则插入
            if (c >= 32 && c <= 126) {
                self_insert();
            }
        } else if (ret == 0) {
            // 匹配完成，执行绑定的函数
            if (matched_func != NULL) {
                void (*func)(void) = matched_func;
                func();
            }
        } else if (ret == 1) {
            // 等待更多字符，什么都不做，继续循环
            continue;
        }

        // 检查是否退出（exit_editor 会将 current_line 置为 NULL）
        if (current_line == NULL) {
            done = 1;
        }
    }

    reset_terminal_mode();

    // 拼接所有行
    size_t total_len = 0;
    for (LINE_UNIT_T * l = global_root->firstline; l; l = l->nextline) {
        total_len += strlen(l->buffer) + 1; // +1 for newline
    }
    char * result = malloc(total_len + 1);
    if (!result)
        return NULL;
    result[0] = '\0';
    for (LINE_UNIT_T * l = global_root->firstline; l; l = l->nextline) {
        strcat(result, l->buffer);
        if (l->nextline)
            strcat(result, "\n");
    }
    return result;
}

/* ========== 初始化和释放 ========== */
LE_ROOT_T * init_line(void) {
    LE_ROOT_T * root = calloc(1, sizeof(LE_ROOT_T));
    if (!root) {
        perror("init_line");
        exit(1);
    }
    root->ps0       = strdup("> ");
    root->ps1       = strdup(".. ");
    root->ps2       = strdup("    ");
    root->ps3       = strdup("    ");
    root->ps4       = strdup("    ");
    root->firstline = NULL;
    root->linenum   = 0;
    root->state     = 0;
    return root;
}

SUCCESS_T free_le_full(LE_ROOT_T * root) {
    if (!root)
        return 0;
    LINE_UNIT_T * curr = root->firstline;
    while (curr) {
        LINE_UNIT_T * next = curr->nextline;
        free(curr->buffer);
        free(curr);
        curr = next;
    }
    free(root->ps0);
    free(root->ps1);
    free(root->ps2);
    free(root->ps3);
    free(root->ps4);
    free(root);
    return 1;
}

/* ========== 演示 ========== */
int main(void) {
    LE_ROOT_T * root = init_line();
    if (!root)
        return 1;

    printf("=== Line Editor with Bindings ===\n");
    printf("Ctrl+A: start of line, Ctrl+E: end of line\n");
    printf("Ctrl+B/F: left/right, Backspace: delete\n");
    printf("Ctrl+D or empty line to exit\n");

    char * text = le_main(root);
    if (text) {
        printf("\n--- Final text ---\n%s\n", text);
        free(text);
    }

    free_le_full(root);
    return 0;
}
