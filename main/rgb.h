#ifndef RGB_H
#define RGB_H

#include <stdint.h>

void rgb_init(void);
void rgb_set_color(uint32_t color); // RGB888
void rgb_set_pixel(int idx, uint32_t color);
void rgb_demo(void);

#endif // RGB_H