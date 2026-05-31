/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "global_conf.h"
#if ENABLE_LVGL
#include "lvgl.h"
#endif
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
typedef StaticTimer_t osStaticTimerDef_t;
typedef StaticEventGroup_t osStaticEventGroupDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
uint32_t defaultTaskBuffer[ 128 ];
osStaticThreadDef_t defaultTaskControlBlock;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .cb_mem = &defaultTaskControlBlock,
  .cb_size = sizeof(defaultTaskControlBlock),
  .stack_mem = &defaultTaskBuffer[0],
  .stack_size = sizeof(defaultTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for imu_task */
osThreadId_t imu_taskHandle;
uint32_t imu_taskBuffer[ 384 ];
osStaticThreadDef_t imu_taskControlBlock;
const osThreadAttr_t imu_task_attributes = {
  .name = "imu_task",
  .cb_mem = &imu_taskControlBlock,
  .cb_size = sizeof(imu_taskControlBlock),
  .stack_mem = &imu_taskBuffer[0],
  .stack_size = sizeof(imu_taskBuffer),
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for packet_tx_task */
osThreadId_t packet_tx_taskHandle;
uint32_t packet_tx_taskBuffer[ 128 ];
osStaticThreadDef_t packet_tx_taskControlBlock;
const osThreadAttr_t packet_tx_task_attributes = {
  .name = "packet_tx_task",
  .cb_mem = &packet_tx_taskControlBlock,
  .cb_size = sizeof(packet_tx_taskControlBlock),
  .stack_mem = &packet_tx_taskBuffer[0],
  .stack_size = sizeof(packet_tx_taskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for packet_rx_task */
osThreadId_t packet_rx_taskHandle;
uint32_t packet_rx_taskBuffer[ 192 ];
osStaticThreadDef_t packet_rx_taskControlBlock;
const osThreadAttr_t packet_rx_task_attributes = {
  .name = "packet_rx_task",
  .cb_mem = &packet_rx_taskControlBlock,
  .cb_size = sizeof(packet_rx_taskControlBlock),
  .stack_mem = &packet_rx_taskBuffer[0],
  .stack_size = sizeof(packet_rx_taskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for app_task */
osThreadId_t app_taskHandle;
uint32_t app_taskBuffer[ 256 ];
osStaticThreadDef_t app_taskControlBlock;
const osThreadAttr_t app_task_attributes = {
  .name = "app_task",
  .cb_mem = &app_taskControlBlock,
  .cb_size = sizeof(app_taskControlBlock),
  .stack_mem = &app_taskBuffer[0],
  .stack_size = sizeof(app_taskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for packet_tx_queue */
osMessageQueueId_t packet_tx_queueHandle;
uint8_t packet_tx_queueBuffer[ 32 * sizeof( void* ) ];
osStaticMessageQDef_t packet_tx_queueControlBlock;
const osMessageQueueAttr_t packet_tx_queue_attributes = {
  .name = "packet_tx_queue",
  .cb_mem = &packet_tx_queueControlBlock,
  .cb_size = sizeof(packet_tx_queueControlBlock),
  .mq_mem = &packet_tx_queueBuffer,
  .mq_size = sizeof(packet_tx_queueBuffer)
};
/* Definitions for moving_ctrl_queue */
osMessageQueueId_t moving_ctrl_queueHandle;
uint8_t moving_ctrl_queueBuffer[ 32 * sizeof( char ) ];
osStaticMessageQDef_t moving_ctrl_queueControlBlock;
const osMessageQueueAttr_t moving_ctrl_queue_attributes = {
  .name = "moving_ctrl_queue",
  .cb_mem = &moving_ctrl_queueControlBlock,
  .cb_size = sizeof(moving_ctrl_queueControlBlock),
  .mq_mem = &moving_ctrl_queueBuffer,
  .mq_size = sizeof(moving_ctrl_queueBuffer)
};
/* Definitions for button_timer */
osTimerId_t button_timerHandle;
osStaticTimerDef_t button_timerControlBlock;
const osTimerAttr_t button_timer_attributes = {
  .name = "button_timer",
  .cb_mem = &button_timerControlBlock,
  .cb_size = sizeof(button_timerControlBlock),
};
/* Definitions for led_timer */
osTimerId_t led_timerHandle;
osStaticTimerDef_t led_timerControlBlock;
const osTimerAttr_t led_timer_attributes = {
  .name = "led_timer",
  .cb_mem = &led_timerControlBlock,
  .cb_size = sizeof(led_timerControlBlock),
};
/* Definitions for buzzer_timer */
osTimerId_t buzzer_timerHandle;
osStaticTimerDef_t buzzer_timerControlBlock;
const osTimerAttr_t buzzer_timer_attributes = {
  .name = "buzzer_timer",
  .cb_mem = &buzzer_timerControlBlock,
  .cb_size = sizeof(buzzer_timerControlBlock),
};
/* Definitions for battery_check_timer */
osTimerId_t battery_check_timerHandle;
osStaticTimerDef_t battery_check_timerControlBlock;
const osTimerAttr_t battery_check_timer_attributes = {
  .name = "battery_check_timer",
  .cb_mem = &battery_check_timerControlBlock,
  .cb_size = sizeof(battery_check_timerControlBlock),
};
/* Definitions for packet_tx_idle */
osSemaphoreId_t packet_tx_idleHandle;
const osSemaphoreAttr_t packet_tx_idle_attributes = {
  .name = "packet_tx_idle"
};
/* Definitions for packet_rx_not_empty */
osSemaphoreId_t packet_rx_not_emptyHandle;
const osSemaphoreAttr_t packet_rx_not_empty_attributes = {
  .name = "packet_rx_not_empty"
};
/* Definitions for IMU_data_ready */
osSemaphoreId_t IMU_data_readyHandle;
const osSemaphoreAttr_t IMU_data_ready_attributes = {
  .name = "IMU_data_ready"
};
/* Definitions for spi_tx_finished */
osSemaphoreId_t spi_tx_finishedHandle;
const osSemaphoreAttr_t spi_tx_finished_attributes = {
  .name = "spi_tx_finished"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void imu_task_entry(void *argument);
void packet_tx_task_entry(void *argument);
void packet_rx_task_entry(void *argument);
void sbus_rx_task_entry(void *argument);
void gui_task_entry(void *argument);
void app_task_entry(void *argument);
void bluetooth_task_entry(void *argument);
void button_timer_callback(void *argument);
void led_timer_callback(void *argument);
void lvgl_timer_callback(void *argument);
void buzzer_timer_callback(void *argument);
void battery_check_timer_callback(void *argument);

extern void MX_USB_HOST_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationTickHook(void);

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
    task. It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()). If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
#if ENABLE_LVGL
    lv_tick_inc(1);
#endif
}
/* USER CODE END 3 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of packet_tx_idle */
  packet_tx_idleHandle = osSemaphoreNew(1, 0, &packet_tx_idle_attributes);
  packet_rx_not_emptyHandle = osSemaphoreNew(1, 0, &packet_rx_not_empty_attributes);
  IMU_data_readyHandle = osSemaphoreNew(1, 0, &IMU_data_ready_attributes);
  spi_tx_finishedHandle = osSemaphoreNew(1, 0, &spi_tx_finished_attributes);

  button_timerHandle = osTimerNew(button_timer_callback, osTimerPeriodic, NULL, &button_timer_attributes);
  led_timerHandle = osTimerNew(led_timer_callback, osTimerPeriodic, NULL, &led_timer_attributes);
  buzzer_timerHandle = osTimerNew(buzzer_timer_callback, osTimerPeriodic, NULL, &buzzer_timer_attributes);
  battery_check_timerHandle = osTimerNew(battery_check_timer_callback, osTimerPeriodic, NULL, &battery_check_timer_attributes);

  packet_tx_queueHandle = osMessageQueueNew(32, sizeof(void*), &packet_tx_queue_attributes);
  moving_ctrl_queueHandle = osMessageQueueNew(32, sizeof(char), &moving_ctrl_queue_attributes);

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  imu_taskHandle = osThreadNew(imu_task_entry, NULL, &imu_task_attributes);
  packet_tx_taskHandle = osThreadNew(packet_tx_task_entry, NULL, &packet_tx_task_attributes);
  packet_rx_taskHandle = osThreadNew(packet_rx_task_entry, NULL, &packet_rx_task_attributes);
  app_taskHandle = osThreadNew(app_task_entry, NULL, &app_task_attributes);
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN StartDefaultTask */
    /* Infinite loop */
    for(;;) {
        osDelay(1);
    }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_imu_task_entry */
/**
* @brief Function implementing the imu_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_imu_task_entry */
__weak void imu_task_entry(void *argument)
{
  /* USER CODE BEGIN imu_task_entry */
    /* Infinite loop */
    for(;;) {
        osDelay(1);
    }
  /* USER CODE END imu_task_entry */
}

/* USER CODE BEGIN Header_packet_tx_task_entry */
/**
* @brief Function implementing the packet_tx_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_packet_tx_task_entry */
__weak void packet_tx_task_entry(void *argument)
{
  /* USER CODE BEGIN packet_tx_task_entry */
    /* Infinite loop */
    for(;;) {
        osDelay(1);
    }
  /* USER CODE END packet_tx_task_entry */
}

/* USER CODE BEGIN Header_packet_rx_task_entry */
/**
* @brief Function implementing the packet_rx_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_packet_rx_task_entry */
__weak void packet_rx_task_entry(void *argument)
{
  /* USER CODE BEGIN packet_rx_task_entry */
    /* Infinite loop */
    for(;;) {
        osDelay(1);
    }
  /* USER CODE END packet_rx_task_entry */
}

/* USER CODE BEGIN Header_sbus_rx_task_entry */
/**
* @brief Function implementing the sbus_rx_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_sbus_rx_task_entry */
__weak void sbus_rx_task_entry(void *argument)
{
  /* USER CODE BEGIN sbus_rx_task_entry */
    /* Infinite loop */
    for(;;) {
        osDelay(1);
    }
  /* USER CODE END sbus_rx_task_entry */
}

/* USER CODE BEGIN Header_gui_task_entry */
/**
* @brief Function implementing the gui_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_gui_task_entry */
__weak void gui_task_entry(void *argument)
{
  /* USER CODE BEGIN gui_task_entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END gui_task_entry */
}

/* USER CODE BEGIN Header_app_task_entry */
/**
* @brief Function implementing the app_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_app_task_entry */
__weak void app_task_entry(void *argument)
{
  /* USER CODE BEGIN app_task_entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END app_task_entry */
}

/* USER CODE BEGIN Header_bluetooth_task_entry */
/**
* @brief Function implementing the bluetooth_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_bluetooth_task_entry */
__weak void bluetooth_task_entry(void *argument)
{
  /* USER CODE BEGIN bluetooth_task_entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END bluetooth_task_entry */
}

/* button_timer_callback function */
__weak void button_timer_callback(void *argument)
{
  /* USER CODE BEGIN button_timer_callback */

  /* USER CODE END button_timer_callback */
}

/* led_timer_callback function */
__weak void led_timer_callback(void *argument)
{
  /* USER CODE BEGIN led_timer_callback */

  /* USER CODE END led_timer_callback */
}

/* lvgl_timer_callback function */
__weak void lvgl_timer_callback(void *argument)
{
  /* USER CODE BEGIN lvgl_timer_callback */

  /* USER CODE END lvgl_timer_callback */
}

/* buzzer_timer_callback function */
__weak void buzzer_timer_callback(void *argument)
{
  /* USER CODE BEGIN buzzer_timer_callback */

  /* USER CODE END buzzer_timer_callback */
}

/* battery_check_timer_callback function */
__weak void battery_check_timer_callback(void *argument)
{
  /* USER CODE BEGIN battery_check_timer_callback */

  /* USER CODE END battery_check_timer_callback */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

