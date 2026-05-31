/**
 * @file lwmem_porting.c
 * @author Lu Yongping (Lucas@hiwonder.com)
 * @brief lwmem 接口移植及内存空间定义
 * @version 0.1
 * @date 2023-06-02
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "lwmem.h"

uint8_t lwmem_ram[1024 * 24];

lwmem_region_t lwmem_regions[] = {
    { (void*)0x10000000, 1024 * 32 },
    { (void*)lwmem_ram,  1024 * 24 },
    { NULL, 0}
};
