/**
 * Pyintel Lux — Core frame assembly implementation.
 * Platform-agnostic. No heap. No stdio.
 */

#include "lux/lux.h"
#include <string.h>

/* ── CRC-16/CCITT-FALSE ──────────────────────────────────────────────── */
static uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

/* ── Internal frame builder ──────────────────────────────────────────── */
static lux_status_t emit_frame(lux_ctx_t *ctx, uint16_t sym_id,
                                lux_payload_type_t ptype,
                                const uint8_t *payload, uint8_t plen) {
    if (!ctx || !ctx->write) return LUX_ERR_NULL;

    uint8_t hdr[LUX_HEADER_SIZE];
    hdr[0] = LUX_SYNC_0;
    hdr[1] = LUX_SYNC_1;
    hdr[2] = sym_id & 0xFF;
    hdr[3] = (sym_id >> 8) & 0xFF;
    uint32_t ts = ctx->get_time_us ? ctx->get_time_us() : 0;
    memcpy(&hdr[4], &ts, 4);
    hdr[8] = (uint8_t)ptype;
    hdr[9] = plen;
    uint16_t crc = crc16(hdr, 10);
    hdr[10] = crc & 0xFF;
    hdr[11] = (crc >> 8) & 0xFF;

    lux_status_t s = ctx->write(hdr, LUX_HEADER_SIZE, ctx->write_ctx);
    if (s != LUX_OK) return s;
    if (plen > 0 && payload)
        s = ctx->write(payload, plen, ctx->write_ctx);
    return s;
}

/* ── Public API ──────────────────────────────────────────────────────── */
lux_status_t lux_init(lux_ctx_t *ctx, lux_write_fn write, void *write_ctx,
                       uint32_t (*get_time_us)(void)) {
    if (!ctx) return LUX_ERR_NULL;
    ctx->write       = write;
    ctx->write_ctx   = write_ctx;
    ctx->get_time_us = get_time_us;
    return LUX_OK;
}

lux_status_t lux_emit_none(lux_ctx_t *ctx, uint16_t sym) {
    return emit_frame(ctx, sym, LUX_TYPE_NONE, NULL, 0);
}
lux_status_t lux_emit_u8(lux_ctx_t *ctx, uint16_t sym, uint8_t v) {
    return emit_frame(ctx, sym, LUX_TYPE_U8, &v, 1);
}
lux_status_t lux_emit_u16(lux_ctx_t *ctx, uint16_t sym, uint16_t v) {
    return emit_frame(ctx, sym, LUX_TYPE_U16, (uint8_t *)&v, 2);
}
lux_status_t lux_emit_u32(lux_ctx_t *ctx, uint16_t sym, uint32_t v) {
    return emit_frame(ctx, sym, LUX_TYPE_U32, (uint8_t *)&v, 4);
}
lux_status_t lux_emit_i32(lux_ctx_t *ctx, uint16_t sym, int32_t v) {
    return emit_frame(ctx, sym, LUX_TYPE_I32, (uint8_t *)&v, 4);
}
lux_status_t lux_emit_f32(lux_ctx_t *ctx, uint16_t sym, float v) {
    return emit_frame(ctx, sym, LUX_TYPE_F32, (uint8_t *)&v, 4);
}
lux_status_t lux_emit_bytes(lux_ctx_t *ctx, uint16_t sym,
                             const uint8_t *data, uint8_t len) {
    return emit_frame(ctx, sym, LUX_TYPE_BYTES, data, len);
}
