/**
 * @brief USART1 (115200) 调试输出：printf 重定向与 V2 遥测统计
 */
#include "debug_uart.h"
#include "global_conf.h"
#include <stdio.h>
#include "usart.h"

#if ENABLE_DEBUG_UART

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &c, 1, 50);
    return ch;
}

#if DEBUG_UART_TELEM_STATS
static uint32_t telem_ok;
static uint32_t telem_fail;
static uint32_t wire_fail;
static uint32_t telem_ticks;
#endif

void debug_uart_boot_banner(void)
{
    printf("\r\n=== RRC USART map ===\r\n");
    printf("USART1 115200 : debug printf + stm32flash (rrc_flash)\r\n");
    printf("USART3 1M     : ROS protocol (host_link)\r\n");
#if USE_PACKET_V2
    printf("Protocol V2   : COBS + TELEMETRY schema 0x03 @ 100Hz\r\n");
#else
    printf("Protocol V1   : AA55 frames\r\n");
#endif
#if DEBUG_UART_TELEM_STATS
    printf("Debug stats   : 1 Hz on USART1 (telem ok/fail, wire queue)\r\n");
#endif
    printf("=====================\r\n");
}

void debug_uart_telemetry_result(int ret)
{
#if DEBUG_UART_TELEM_STATS
    if(ret == 0) {
        telem_ok++;
    } else {
        telem_fail++;
    }
    telem_ticks++;
    if(telem_ticks >= 100u) {
        telem_ticks = 0;
        printf("[telem] ok=%lu fail=%lu wire_q=%lu\r\n",
               (unsigned long)telem_ok,
               (unsigned long)telem_fail,
               (unsigned long)wire_fail);
    }
#else
    (void)ret;
#endif
}

void debug_uart_tx_wire_fail(void)
{
#if DEBUG_UART_TELEM_STATS
    wire_fail++;
#endif
}

void debug_uart_periodic(void)
{
    /* 预留：非遥测路径的周期日志 */
}

#else

int __io_putchar(int ch)
{
    (void)ch;
    return ch;
}

#endif /* ENABLE_DEBUG_UART */
