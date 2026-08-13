/**
 * Pyintel Lux — Core C API
 * Single-header public interface for lux-emb (bare-metal / no_std targets).
 * Include this in your firmware. No heap allocation. No stdio dependency.
 */

#ifndef PYINTEL_LUX_H
#define PYINTEL_LUX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Wire constants ─────────────────────────────────────────────────── */
#define LUX_SYNC_0       0x4C   /* 'L' */
#define LUX_SYNC_1       0x58   /* 'X' */
#define LUX_HEADER_SIZE  14

/* ── Reserved system symbol IDs ─────────────────────────────────────── */
#define LUX_SYM_HEARTBEAT        0x0001
#define LUX_SYM_RESET            0x0002
#define LUX_SYM_OVERFLOW         0x0003
#define LUX_SYM_TRANSPORT_SWITCH 0x0004

/* ── Payload type codes ──────────────────────────────────────────────── */
typedef enum {
    LUX_TYPE_NONE   = 0x00,
    LUX_TYPE_U8     = 0x01,
    LUX_TYPE_U16    = 0x02,
    LUX_TYPE_U32    = 0x03,
    LUX_TYPE_I32    = 0x04,
    LUX_TYPE_F32    = 0x05,
    LUX_TYPE_BYTES  = 0x06,
    LUX_TYPE_STRREF = 0x07,
} lux_payload_type_t;

/* ── Status codes ────────────────────────────────────────────────────── */
typedef enum {
    LUX_OK           =  0,
    LUX_ERR_NULL     = -1,
    LUX_ERR_OVERFLOW = -2,
    LUX_ERR_TRANSPORT= -3,
} lux_status_t;

/* ── Frame header struct (maps directly onto wire bytes) ─────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  sync[2];        /* 0x4C 0x58                         */
    uint16_t seq_num;        /* monotonic sequence counter        */
    uint16_t symbol_id;      /* compile-time token (little-endian)*/
    uint32_t timestamp_us;   /* monotonic µs clock                */
    uint8_t  payload_type;   /* lux_payload_type_t                */
    uint8_t  payload_len;    /* bytes following this header        */
    uint16_t crc16;          /* CRC-16/CCITT over bytes [0..11]   */
} lux_frame_header_t;

/* ── Transport write callback (user-provided) ────────────────────────── */
typedef lux_status_t (*lux_write_fn)(const uint8_t *buf, size_t len, void *ctx);

#define LUX_TX_BUFFER_SIZE  256

/* ── Context ─────────────────────────────────────────────────────────── */
typedef struct {
    lux_write_fn write;         /* platform transport write function */
    void        *write_ctx;     /* passed through to write()         */
    uint32_t   (*get_time_us)(void); /* monotonic µs clock source    */
    uint16_t     seq_num;       /* monotonic sequence counter        */
    uint8_t      tx_buf[LUX_TX_BUFFER_SIZE]; /* internal batch buffer*/
    size_t       tx_buf_len;    /* bytes queued in tx_buf            */
} lux_ctx_t;

/* ── API ─────────────────────────────────────────────────────────────── */
lux_status_t lux_init(lux_ctx_t *ctx, lux_write_fn write, void *write_ctx,
                       uint32_t (*get_time_us)(void));
lux_status_t lux_flush(lux_ctx_t *ctx);

lux_status_t lux_emit_none(lux_ctx_t *ctx, uint16_t symbol_id);
lux_status_t lux_emit_u8  (lux_ctx_t *ctx, uint16_t symbol_id, uint8_t  val);
lux_status_t lux_emit_u16 (lux_ctx_t *ctx, uint16_t symbol_id, uint16_t val);
lux_status_t lux_emit_u32 (lux_ctx_t *ctx, uint16_t symbol_id, uint32_t val);
lux_status_t lux_emit_i32 (lux_ctx_t *ctx, uint16_t symbol_id, int32_t  val);
lux_status_t lux_emit_f32 (lux_ctx_t *ctx, uint16_t symbol_id, float    val);
lux_status_t lux_emit_bytes(lux_ctx_t *ctx, uint16_t symbol_id,
                             const uint8_t *data, uint8_t len);

/* ── Convenience macros ──────────────────────────────────────────────── */
#define LUX_TRACE(ctx, sym)          lux_emit_none((ctx), (sym))
#define LUX_TRACE_U32(ctx, sym, v)   lux_emit_u32((ctx), (sym), (v))
#define LUX_TRACE_F32(ctx, sym, v)   lux_emit_f32((ctx), (sym), (v))

#ifdef __cplusplus
}
#endif

#endif /* PYINTEL_LUX_H */
