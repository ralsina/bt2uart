#pragma once

#include <stdint.h>
#include <stdbool.h>

enum key_state
{
    KEY_STATE_IDLE = 0,
    KEY_STATE_PRESSED,
    KEY_STATE_HOLD,
    KEY_STATE_RELEASED,
};

#define KEY_MOD_ALT         0x9A
#define KEY_MOD_SHL         0x9B
#define KEY_MOD_SHR         0x9C
#define KEY_MOD_SYM         0x9D

void bt_keyboard_init(void);
bool bt_keyboard_is_connected(void);

int btstack_main(int argc, const char *argv[]);
