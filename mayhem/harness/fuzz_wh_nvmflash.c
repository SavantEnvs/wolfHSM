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
#include <stddef.h>

#include "wolfhsm/wh_settings.h"

#if defined(WOLFHSM_CFG_ENABLE_SERVER)

/* core test includes */
#include "wh_test_common.h"

/* APIs to test */
#include "wolfhsm/wh_error.h"
#include "wolfhsm/wh_nvm.h"
#include "wolfhsm/wh_nvm_flash.h"
#include "wolfhsm/wh_nvm_flash_log.h"
#include "wolfhsm/wh_flash_unit.h"

/* NVM simulator backends to use for testing NVM module */
#include "wolfhsm/wh_flash_ramsim.h"
/* Fault injection layer to simulate power failures */
#include "wh_test_flash_fault_inject.h"

#include <unistd.h>  /* For unlink */
#include "port/posix/posix_transport_tcp.h"
#include "port/posix/posix_flash_file.h"

#define FLASH_RAM_SIZE (1024 * 1024) /* 1MB */
#define FLASH_SECTOR_SIZE (4096) /* 4KB */
#define FLASH_PAGE_SIZE (8) /* 8B */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    
    if (size == 0) return 0;
    /* HAL Flash state and configuration */
    const whFlashCb       fcb[1]              = {POSIX_FLASH_FILE_CB};
    posixFlashFileContext fctx[1] = {0};
    posixFlashFileConfig  cfg[1]  = {{
          .filename       = "fuzz.bin",
          .partition_size = 16384,
          .erased_byte    = (~(uint8_t)0),
    }};

    uint32_t partition_units = 0;

    fcb->Init(fctx, cfg);

    partition_units = wh_FlashUnit_Bytes2Units(fcb->PartitionSize(fctx)) ;

    /* Unlock the first partition */
    wh_FlashUnit_WriteUnlock(fcb, fctx, 0, partition_units);

    /* Erase the first partition */
    wh_FlashUnit_Erase(fcb, fctx, 0, partition_units);

    /* Fuzz */
    int multiplier = (data[0] > 0) ? data[0]: 1;
    wh_FlashUnit_ProgramBytes(fcb, fctx, multiplier * WHFU_BYTES_PER_UNIT, size, data);
    
    /* Read back and verify the data */
    uint8_t read_bytes[size];
    memset(read_bytes, 0, sizeof(read_bytes));
    wh_FlashUnit_ReadBytes(fcb, fctx, multiplier * WHFU_BYTES_PER_UNIT, size, read_bytes);
    

    /* Compare written and read data */
    if (memcmp(data, read_bytes, size) != 0) {
        printf("Data mismatch between written and read\n");
        /* abort(); */
    }

    /* Erase again the first partition */
    wh_FlashUnit_Erase(fcb, fctx, 0, partition_units);

    /* Lock the first partition */
    wh_FlashUnit_WriteLock(fcb, fctx, 0, partition_units);

    fcb->Cleanup(fctx);

    unlink(cfg[0].filename);
    return 0;
}

#endif /* WOLFHSM_CFG_ENABLE_SERVER */

