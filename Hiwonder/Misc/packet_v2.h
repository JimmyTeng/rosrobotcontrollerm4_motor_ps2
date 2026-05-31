#ifndef __PACKET_V2_H_
#define __PACKET_V2_H_

#include <stdint.h>
#include "packet.h"

#define PACKET_V2_VERSION           0x02u
#define PACKET_V2_MAX_DATA          512u
#define PACKET_V2_MAX_LOGICAL       (8u + PACKET_V2_MAX_DATA + 1u)
#define PACKET_V2_MAX_WIRE          (PACKET_V2_MAX_LOGICAL + (PACKET_V2_MAX_LOGICAL / 254u) + 2u)

#define PACKET_FUNC_TELEMETRY       10u
#define PACKET_V2_FLAG_DIR_HOST_TO_MCU  0x01u

#define PACKET_TELEMETRY_SCHEMA_V3  0x03u
#define PACKET_TELEMETRY_SIZE_V3    169u

struct PacketController;

void packet_v2_init(struct PacketController *self);
void packet_v2_recv(struct PacketController *self);
int packet_v2_transmit(struct PacketController *self, uint8_t func, void *data, size_t data_len);

#endif
