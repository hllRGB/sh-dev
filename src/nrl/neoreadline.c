#include "neoreadline.h"
#include <termios.h>
struct termios cmdline_termios_attr readline_termios_attr;
uint32_t nrl_point;  // 当前字符
uint32_t nrl_line;   // 当前行
uint32_t nrl_length; // 所在行长度
uint32_t void nrl_init(void) {}
char *neoreadline(const char *prompt) {}
