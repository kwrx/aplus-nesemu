#include <stdio.h>

#include <controller.h>


static uint8_t prev_write;
static uint8_t p = 10;

uint8_t psg_io_read(void)
{
    // Joystick 1
    if (p++ < 9) {
        return nes_key_state(p);
    }
    return 0;
}

void psg_io_write(uint8_t data)
{
    if ((data & 1) == 0 && prev_write == 1) {
        // strobe
        p = 0;
    }
    prev_write = data & 1;
}
