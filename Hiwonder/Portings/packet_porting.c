/**
 * @file packet_portting.c
 * @brief 串口协议接口实现 (USART3, 1Mbps)
 */

#include "global.h"
#include "global_conf.h"
#include "lwrb.h"
#include "usart.h"
#include "packet.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include "lwmem_porting.h"
#if USE_PACKET_V2
#include "packet_v2.h"
#endif
#include "debug_uart.h"

#define PACKET_RX_FIFO_BUFFER_SIZE 2048
#define PACKET_RX_DMA_BUFFER_SIZE 256

struct PacketController packet_controller;

static void packet_dma_receive_event_callback(UART_HandleTypeDef *huart, uint16_t length);
static void packet_dma_transmit_finished(UART_HandleTypeDef *huart);
static int send_packet(struct PacketController *self, struct PacketRawFrame *frame);
static void packet_uart_error_callback(UART_HandleTypeDef *huart);

extern osSemaphoreId_t packet_tx_idleHandle;
extern osSemaphoreId_t packet_rx_not_emptyHandle;
extern osMessageQueueId_t packet_tx_queueHandle;

#if USE_PACKET_V2
typedef struct {
    uint16_t len;
    uint8_t data[PACKET_V2_MAX_WIRE];
} PacketWireFrame_TypeDef;

static int send_wire(struct PacketController *self, uint8_t *wire, uint16_t len);
#endif

static uint8_t packet_initialized;

void packet_init(void)
{
    if(packet_initialized) {
        return;
    }
    packet_initialized = 1;
    memset(&packet_controller, 0, sizeof(packet_controller));
    packet_controller.state = PACKET_CONTROLLER_STATE_STARTBYTE1;
    packet_controller.data_index = 0;

    static uint8_t rx_dma_buffer1[PACKET_RX_DMA_BUFFER_SIZE];
    static uint8_t rx_dma_buffer2[PACKET_RX_DMA_BUFFER_SIZE];

    packet_controller.rx_dma_buffers[0] = rx_dma_buffer1;
    packet_controller.rx_dma_buffers[1] = rx_dma_buffer2;
    packet_controller.rx_dma_buffer_size = PACKET_RX_DMA_BUFFER_SIZE;
    packet_controller.rx_dma_buffer_index = 0;

    packet_controller.rx_fifo_buffer = LWMEM_CCM_MALLOC(PACKET_RX_FIFO_BUFFER_SIZE);
    packet_controller.rx_fifo = LWMEM_CCM_MALLOC(sizeof(lwrb_t));
    lwrb_init(packet_controller.rx_fifo, packet_controller.rx_fifo_buffer, PACKET_RX_FIFO_BUFFER_SIZE);

    packet_controller.send_packet = send_packet;
#if USE_PACKET_V2
    packet_controller.send_wire = send_wire;
    packet_v2_init(&packet_controller);
#endif
}

static int send_packet(struct PacketController *self, struct PacketRawFrame *frame)
{
    (void)self;
    return osMessageQueuePut(packet_tx_queueHandle, &frame, 0, 10);
}

#if USE_PACKET_V2
static int send_wire(struct PacketController *self, uint8_t *wire, uint16_t len)
{
    (void)self;
    if(len > PACKET_V2_MAX_WIRE) {
        return -1;
    }
    PacketWireFrame_TypeDef *frame = LWMEM_RAM_MALLOC(sizeof(PacketWireFrame_TypeDef));
    if(frame == NULL) {
        return -2;
    }
    frame->len = len;
    memcpy(frame->data, wire, len);
    void *ptr = frame;
    osStatus_t st = osMessageQueuePut(packet_tx_queueHandle, &ptr, 0, 10);
    if(st != osOK) {
        lwmem_free(frame);
        debug_uart_tx_wire_fail();
        return -3;
    }
    return 0;
}
#endif

static void packet_dma_receive_event_callback(UART_HandleTypeDef *huart, uint16_t length)
{
    int cur_index = packet_controller.rx_dma_buffer_index;
    packet_controller.rx_dma_buffer_index ^= 1;
    HAL_UART_AbortReceive(&huart3);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
        packet_controller.rx_dma_buffers[packet_controller.rx_dma_buffer_index],
        PACKET_RX_DMA_BUFFER_SIZE);
    lwrb_write(packet_controller.rx_fifo, packet_controller.rx_dma_buffers[cur_index], length);
    osSemaphoreRelease(packet_rx_not_emptyHandle);
}

void packet_start_recv(void)
{
    HAL_UART_AbortReceive(&huart3);
    HAL_UART_RegisterCallback(&huart3, HAL_UART_ERROR_CB_ID, packet_uart_error_callback);
    HAL_UART_RegisterRxEventCallback(&huart3, packet_dma_receive_event_callback);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
        packet_controller.rx_dma_buffers[packet_controller.rx_dma_buffer_index],
        PACKET_RX_DMA_BUFFER_SIZE);
}

static void packet_uart_error_callback(UART_HandleTypeDef *huart)
{
    (void)huart;
    packet_start_recv();
}

void packet_rx_task_entry(void *argument)
{
    (void)argument;
    packet_init();
    osSemaphoreAcquire(packet_rx_not_emptyHandle, 0);
    __HAL_UNLOCK(&huart3);
    packet_start_recv();
    for(;;) {
        osSemaphoreAcquire(packet_rx_not_emptyHandle, osWaitForever);
#if USE_PACKET_V2
        packet_v2_recv(&packet_controller);
#else
        packet_recv(&packet_controller);
#endif
    }
}

void packet_tx_task_entry(void *argument)
{
    (void)argument;
    osSemaphoreRelease(packet_tx_idleHandle);
    for(;;) {
        osSemaphoreAcquire(packet_tx_idleHandle, osWaitForever);
        osStatus_t status = osMessageQueueGet(packet_tx_queueHandle,
            &packet_controller.tx_dma_buffer, NULL, osWaitForever);
        if(osOK == status) {
            HAL_UART_RegisterCallback(&huart3, HAL_UART_TX_COMPLETE_CB_ID,
                packet_dma_transmit_finished);
#if USE_PACKET_V2
            {
                PacketWireFrame_TypeDef *frame = (PacketWireFrame_TypeDef *)packet_controller.tx_dma_buffer;
                HAL_UART_Transmit_DMA(&huart3, frame->data, frame->len);
            }
#else
            HAL_UART_Transmit_DMA(&huart3, (uint8_t *)packet_controller.tx_dma_buffer,
                packet_controller.tx_dma_buffer->data_length + 5);
#endif
        }
    }
}

static void packet_dma_transmit_finished(UART_HandleTypeDef *huart)
{
#if USE_PACKET_V2
    lwmem_free(packet_controller.tx_dma_buffer);
#else
    lwmem_free(packet_controller.tx_dma_buffer);
#endif
    osStatus_t status = osMessageQueueGet(packet_tx_queueHandle,
        &packet_controller.tx_dma_buffer, NULL, 0);
    if(osOK == status) {
        HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID,
            packet_dma_transmit_finished);
#if USE_PACKET_V2
        {
            PacketWireFrame_TypeDef *frame = (PacketWireFrame_TypeDef *)packet_controller.tx_dma_buffer;
            HAL_UART_Transmit_DMA(huart, frame->data, frame->len);
        }
#else
        HAL_UART_Transmit_DMA(huart, (uint8_t *)packet_controller.tx_dma_buffer,
            packet_controller.tx_dma_buffer->data_length + 5);
#endif
    } else {
        osSemaphoreRelease(packet_tx_idleHandle);
    }
}
