#ifndef __DEBUG_UART_H__
#define __DEBUG_UART_H__

#include "global_conf.h"

#if ENABLE_DEBUG_UART

void debug_uart_boot_banner(void);
void debug_uart_telemetry_result(int ret);
void debug_uart_tx_wire_fail(void);
void debug_uart_periodic(void);

#else

#define debug_uart_boot_banner()        ((void)0)
#define debug_uart_telemetry_result(r)  ((void)0)
#define debug_uart_tx_wire_fail()       ((void)0)
#define debug_uart_periodic()           ((void)0)

#endif

#endif
