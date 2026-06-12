#ifndef KEYPAD_H
#define KEYPAD_H

#define KEY_VOL_DOWN 0
#define KEY_POW      1
#define KEY_BOOT     2
#define KEY_RGB      3
#define KEY_VOL_UP   4

void keypad_init(void (*handler)(int key, int pressed));
void keypad_process(void);

#endif // KEYPAD_H