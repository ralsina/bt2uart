#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <btstack.h>
#include <stdio.h>

#include "bt_keyboard.h"
#include "i2c_slave.h"
#include "reg.h"
#include "interrupt.h"
#include "pins.h"

static btstack_timer_source_t heartbeat;

static void heartbeat_handler(btstack_timer_source_t *ts)
{
    static int count = 0;
    cyw43_arch_gpio_put(PICO_W_LED, count & 1);
    printf("[%d] alive\n", count++);
    btstack_run_loop_set_timer(&heartbeat, 5000);
    btstack_run_loop_add_timer(&heartbeat);
}

static void pulse_led(void)
{
    cyw43_arch_gpio_put(PICO_W_LED, 1);
    busy_wait_ms(50);
    cyw43_arch_gpio_put(PICO_W_LED, 0);
}

int main(void)
{
    stdio_init_all();

    // wait for USB serial to enumerate before first print
    sleep_ms(2000);

    printf("BT2I2C Bridge init...\n");

    if (cyw43_arch_init()) {
        printf("cyw43_arch_init FAILED\n");
        while (1) { tight_loop_contents(); }
    }
    printf("cyw43_arch_init OK\n");

    btstack_main(0, NULL);
    printf("btstack_main returned\n");

    reg_init();
    interrupt_init();
    i2c_slave_init();
    printf("I2C slave initialized (address 0x%02x)\n", reg_get_value(REG_ID_ADR));

    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 1000);
    btstack_run_loop_add_timer(&heartbeat);

    pulse_led();

    btstack_run_loop_execute();

    return 0;
}
