/**
 * @file packet_portting.c
 * @brief 串口协议接口实现 (USART3, 1Mbps)
 */

#include "global.h"
#include "lwrb.h"
#include "usart.h"
#include "packet.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include "lwmem_porting.h"

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

void packet_init(void)
{
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
}

static int send_packet(struct PacketController *self, struct PacketRawFrame *frame)
{
    return osMessageQueuePut(packet_tx_queueHandle, &frame, 0, 10);
}

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
        packet_recv(&packet_controller);
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
            HAL_UART_Transmit_DMA(&huart3, (uint8_t *)packet_controller.tx_dma_buffer,
                packet_controller.tx_dma_buffer->data_length + 5);
        }
    }
}

static void packet_dma_transmit_finished(UART_HandleTypeDef *huart)
{
    lwmem_free(packet_controller.tx_dma_buffer);
    osStatus_t status = osMessageQueueGet(packet_tx_queueHandle,
        &packet_controller.tx_dma_buffer, NULL, 0);
    if(osOK == status) {
        HAL_UART_RegisterCallback(huart, HAL_UART_TX_COMPLETE_CB_ID,
            packet_dma_transmit_finished);
        HAL_UART_Transmit_DMA(huart, (uint8_t *)packet_controller.tx_dma_buffer,
            packet_controller.tx_dma_buffer->data_length + 5);
    } else {
        osSemaphoreRelease(packet_tx_idleHandle);
    }
}
