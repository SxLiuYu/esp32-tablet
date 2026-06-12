#ifndef UI_H
#define UI_H

typedef enum {
    UI_STATE_IDLE,
    UI_STATE_LISTENING,
    UI_STATE_THINKING,
    UI_STATE_REPLY,
    UI_STATE_READY,
    UI_STATE_ERROR,
    UI_STATE_AP_MODE,
} ui_state_t;

void ui_init(void);
void ui_set_state(ui_state_t state);
void ui_show_reply(const char *text);

#endif // UI_H