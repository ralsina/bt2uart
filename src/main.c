#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <hardware/uart.h>
#include <btstack.h>
#include <stdio.h>

#include "bt_keyboard.h"
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

    printf("BT2UART Bridge v0.2\n");

    // Init UART to host on GP4/TX GP5/RX
    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);
    printf("UART init on GP%d/GP%d at %d baud\n", PIN_UART_TX, PIN_UART_RX, UART_BAUD);

    if (cyw43_arch_init()) {
        printf("cyw43_arch_init FAILED\n");
        while (1) { tight_loop_contents(); }
    }
    printf("cyw43_arch_init OK\n");

    btstack_main(0, NULL);
    printf("btstack_main returned\n");

    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 1000);
    btstack_run_loop_add_timer(&heartbeat);

    pulse_led();

    btstack_run_loop_execute();

    return 0;
}
