// bind.h
#include <stdint.h>
#define FUNCTION 1
#define NODE 2
#define NONE 0
#define auto(x, y) typeof(y) x = y

#define nonnull _Nonnull
#define nullable _Nullable

struct bind_node_str {
    uint16_t timeout_us;
    char type; // 暂时那三个define
    void * nonnull ptr;
};

typedef struct bind_node_str BIND_NODE_T;
typedef int8_t SUCCESS_T;

extern void initialize_keymap(void);

extern int add_bind(long timeout, char * nonnull seq, void * nonnull func);

extern SUCCESS_T further_match(void * nonnull * nonnull addr, char byte);
