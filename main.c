/*
 *   This file is part of nes_emu.
 *   Copyright (c) 2019 Franz Flasch.
 *
 *   nes_emu is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   nes_emu is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with nes_emu.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <sched.h>

#include <aplus/fb.h>
#include <aplus/events.h>
#include <aplus/input.h>

#define _POSIX_SOURCE
#include <time.h>

/* NES specific */
#include <nes.h>
#include <ppu.h>
#include <cpu.h>
#include <cartridge.h>
#include <controller.h>

#define debug_print(fmt, ...)         \
    do                                \
    {                                 \
        if (DEBUG_MAIN)               \
            printf(fmt, __VA_ARGS__); \
    } while (0)

void die(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    vfprintf(stderr, format, vargs);
    va_end(vargs);
    exit(1);
}

#define FPS 60
#define FPS_UPDATE_TIME_MS (1000 / FPS)

static struct
{

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    int kbd;

    uint8_t keys[NR_KEYS];

} context;

void os_video_init()
{

    int fd;

    if ((fd = open("/dev/fb0", O_RDWR)) < 0)
    {
        fprintf(stderr, "doom: open() failed: cannot open /dev/fb0: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &context.var) < 0)
    {
        fprintf(stderr, "doom: ioctl(FBIOGET_VSCREENINFO) failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &context.fix) < 0)
    {
        fprintf(stderr, "doom: ioctl(FBIOGET_FSCREENINFO) failed: %s\n", strerror(errno));
        exit(1);
    }

    if (!context.fix.smem_start || !context.var.xres_virtual || !context.var.yres_virtual)
    {
        fprintf(stderr, "doom: wrong framebuffer configuration\n");
        exit(1);
    }

    if (close(fd) < 0)
    {
        fprintf(stderr, "doom: close() failed: %s\n", strerror(errno));
        exit(1);
    }

    if ((context.kbd = open("/dev/kbd", O_RDONLY)) < 0)
    {
        fprintf(stderr, "doom: open() failed: cannot open /dev/kbd: %s\n", strerror(errno));
        exit(1);
    }

    if (fcntl(context.kbd, F_SETFL, O_NONBLOCK) < 0)
    {
        fprintf(stderr, "doom: fcntl(F_SETFL) failed: %s\n", strerror(errno));
        exit(1);
    }
}

void os_draw_frame()
{

    uintptr_t d = (uintptr_t)context.fix.smem_start;
    uintptr_t s = (uintptr_t)DG_ScreenBuffer;

    for (size_t y = 0; y < DOOMGENERIC_RESY; y++)
    {

        memcpy((void *)d, (const void *)s, DOOMGENERIC_RESX * sizeof(uint32_t));

        s += DOOMGENERIC_RESX * sizeof(uint32_t);
        d += context.fix.line_length;
    }
}

void os_sleep(unsigned int ms)
{
    usleep(ms * 1000);
}

uint32_t os_getticks()
{

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
        fprintf(stderr, "doom: clock_gettime() failed: %s\n", strerror(errno));
        exit(1);
    }

    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int os_getkey(int *pressed, unsigned char *key)
{

    event_t ev;

    if (read(context.kbd, &ev, sizeof(ev)) == sizeof(event_t))
    {
        context.keys[ev.ev_key] = ev.ev_down;
    }
 
}

/* Query a button's state.
   Returns 1 if button #b is pressed. */
uint8_t nes_key_state(uint8_t b)
{
    switch (b)
    {
    case 0: // On / Off
        return 1;
    case 1: // A
        return context.keys[KEY_A] ? 1 : 0;
    case 2: // B
        return context.keys[KEY_S] ? 1 : 0;
    case 3: // SELECT
        return context.keys[KEY_C] ? 1 : 0;
    case 4: // START
        return context.keys[KEY_ENTER] ? 1 : 0;
    case 5: // UP
        return context.keys[KEY_UP] ? 1 : 0;
    case 6: // DOWN
        return context.keys[KEY_DOWN] ? 1 : 0;
    case 7: // LEFT
        return context.keys[KEY_LEFT] ? 1 : 0;
    case 8: // RIGHT
        return context.keys[KEY_RIGHT] ? 1 : 0;
    default:
        return 1;
    }
}

uint8_t nes_key_state_ctrl2(uint8_t b)
{
    switch (b)
    {
    case 0: // On / Off
        return 1;
    case 1: // A
        return 0;
    case 2: // B
        return 0;
    case 3: // SELECT
        return 0;
    case 4: // START
        return 0;
    case 5: // UP
        return 0;
    case 6: // DOWN
        return 0;
    case 7: // LEFT
        return 0;
    case 8: // RIGHT
        return 0;
    default:
        return 1;
    }
}

int main(int argc, char *argv[])
{
    /* NES part */
    static nes_ppu_t nes_ppu;
    static nes_cpu_t nes_cpu;
    static nes_cartridge_t nes_cart;
    static nes_mem_td nes_memory = {0};

    uint32_t cpu_clocks = 0;
    uint32_t ppu_clocks = 0;
    uint32_t ppu_rest_clocks = 0;
    uint32_t ppu_clock_index = 0;
    uint8_t ppu_status = 0;

    if (argc != 2)
    {
        die("Please specify rom file\n");
    }

    /* init cartridge */
    nes_cart_init(&nes_cart, &nes_memory);

    /* load rom */
    if (nes_cart_load_rom(&nes_cart, argv[1]) != 0)
    {
        die("ROM does not exist\n");
    }

    /* init cpu */
    nes_cpu_init(&nes_cpu, &nes_memory);
    nes_cpu_reset(&nes_cpu);

    /* init ppu */
    nes_ppu_init(&nes_ppu, &nes_memory);

    /* Initialization */
    unsigned int lastTime = 0, currentTime;

    os_video_init();

    while (1)
    {

        os_getkey();

        /* NES core loop */
        for (;;)
        {
            cpu_clocks = 0;
            if (!ppu_rest_clocks)
            {
                if (ppu_status & PPU_STATUS_NMI)
                    cpu_clocks += nes_cpu_nmi(&nes_cpu);
                cpu_clocks += nes_cpu_run(&nes_cpu);
            }

            /* the ppu runs at a 3 times higher clock rate than the cpu
            so we need to give the ppu some clocks here to catchup */
            ppu_clocks = (cpu_clocks * 3) + ppu_rest_clocks;
            ppu_status = 0;
            for (ppu_clock_index = 0; ppu_clock_index < ppu_clocks; ppu_clock_index++)
            {
                ppu_status |= nes_ppu_run(&nes_ppu, nes_cpu.num_cycles);
                if (ppu_status & PPU_STATUS_FRAME_READY)
                    break;
                else
                    ppu_rest_clocks = 0;
            }

            ppu_rest_clocks = (ppu_clocks - ppu_clock_index);

            nes_ppu_dump_regs(&nes_ppu);

            if (ppu_status & PPU_STATUS_FRAME_READY)
                break;
        }

        /* Commented DEBUG code */
        // for(int i=0x0000;i<0x0FFF;i++)
        // {
        //     printf("Pattern Table 0: %x %x\n", i, (uint8_t)ppu_memory_access(&nes_memory, i, 0, ACCESS_READ_BYTE));
        // }
        // for(int i=0x1000;i<0x1FFF;i++)
        // {
        //     printf("Pattern Table 1: %x %x\n", i, (uint8_t)ppu_memory_access(&nes_memory, i, 0, ACCESS_READ_BYTE));
        // }
        // /* Nametable 0 contents */
        // for(int i=0x2000;i<0x23FF;i++)
        // {
        //     printf("Nametable 0: %x %x\n", i, (uint8_t)ppu_memory_access(&nes_memory, i, 0, ACCESS_READ_BYTE));
        // }
        // /* Nametable 1 contents */
        // for(int i=0x2400;i<0x27FF;i++)
        // {
        //     printf("Nametable 1: %x %x\n", i, (uint8_t)ppu_memory_access(&nes_memory, i, 0, ACCESS_READ_BYTE));
        // }
        // for(int i=0x3F00;i<=0x3F1F;i++)
        // {
        //     printf("PALLETE: %x %x\n", i, ppu_memory_access(&nes_memory, i, 0, ACCESS_READ_BYTE));
        // }
        // for(int i=0;i<256;i++)
        // {
        //     printf("OAM: %d %x\n", i, nes_memory.oam_memory[i]);
        // }

        os_draw_frame();

        /* 60 FPS framerate limit */
        while ((currentTime = os_getticks()) < (lastTime + FPS_UPDATE_TIME_MS))
            ;

        lastTime = currentTime;
    }

    return 0;
}
