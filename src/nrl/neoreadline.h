#define vimode 0
#define emacsmode 1
struct CHARMETA = {uint32_t index;
uint8_t length : 2;
uint8_t color;
} // 字符的元数据
