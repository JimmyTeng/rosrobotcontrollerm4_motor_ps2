#ifndef SENSOR_TELEMETRY_H
#define SENSOR_TELEMETRY_H

#include <stdint.h>
#include "global_conf.h"

/* 与 TIM7 编码器测速周期一致：10 ms → 100 Hz */
#define SENSOR_TELEMETRY_PERIOD_MS  10U

void sensor_telemetry_init(void);
void sensor_telemetry_tick_from_isr(void);
void sensor_telemetry_wait_tick(void);
void sensor_telemetry_publish(void);

#if !ENABLE_IMU
void sensor_telemetry_task_start(void);
#endif

#endif
