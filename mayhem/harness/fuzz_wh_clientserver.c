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
#include <stdint.h>
#include <stdio.h>  /* For printf */
#include <string.h> /* For memset, memcpy */

#include "wolfhsm/wh_settings.h"

#include "wh_test_common.h"
#include "wolfhsm/wh_error.h"

#include "wolfhsm/wh_comm.h"
#include "wolfhsm/wh_transport_mem.h"

#include "wolfhsm/wh_nvm.h"
#include "wolfhsm/wh_nvm_flash.h"
#include "wolfhsm/wh_flash_ramsim.h"

#include "wolfhsm/wh_server.h"
#include "wolfhsm/wh_message.h"
#include "wolfhsm/wh_message_comm.h"

#include "wolfhsm/wh_client.h"

#define BUFFER_SIZE 4096
#define REQ_SIZE 32
#define RESP_SIZE 64
#define FLASH_RAM_SIZE (1024 * 1024) /* 1MB */
#define FLASH_SECTOR_SIZE (128 * 1024) /* 128KB */
#define FLASH_PAGE_SIZE (8) /* 8B */

/* Pointer to a local server context so a connect callback can access it. Should
 * be set before calling wh_ClientInit() */
static whServerContext* clientServerSequentialTestServerCtx = NULL;

static int _clientServerSequentialTestConnectCb(void*           context,
                                                whCommConnected connected)
{
    (void)context;

    if (clientServerSequentialTestServerCtx == NULL) {
        WH_ERROR_PRINT("Client connect callback server context is NULL\n");
        assert(0);
    }

    /* Set server connect flag. In a "real" system, this should signal the
     * server via out-of-band mechanism. The server app is responsible for
     * receiving this signal and calling wh_Server_SetConnected() */
    return wh_Server_SetConnected(clientServerSequentialTestServerCtx,
                                  connected);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int ret = 0;
    whTestNvmBackendType nvmType = WH_NVM_TEST_BACKEND_FLASH;
    /* Transport memory configuration */
    uint8_t              req[BUFFER_SIZE];
    uint8_t              resp[BUFFER_SIZE];
    whTransportMemConfig tmcf[1] = {{
        .req       = (whTransportMemCsr*)req,
        .req_size  = sizeof(req),
        .resp      = (whTransportMemCsr*)resp,
        .resp_size = sizeof(resp),
    }};

    /* Client configuration/contexts */
    whTransportClientCb         tccb[1]    = {WH_TRANSPORT_MEM_CLIENT_CB};
    whTransportMemClientContext tmcc[1]    = {0};
    whCommClientConfig          cc_conf[1] = {{
                 .transport_cb      = tccb,
                 .transport_context = (void*)tmcc,
                 .transport_config  = (void*)tmcf,
                 .client_id         = 123,
                 .connect_cb        = _clientServerSequentialTestConnectCb,
    }};

    whClientContext client[1] = {0};
    whClientConfig c_conf[1] = {{
        .comm = cc_conf,
    }};

    /* Server configuration/contexts */
    whTransportServerCb         tscb[1]    = {WH_TRANSPORT_MEM_SERVER_CB};
    whTransportMemServerContext tmsc[1]    = {0};
    whCommServerConfig          cs_conf[1] = {{
                 .transport_cb      = tscb,
                 .transport_context = (void*)tmsc,
                 .transport_config  = (void*)tmcf,
                 .server_id         = 124,
    }};

    /* RamSim Flash state and configuration */
    uint8_t memory[FLASH_RAM_SIZE] = {0};
    whFlashRamsimCtx fc[1]      = {0};
    whFlashRamsimCfg fc_conf[1] = {{
        .size       = FLASH_RAM_SIZE,
        .sectorSize = FLASH_SECTOR_SIZE,
        .pageSize   = FLASH_PAGE_SIZE,
        .erasedByte = ~(uint8_t)0,
        .memory     = memory,
    }};
    const whFlashCb  fcb[1]     = {WH_FLASH_RAMSIM_CB};

    whTestNvmBackendUnion nvm_setup;
    whNvmConfig           n_conf[1];
    whNvmContext nvm[1]    = {{0}};

    whTest_NvmCfgBackend(nvmType, &nvm_setup, n_conf, fc_conf, fc, fcb);

    whServerCryptoContext crypto[1] = {0};

    whServerConfig s_conf[1] = {{
        .comm_config = cs_conf,
        .nvm         = nvm,
        .crypto      = crypto,
    }};
    whServerContext server[1] = {0};
    whCommConnected server_connected = WH_COMM_DISCONNECTED;

    /* Expose the server context to our client connect callback */
    clientServerSequentialTestServerCtx = server;

    wolfCrypt_Init();
    wc_InitRng_ex(crypto->rng, NULL, INVALID_DEVID);
    wh_Nvm_Init(nvm, n_conf);

    /* wh_Server_GetConnected(server, &server_connected); */
    /* assert(WH_COMM_DISCONNECTED == server_connected); */
    /* assert(WH_ERROR_NOTREADY == wh_Server_HandleRequestMessage(server)); */

    wh_Server_Init(server, s_conf);
    wh_Client_Init(client, c_conf);

    wh_Server_GetConnected(server, &server_connected);
    /* assert(WH_COMM_CONNECTED == server_connected); */

    char     recv_buffer[WOLFHSM_CFG_COMM_DATA_LEN] = {0};
    char     send_buffer[WOLFHSM_CFG_COMM_DATA_LEN] = {0};
    uint16_t send_len                 = 0;
    uint16_t recv_len                 = 0;

    /* Fill send_buffer with fuzz data */ 
    size_t copy_size = (size >= sizeof(send_buffer)) ? sizeof(send_buffer) : size; 
    memcpy(send_buffer, data, copy_size);
    send_len = (uint16_t)(copy_size);

    /* Basic echo sequence using fuzz data */
    wh_Client_EchoRequest(client, send_len, send_buffer);
    wh_Server_HandleRequestMessage(server);
    wh_Client_EchoResponse(client, &recv_len, recv_buffer);

    /* original correctness checks */
    /* assert(recv_len == send_len); */
    /* assert(strncmp(recv_buffer, send_buffer, recv_len) == 0); */

    /* Tear down */
    wh_Client_CommCloseRequest(client);
    wh_Server_HandleRequestMessage(server);
    wh_Client_CommCloseResponse(client);

    wh_Server_Cleanup(server);
    wh_Client_Cleanup(client);
    wh_Nvm_Cleanup(nvm);

    wc_FreeRng(crypto->rng);
    wolfCrypt_Cleanup();

    return ret;
}