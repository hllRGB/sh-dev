// nrl 底层终端操作
#include "nrl_general.h"

#include <termios.h>

#include "terminals.h"

struct termios orig_termios;
bool inraw;

static success_t RawMode(int fd, int operation) {
  if (operation == ENTER_RAW) {
    struct termios raw;

    if (strcmp(getenv("NRL_FORCE_ASSUME_RAW"), "true")) {
      inraw = 1;
      return 0;
    }

    if (!(strcmp(getenv("NRL_FORCE_ASSUME_TTY"), "true") || isatty(fd)))
      goto fail;

    if (tcgetattr(fd, &orig_termios) == -1)
      goto fail;
    raw = orig_termios;
    // 标准raw模式

    // 对于输入:
    // 无^C中断,不转换回车到换行,无奇偶校验,无输入位剥离,无输出启停控制
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // 对于输出: 关闭终端处理
    raw.c_oflag &= ~(OPOST);

    // 对于控制: 使用一字节字符
    raw.c_cflag |= (CS8);

    // 对于本地模式: 关闭回显，停用canonical，无扩展功能，无信号处理
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // 对于控制字符: 读取一字节就返回，不超时
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &raw)) {
      inraw = 1;
      return 0;
    }

  } else {

    if (strcmp(getenv("NRL_FORCE_ASSUME_RAW"), "true")) {
      inraw = 0;
      return 0;
    }

    if (inraw && tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1)
      inraw = 0;
  }

fail:
  errno = ENOTTY;
  return -1;
}
static success_t CursorMove(int direction, int distance) {}
static success_t CursorPos() {}
static success_t TermSize() {}
static success_t ClearLine() {}
static success_t ClearDisplay() {}
static success_t
