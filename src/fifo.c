#include "fifo.h"
#include <hardware/sync.h>

#ifndef PICO_SPINLOCK_ID_0
#define PICO_SPINLOCK_ID_0 0
#endif

static struct {
    struct fifo_item fifo[KEY_FIFO_SIZE];
    uint8_t count;
    uint8_t read_idx;
    uint8_t write_idx;
    bool capslock;
    bool numlock;
} self;

uint8_t fifo_count(void)
{
    return self.count;
}

void fifo_flush(void)
{
    uint32_t save = spin_lock_blocking(spin_lock_instance(PICO_SPINLOCK_ID_0));
    self.write_idx = 0;
    self.read_idx = 0;
    self.count = 0;
    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
}

bool fifo_enqueue(const struct fifo_item item)
{
    uint32_t save = spin_lock_blocking(spin_lock_instance(PICO_SPINLOCK_ID_0));
    if (self.count >= KEY_FIFO_SIZE) {
        spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
        return false;
    }
    self.fifo[self.write_idx++] = item;
    self.write_idx %= KEY_FIFO_SIZE;
    ++self.count;
    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
    return true;
}

void fifo_enqueue_force(const struct fifo_item item)
{
    uint32_t save = spin_lock_blocking(spin_lock_instance(PICO_SPINLOCK_ID_0));
    if (self.count < KEY_FIFO_SIZE) {
        self.fifo[self.write_idx++] = item;
        self.write_idx %= KEY_FIFO_SIZE;
        ++self.count;
    } else {
        self.fifo[self.write_idx++] = item;
        self.write_idx %= KEY_FIFO_SIZE;
        self.read_idx++;
        self.read_idx %= KEY_FIFO_SIZE;
    }
    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
}

struct fifo_item fifo_dequeue(void)
{
    struct fifo_item item = { 0 };
    uint32_t save = spin_lock_blocking(spin_lock_instance(PICO_SPINLOCK_ID_0));
    if (self.count == 0) {
        spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
        return item;
    }
    item = self.fifo[self.read_idx++];
    self.read_idx %= KEY_FIFO_SIZE;
    --self.count;
    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_0), save);
    return item;
}

void fifo_set_capslock(bool on)
{
    self.capslock = on;
}

void fifo_set_numlock(bool on)
{
    self.numlock = on;
}

bool fifo_get_capslock(void)
{
    return self.capslock;
}

bool fifo_get_numlock(void)
{
    return self.numlock;
}
