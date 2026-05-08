#include "reg.h"
#include "fifo.h"
#include "interrupt.h"
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <hardware/sync.h>

static struct {
    uint8_t regs[REG_ID_LAST];
} self;

void reg_process_packet(uint8_t in_reg, uint8_t in_data,
                        uint8_t *out_buffer, uint8_t *out_len)
{
    bool is_write = (in_reg & PACKET_WRITE_MASK);
    uint8_t reg = (in_reg & ~PACKET_WRITE_MASK);

    *out_len = 0;

    switch (reg) {

    case REG_ID_CFG:
    case REG_ID_INT:
    case REG_ID_DEB:
    case REG_ID_FRQ:
    case REG_ID_BKL:
    case REG_ID_BK2:
    case REG_ID_GIC:
    case REG_ID_GIN:
    case REG_ID_HLD:
    case REG_ID_ADR:
    case REG_ID_IND:
    case REG_ID_CF2:
    case REG_ID_DIR:
    case REG_ID_PUE:
    case REG_ID_PUD:
    case REG_ID_GIO:
        if (is_write) {
            reg_set_value(reg, in_data);
        } else {
            out_buffer[0] = reg_get_value(reg);
            *out_len = 1;
        }
        break;

    case REG_ID_TOX:
    case REG_ID_TOY:
        out_buffer[0] = reg_get_value(reg);
        *out_len = 1;
        reg_set_value(reg, 0);
        break;

    case REG_ID_VER:
        out_buffer[0] = VER_VAL;
        *out_len = 1;
        break;

    case REG_ID_KEY: {
        uint8_t val = fifo_count() & KEY_COUNT_MASK;
        if (fifo_get_capslock())  val |= KEY_CAPSLOCK;
        if (fifo_get_numlock())   val |= KEY_NUMLOCK;
        out_buffer[0] = val;
        *out_len = 1;
        break;
    }

    case REG_ID_FIF: {
        struct fifo_item item = fifo_dequeue();
        out_buffer[0] = (uint8_t)item.state;
        out_buffer[1] = (uint8_t)item.key;
        *out_len = 2;
        break;
    }

    case REG_ID_RST:
        reset_usb_boot(0, 0);
        break;

    default:
        break;
    }
}

uint8_t reg_get_value(enum reg_id reg)
{
    return self.regs[reg];
}

void reg_set_value(enum reg_id reg, uint8_t value)
{
    self.regs[reg] = value;
}

bool reg_is_bit_set(enum reg_id reg, uint8_t bit)
{
    return self.regs[reg] & bit;
}

void reg_set_bit(enum reg_id reg, uint8_t bit)
{
    self.regs[reg] |= bit;
}

void reg_clear_bit(enum reg_id reg, uint8_t bit)
{
    self.regs[reg] &= ~bit;
}

void reg_init(void)
{
    reg_set_value(REG_ID_CFG, CFG_OVERFLOW_INT | CFG_KEY_INT | CFG_USE_MODS);
    reg_set_value(REG_ID_BKL, 255);
    reg_set_value(REG_ID_DEB, 10);
    reg_set_value(REG_ID_FRQ, 10);
    reg_set_value(REG_ID_BK2, 255);
    reg_set_value(REG_ID_PUD, 0xFF);
    reg_set_value(REG_ID_HLD, 30);
    reg_set_value(REG_ID_ADR, 0x1F);
    reg_set_value(REG_ID_IND, 1);
    reg_set_value(REG_ID_CF2, 0);
    reg_set_value(REG_ID_DIR, 0xFF);
}
