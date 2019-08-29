#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include <stdint.h>

/* CPU MEMORY */
#define CPU_MEM_INTERNAL_RAM_OFFSET         0x0000
#define CPU_MEM_INTERNAL_RAM_SIZE           0x0800

#define CPU_MEM_INTERNAL_RAM_MIRROR1_OFFSET 0x0800
#define CPU_MEM_INTERNAL_RAM_MIRROR1_SIZE   0x0800

#define CPU_MEM_INTERNAL_RAM_MIRROR2_OFFSET 0x1000
#define CPU_MEM_INTERNAL_RAM_MIRROR2_SIZE   0x0800

#define CPU_MEM_INTERNAL_RAM_MIRROR3_OFFSET 0x1800
#define CPU_MEM_INTERNAL_RAM_MIRROR3_SIZE   0x0800

#define CPU_MEM_PPU_REGISTER_OFFSET         0x2000
#define CPU_MEM_PPU_REGISTER_SIZE           0x0008

#define CPU_MEM_PPU_MIRRORS_OFFSET          0x2008
#define CPU_MEM_PPU_MIRRORS_SIZE            0x1FF8
#define CPU_MEM_PPU_MIRRORS_REPEAT          0x0008

#define CPU_MEM_PPU_REGISTER_AREA_END       0x3FFF

#define CPU_MEM_APU_REGISTER_OFFSET         0x4000
#define CPU_MEM_APU_REGISTER_SIZE           0x0018

#define CPU_MEM_APU_IO_REGISTER_OFFSET      0x4018
#define CPU_MEM_APU_IO_REGISTER_SIZE        0x0008

#define CPU_MEM_CRTRDG_REGISTER_OFFSET      0x4020
#define CPU_MEM_CRTRDG_REGISTER_SIZE        0xBFE0

#define CPU_MEM_PHYS_SIZE CPU_MEM_INTERNAL_RAM_SIZE + \
                          CPU_MEM_PPU_REGISTER_SIZE + \
                          CPU_MEM_APU_REGISTER_SIZE + \
                          CPU_MEM_APU_IO_REGISTER_SIZE + \
                          CPU_MEM_CRTRDG_REGISTER_SIZE
#define CPU_MEM_VIRT_SIZE 0x10000


#define CPU_MEM_PPU_CTRL_REGISTER           0x2000
#define CPU_MEM_PPU_MASK_REGISTER           0x2001
#define CPU_MEM_PPU_STATUS_REGISTER         0x2002
#define CPU_MEM_PPU_OAMADDR_REGISTER        0x2003
#define CPU_MEM_PPU_OAMDATA_REGISTER        0x2004
#define CPU_MEM_PPU_SCROLL_REGISTER         0x2005
#define CPU_MEM_PPU_ADDR_REGISTER           0x2006
#define CPU_MEM_PPU_DATA_REGISTER           0x2007
#define CPU_MEM_PPU_OAMDMA_REGISTER         0x4014


#define CPU_CONTROLLER1_REGISTER 0x4016
#define CPU_CONTROLLER2_REGISTER 0x4017


#define ACCESS_WRITE_WORD 0
#define ACCESS_WRITE_BYTE 1
#define ACCESS_READ_WORD  2
#define ACCESS_READ_BYTE  3
#define ACCESS_FUNC_MAX   4


typedef struct nes_mem_struct
{
    /* Only OxC808 bytes are actually used by the nes:
     * 0x10000−0x800−0x800−0x800−0x1ff8 = 0xC808
     */
    uint8_t cpu_memory[CPU_MEM_PHYS_SIZE];

} nes_mem_td;

uint16_t memory_access(nes_mem_td *memmap, uint16_t addr, uint16_t data, uint8_t access_type);

#endif /* MEMORY_CONTROLLER_H */
