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

#include <aplus/input.h>
#include <aplus/ui.h>

#include <cairo/cairo.h>

#define _POSIX_SOURCE
#include <time.h>

#include <nes.h>
#include <ppu.h>
#include <cpu.h>
#include <cartridge.h>
#include <controller.h>

#define NESEMU_RESX 256
#define NESEMU_RESY 240
#define NESEMU_US_PER_FRAME (1000000 / 60)

struct {
    ui_connection_t* conn;
    ui_window_t* win;
    cairo_surface_t* surface;
    cairo_surface_t* backbuffer;
    cairo_t* cr;
    uint8_t keystate[NR_KEYS];
} context;

void die(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

void os_context_init() {
    memset(context.keystate, 0, sizeof(context.keystate));

    context.conn = ui_connect(NULL, 5000);
    if(!context.conn) {
        fprintf(stderr, "nesemu: ui_connect() failed: %s\n", strerror(errno));
        exit(1);
    }

    context.win = ui_window_create(context.conn, NESEMU_RESX * 2, NESEMU_RESY * 2, "NES Emulator");
    if(!context.win) {
        fprintf(stderr, "nesemu: ui_window_create() failed: %s\n", strerror(errno));
        exit(1);
    }
}

void os_context_bind(void* screen) {

    if (!context.conn || !context.win) {
        fprintf(stderr, "nesemu: os_video_init() called before ui_connect() and ui_window_create()\n");
        exit(1);
    }

    if (context.surface) {
        cairo_surface_destroy(context.surface);
        context.surface = NULL;
    }

    if (context.backbuffer) {
        cairo_surface_destroy(context.backbuffer);
        context.backbuffer = NULL;
    }

    if (context.cr) {
        cairo_destroy(context.cr);
        context.cr = NULL;
    }
    
    context.surface = cairo_image_surface_create_for_data((unsigned char*)ui_window_pixels(context.win), CAIRO_FORMAT_RGB24, ui_window_width(context.win), ui_window_height(context.win), ui_window_stride(context.win));
    if(!context.surface) {
        fprintf(stderr, "nesemu: cairo_image_surface_create_for_data() failed\n");
        exit(1);
    }

    context.backbuffer = cairo_image_surface_create_for_data((unsigned char*)screen, CAIRO_FORMAT_RGB24, NESEMU_RESX, NESEMU_RESY, NESEMU_RESX * sizeof(uint32_t));
    if(!context.backbuffer) {
        fprintf(stderr, "nesemu: cairo_image_surface_create_for_data() failed\n");
        exit(1);
    }

    context.cr = cairo_create(context.surface);
    if(!context.cr) {
        fprintf(stderr, "nesemu: cairo_create() failed\n");
        exit(1);
    }
}

void os_draw_frame(void* screen)
{
    cairo_save(context.cr);

    cairo_scale(context.cr,
        (double)ui_window_width(context.win) / (double)NESEMU_RESX,
        (double)ui_window_height(context.win) / (double)NESEMU_RESY
    );

    cairo_set_source_surface(context.cr, context.backbuffer, 0, 0);
    cairo_set_operator(context.cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(context.cr);

    cairo_restore(context.cr);
    cairo_surface_flush(context.surface);
}

void os_sleep(unsigned int us) {

    usleep(us);
}

uint32_t os_getticks() {

    struct timespec ts = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000000ULL) + ((uint64_t)ts.tv_nsec / 1000ULL);
}

uint8_t nes_key_state(uint8_t b) {
    switch (b) {
        case 0: // On / Off
            return 1;
        case 1: // A
            return context.keystate[KEY_A] ? 1 : 0;
        case 2: // B
            return context.keystate[KEY_S] ? 1 : 0;
        case 3: // SELECT
            return context.keystate[KEY_C] ? 1 : 0;
        case 4: // START
            return context.keystate[KEY_ENTER] ? 1 : 0;
        case 5: // UP
            return context.keystate[KEY_UP] ? 1 : 0;
        case 6: // DOWN
            return context.keystate[KEY_DOWN] ? 1 : 0;
        case 7: // LEFT
            return context.keystate[KEY_LEFT] ? 1 : 0;
        case 8: // RIGHT
            return context.keystate[KEY_RIGHT] ? 1 : 0;
        default:
            return 0;
    }
    return 0;
}

uint8_t nes_key_state_ctrl2(uint8_t b) {
    switch (b) {
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
            return 0;
    }
    return 0;
}

int main(int argc, char** argv) {

    static nes_ppu_t nes_ppu;
    static nes_cpu_t nes_cpu;
    static nes_cartridge_t nes_cart;
    static nes_mem_td nes_memory = {0};

    uint32_t cpu_clocks = 0;
    uint32_t ppu_clocks = 0;
    uint32_t ppu_rest_clocks = 0;
    uint32_t ppu_clock_index = 0;
    uint8_t ppu_status = 0;

    if (argc < 2) {
        fprintf(stderr, "nesemu: usage: %s <romfile>\n", argv[0]);
        exit(1);
    }

    /* init cartridge */
    nes_cart_init(&nes_cart, &nes_memory);
    if (nes_cart_load_rom(&nes_cart, argv[1]) != 0) {
        fprintf(stderr, "nesemu: failed to load ROM: %s\n", argv[1]);
        exit(1);
    }

    /* init cpu */
    nes_cpu_init(&nes_cpu, &nes_memory);
    nes_cpu_reset(&nes_cpu);

    /* init ppu */
    nes_ppu_init(&nes_ppu, &nes_memory);

    /* init context */
    os_context_init();
    os_context_bind(nes_ppu.screen_bitmap);


    uint64_t last_ticks = os_getticks();
    uint64_t current_ticks = 0;

    bool running = true;
    while(running) {

        for(;;) {

            ui_event_t ev;
            int e = ui_next_event(context.conn, &ev, 0);

            if(e < 0) {
                fprintf(stderr, "nesemu: ui_next_event() failed: %s\n", strerror(errno));
                exit(1);
            }

            if(e == 0) {
                break;
            }

            switch (ev.type) {

                case UI_EVENT_CLOSE:
                    running = false;
                    break;

                case UI_EVENT_KEY:
                    if (ev.key.down) {
                        context.keystate[ev.key.vkey] = 1;
                    } else {
                        context.keystate[ev.key.vkey] = 0;
                    }
                    break;

                case UI_EVENT_CONFIGURE:
                    if(ui_window_apply_configure(context.win) < 0) {
                        fprintf(stderr, "nesemu: ui_window_apply_configure() failed: %s\n", strerror(errno));
                        exit(1);
                    }
                    os_context_bind(nes_ppu.screen_bitmap);
                    break;

                default:
                    break;
            }

            if(!running) {
                break;
            }

        }

        for(;;) {

            cpu_clocks = 0;
            if (!ppu_rest_clocks) {
                if (ppu_status & PPU_STATUS_NMI) {
                    cpu_clocks += nes_cpu_nmi(&nes_cpu);
                }
                cpu_clocks += nes_cpu_run(&nes_cpu);
            }

            /* the ppu runs at a 3 times higher clock rate than the cpu
            so we need to give the ppu some clocks here to catchup */
            ppu_clocks = (cpu_clocks * 3) + ppu_rest_clocks;
            ppu_status = 0;
            for (ppu_clock_index = 0; ppu_clock_index < ppu_clocks; ppu_clock_index++) {
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

        os_draw_frame(nes_ppu.screen_bitmap);
        
        ui_window_damage_all(context.win);
        if(ui_window_commit(context.win) < 0) {
            fprintf(stderr, "nesemu: ui_window_commit() failed: %s\n", strerror(errno));
            exit(1);
        }



        if ((current_ticks = os_getticks()) < last_ticks + NESEMU_US_PER_FRAME) {
            os_sleep((last_ticks + NESEMU_US_PER_FRAME) - current_ticks);
        }

        last_ticks = current_ticks;

    }

    ui_window_destroy(context.win);
    ui_disconnect(context.conn);

    return 0;
}
