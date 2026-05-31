#include "packet_v2.h"
#include "cobs.h"
#include "checksum.h"
#include "lwmem.h"
#include "lwmem_porting.h"
#include "string.h"

#define PACKET_V2_RX_COBS_MAX  600u

static uint16_t tx_seq;
static uint8_t cobs_rx_acc[PACKET_V2_RX_COBS_MAX];
static size_t cobs_rx_len;

static int build_logical(uint8_t func, const void *data, size_t data_len,
                         uint8_t flags, uint8_t *out, size_t out_cap, size_t *out_len)
{
    if(data_len > PACKET_V2_MAX_DATA || out_cap < 8 + data_len + 1) {
        return -1;
    }
    uint16_t seq = tx_seq++;
    out[0] = PACKET_V2_VERSION;
    out[1] = flags;
    out[2] = (uint8_t)(seq & 0xFF);
    out[3] = (uint8_t)(seq >> 8);
    out[4] = func;
    out[5] = (uint8_t)(data_len & 0xFF);
    out[6] = (uint8_t)(data_len >> 8);
    if(data_len > 0 && data != NULL) {
        memcpy(&out[7], data, data_len);
    }
    out[7 + data_len] = checksum_crc8(out, (int)(7 + data_len));
    *out_len = 8 + data_len;
    return 0;
}

static void dispatch_logical(struct PacketController *self, const uint8_t *logical, size_t n)
{
    if(n < 8 || logical[0] != PACKET_V2_VERSION) {
        return;
    }
    uint16_t data_len = (uint16_t)logical[5] | ((uint16_t)logical[6] << 8);
    if((size_t)(8 + data_len) != n) {
        return;
    }
    if(logical[7 + data_len] != checksum_crc8(logical, (int)(7 + data_len))) {
        return;
    }
    /* 忽略上行/遥测帧，避免串口回显把 TELEMETRY 误解析成 MOTOR 等命令 */
    if((logical[1] & PACKET_V2_FLAG_DIR_HOST_TO_MCU) == 0) {
        return;
    }
    if(logical[4] == PACKET_FUNC_TELEMETRY) {
        return;
    }
    if(data_len > 255) {
        return;
    }
    memset(&self->frame, 0, sizeof(self->frame));
    self->frame.function = logical[4];
    self->frame.data_length = (uint8_t)data_len;
    if(data_len > 0) {
        memcpy(self->frame.data_and_checksum, &logical[7], data_len);
    }
    if(self->frame.function < PACKET_FUNC_NONE && self->handles[self->frame.function] != NULL) {
        self->handles[self->frame.function](&self->frame);
    }
}

void packet_v2_init(struct PacketController *self)
{
    (void)self;
    tx_seq = 0;
    cobs_rx_len = 0;
}

void packet_v2_recv(struct PacketController *self)
{
    uint8_t chunk[PACKET_PARSE_BUFFER_SIZE];
    size_t avail = lwrb_get_full(self->rx_fifo);
    while(avail > 0) {
        size_t n = avail > PACKET_PARSE_BUFFER_SIZE ? PACKET_PARSE_BUFFER_SIZE : avail;
        n = lwrb_read(self->rx_fifo, chunk, n);
        for(size_t i = 0; i < n; ++i) {
            uint8_t b = chunk[i];
            if(b == 0) {
                if(cobs_rx_len > 0) {
                    uint8_t logical[PACKET_V2_MAX_LOGICAL];
                    size_t decoded = cobs_decode(cobs_rx_acc, cobs_rx_len, logical);
                    if(decoded > 0) {
                        dispatch_logical(self, logical, decoded);
                    }
                }
                cobs_rx_len = 0;
            } else {
                if(cobs_rx_len < PACKET_V2_RX_COBS_MAX) {
                    cobs_rx_acc[cobs_rx_len++] = b;
                } else {
                    cobs_rx_len = 0;
                }
            }
        }
        avail = lwrb_get_full(self->rx_fifo);
    }
}

int packet_v2_transmit(struct PacketController *self, uint8_t func, void *data, size_t data_len)
{
    if(self == NULL || self->send_wire == NULL) {
        return -4;
    }
    uint8_t logical[PACKET_V2_MAX_LOGICAL];
    size_t logical_len = 0;
    if(build_logical(func, data, data_len, 0x00, logical, sizeof(logical), &logical_len) != 0) {
        return -1;
    }
    uint8_t enc_buf[PACKET_V2_MAX_WIRE];
    size_t enc = cobs_encode(logical, logical_len, enc_buf);
    if(enc + 1u > sizeof(enc_buf)) {
        return -2;
    }
    enc_buf[enc++] = 0;
    if(self->send_wire(self, enc_buf, (uint16_t)enc) != 0) {
        return -3;
    }
    return 0;
}
