/*
 * Copyright (C) 2024 wolfSSL Inc.
 *
 * This file is part of wolfHSM.
 *
 * wolfHSM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfHSM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfHSM.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>

#include "wolfhsm/wh_settings.h"

#if defined(WOLFHSM_CFG_ENABLE_SERVER)

#include "wh_test_common.h"
#include "wh_test_flash_ramsim.h"
#include "wolfhsm/wh_flash_ramsim.h"
#include "wolfhsm/wh_error.h"

#define TEST_FLASH_SIZE (1024 * 1024)
#define TEST_SECTOR_SIZE (4096)
#define TEST_PAGE_SIZE (256)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    whFlashRamsimCtx ctx;
    static uint8_t memory[TEST_FLASH_SIZE];
    whFlashRamsimCfg cfg = {
        .size       = TEST_FLASH_SIZE,
        .sectorSize = TEST_SECTOR_SIZE,
        .pageSize   = TEST_PAGE_SIZE,
        .erasedByte = 0xFF,
        .memory     = memory,
    };

    if (size == 0) return 0;

    whFlashRamsim_Init(&ctx, &cfg);
    whFlashRamsim_Erase(&ctx, 0, cfg.size);

    if (size >= cfg.size) {
        size_t to_program = cfg.size;
        whFlashRamsim_Program(&ctx, 0, (uint32_t)to_program, data);
    } else {
        size_t data_off = 0;
        size_t flash_off = 0;
        while (data_off < size && flash_off < cfg.size) {
            size_t want = cfg.pageSize;
            size_t flash_remaining = cfg.size - flash_off;
            if (want > flash_remaining) {
                want = flash_remaining;
            }

            size_t data_remaining = size - data_off;
            size_t this_size = data_remaining;
            if (want < data_remaining) {
                this_size = want;
            }

            whFlashRamsim_Program(&ctx, (uint32_t)flash_off, (uint32_t)this_size, &data[data_off]);

            data_off += this_size;
            flash_off += this_size;
        }
    }

    whFlashRamsim_Cleanup(&ctx);
    return 0;
}

#endif /* WOLFHSM_CFG_ENABLE_SERVER */