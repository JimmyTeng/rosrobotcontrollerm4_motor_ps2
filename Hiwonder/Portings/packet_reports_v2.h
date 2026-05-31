#ifndef __PACKET_REPORTS_V2_H_
#define __PACKET_REPORTS_V2_H_

#include <stdint.h>

#pragma pack(push, 1)

#define PACKET_V2_MOTOR_CHANNEL_SIZE  20u
#define PACKET_V2_MOTOR_BLOCK_SIZE    82u
#define PACKET_V2_POWER_BLOCK_SIZE    32u
#define PACKET_V2_IMU_RAW_SIZE        16u
#define PACKET_V2_IMU_FUSED_SIZE      28u
#define PACKET_V2_BUTTONS_SIZE        10u

typedef struct {
    uint8_t motor_id;
    uint8_t flags;
    int16_t pwm;
    float rps_cmd;
    float rps_meas;
    uint16_t current_ma;
    uint16_t voltage_mv;
    uint16_t reserved;
    uint8_t pad[2];
} PacketV2MotorChannel_TypeDef;

typedef struct {
    uint8_t motor_num;
    uint8_t motor_channel_mask;
    PacketV2MotorChannel_TypeDef channels[4];
} PacketV2MotorsBlock_TypeDef;

typedef struct {
    uint16_t valid_mask;
    uint16_t reserved;
    uint32_t vin_mv;
    int32_t vin_ma;
    uint32_t vout_mv;
    int32_t vout_ma;
    uint32_t vmotor_mv;
    int32_t imotor_bus_ma;
    uint32_t reserved2;
} PacketV2PowerBlock_TypeDef;

typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    uint16_t acc_ssvt;
    uint16_t gyr_ssvt;
} PacketV2ImuRawBlock_TypeDef;

typedef struct {
    float qw;
    float qx;
    float qy;
    float qz;
    float roll;
    float pitch;
    float yaw;
} PacketV2ImuFusedBlock_TypeDef;

typedef struct {
    uint8_t key_id;
    uint8_t pin_level;
    uint8_t stage;
    uint8_t event_latch;
} PacketV2KeyChannel_TypeDef;

typedef struct {
    uint8_t key_num;
    uint8_t reserved;
    PacketV2KeyChannel_TypeDef keys[2];
} PacketV2ButtonsBlock_TypeDef;

typedef struct {
    uint8_t schema_id;
    PacketV2MotorsBlock_TypeDef motors;
    PacketV2PowerBlock_TypeDef power;
    PacketV2ImuRawBlock_TypeDef imu_raw;
    PacketV2ImuFusedBlock_TypeDef imu_fused;
    PacketV2ButtonsBlock_TypeDef buttons;
} PacketTelemetryV3_TypeDef;

#pragma pack(pop)

#endif
