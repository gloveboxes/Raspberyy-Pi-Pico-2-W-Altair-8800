#include "ws.h"

#include "pico/stdlib.h"

#include "lwip/tcp.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define WS_SERVER_PORT 8082
#define WS_MAX_CLIENTS 1
#define WS_RX_BUFFER 1024
#define WS_FRAME_PAYLOAD 96
#define WS_PENDING_BUFFER (WS_FRAME_PAYLOAD + 8)
#define WS_POLL_INTERVAL 2
#define WS_IDLE_TIMEOUT_MS (30 * 60 * 1000)

typedef enum
{
    WS_STATE_IDLE = 0,
    WS_STATE_HANDSHAKE,
    WS_STATE_CONNECTED
} websocket_state_t;

typedef struct
{
    struct tcp_pcb *pcb;
    websocket_state_t state;
    uint8_t rx_buffer[WS_RX_BUFFER];
    size_t rx_len;
    uint8_t pending_buffer[WS_PENDING_BUFFER];
    size_t pending_len;
    size_t pending_offset;
    absolute_time_t last_activity;
} websocket_client_t;

static ws_callbacks_t ws_callbacks;
static bool ws_initialized = false;
static bool ws_running = false;

static struct tcp_pcb *ws_listener = NULL;
static websocket_client_t ws_clients[WS_MAX_CLIENTS];

static bool websocket_setup_listener(void);
static websocket_client_t *websocket_alloc_client(void);
static void websocket_release_client(websocket_client_t *client, bool abort_pcb);
static err_t websocket_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t websocket_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t websocket_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t websocket_poll_cb(void *arg, struct tcp_pcb *tpcb);
static void websocket_err_cb(void *arg, err_t err);
static void websocket_process_buffer(websocket_client_t *client);
static bool websocket_process_handshake(websocket_client_t *client);
static bool websocket_process_frames(websocket_client_t *client);
static int websocket_find_double_crlf(const uint8_t *buffer, size_t length);
static bool websocket_extract_key(const char *request, size_t request_len, char *key, size_t key_len);
static bool websocket_compute_accept_key(const char *client_key, char *accept_key, size_t accept_len);
static bool websocket_handle_frame(websocket_client_t *client, uint8_t opcode, const uint8_t *payload, size_t payload_len);
static size_t ws_collect_output(uint8_t *buffer, size_t max_len);
static size_t websocket_build_frame(uint8_t opcode, const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_size);
static void websocket_flush_clients(void);
static void websocket_prepare_pending_frame(websocket_client_t *client);
static void websocket_push_pending(websocket_client_t *client);

typedef struct
{
    uint32_t state[5];
    uint64_t bitcount;
    uint8_t buffer[64];
    size_t buffer_len;
} ws_sha1_ctx;

static void ws_sha1_init(ws_sha1_ctx *ctx);
static void ws_sha1_update(ws_sha1_ctx *ctx, const uint8_t *data, size_t len);
static void ws_sha1_final(ws_sha1_ctx *ctx, uint8_t digest[20]);
static void websocket_sha1(const uint8_t *data, size_t len, uint8_t digest[20]);
static size_t websocket_base64_encode(const uint8_t *data, size_t len, char *out, size_t out_size);

void ws_init(const ws_callbacks_t *callbacks)
{
    if (!callbacks)
    {
        memset(&ws_callbacks, 0, sizeof(ws_callbacks));
        ws_initialized = false;
        return;
    }

    ws_callbacks = *callbacks;
    ws_initialized = true;
}

bool ws_start(void)
{
    if (!ws_initialized)
    {
        printf("WebSocket server not initialized\n");
        return false;
    }

    if (ws_running)
    {
        return true;
    }

    memset(ws_clients, 0, sizeof(ws_clients));

    bool ok = websocket_setup_listener();
    if (!ok)
    {
        printf("Failed to start WebSocket listener on port %d\n", WS_SERVER_PORT);
        return false;
    }

    ws_running = true;
    printf("WebSocket server listening on port %d\n", WS_SERVER_PORT);
    return true;
}

bool ws_is_running(void)
{
    return ws_running && ws_listener != NULL;
}

void ws_poll(void)
{
    if (!ws_running)
    {
        return;
    }

    websocket_flush_clients();
}

static bool websocket_setup_listener(void)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb)
    {
        return false;
    }

    if (tcp_bind(pcb, IP_ANY_TYPE, WS_SERVER_PORT) != ERR_OK)
    {
        tcp_close(pcb);
        return false;
    }

    ws_listener = tcp_listen_with_backlog(pcb, WS_MAX_CLIENTS);
    if (!ws_listener)
    {
        tcp_close(pcb);
        return false;
    }

    tcp_arg(ws_listener, NULL);
    tcp_accept(ws_listener, websocket_accept);
    return true;
}

static websocket_client_t *websocket_alloc_client(void)
{
    for (size_t i = 0; i < WS_MAX_CLIENTS; ++i)
    {
        if (ws_clients[i].pcb == NULL)
        {
            websocket_client_t *client = &ws_clients[i];
            memset(client, 0, sizeof(*client));
            client->state = WS_STATE_HANDSHAKE;
            client->last_activity = get_absolute_time();
            return client;
        }
    }
    return NULL;
}

static void websocket_release_client(websocket_client_t *client, bool abort_pcb)
{
    if (!client)
    {
        return;
    }

    if (client->pcb)
    {
        tcp_arg(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_sent(client->pcb, NULL);
        tcp_poll(client->pcb, NULL, 0);
        tcp_err(client->pcb, NULL);

        if (abort_pcb)
        {
            tcp_abort(client->pcb);
        }
        else
        {
            err_t close_err = tcp_close(client->pcb);
            if (close_err != ERR_OK)
            {
                tcp_abort(client->pcb);
            }
        }
    }

    memset(client, 0, sizeof(*client));
}

static err_t websocket_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK || !newpcb)
    {
        return ERR_VAL;
    }

    websocket_client_t *client = websocket_alloc_client();
    if (!client)
    {
        tcp_close(newpcb);
        return ERR_MEM;
    }

    client->pcb = newpcb;

    tcp_arg(newpcb, client);
    tcp_recv(newpcb, websocket_recv_cb);
    tcp_sent(newpcb, websocket_sent_cb);
    tcp_poll(newpcb, websocket_poll_cb, WS_POLL_INTERVAL);
    tcp_err(newpcb, websocket_err_cb);

    printf("WebSocket client connected\n");
    return ERR_OK;
}

static err_t websocket_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    websocket_client_t *client = (websocket_client_t *)arg;
    if (!client)
    {
        if (p)
        {
            pbuf_free(p);
        }
        return ERR_OK;
    }

    if (!p)
    {
        websocket_release_client(client, false);
        printf("WebSocket client disconnected\n");
        return ERR_OK;
    }

    if (err != ERR_OK)
    {
        pbuf_free(p);
        return err;
    }

    if (client->rx_len + p->tot_len > sizeof(client->rx_buffer))
    {
        pbuf_free(p);
        websocket_release_client(client, true);
        printf("WebSocket receive buffer overflow\n");
        return ERR_OK;
    }

    pbuf_copy_partial(p, client->rx_buffer + client->rx_len, p->tot_len, 0);
    client->rx_len += p->tot_len;
    client->last_activity = get_absolute_time();

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    websocket_process_buffer(client);
    return ERR_OK;
}

static err_t websocket_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)tpcb;
    (void)len;

    websocket_client_t *client = (websocket_client_t *)arg;
    if (!client)
    {
        return ERR_OK;
    }

    websocket_push_pending(client);
    return ERR_OK;
}

static err_t websocket_poll_cb(void *arg, struct tcp_pcb *tpcb)
{
    (void)tpcb;
    websocket_client_t *client = (websocket_client_t *)arg;
    if (!client)
    {
        return ERR_OK;
    }

    if (absolute_time_diff_us(client->last_activity, get_absolute_time()) > (int64_t)WS_IDLE_TIMEOUT_MS * 1000)
    {
        websocket_release_client(client, false);
        printf("WebSocket client timed out\n");
        return ERR_OK;
    }

    websocket_push_pending(client);
    return ERR_OK;
}

static void websocket_err_cb(void *arg, err_t err)
{
    (void)err;
    websocket_client_t *client = (websocket_client_t *)arg;
    if (client)
    {
        websocket_release_client(client, true);
    }
}

static void websocket_process_buffer(websocket_client_t *client)
{
    if (client->state == WS_STATE_HANDSHAKE)
    {
        websocket_process_handshake(client);
        return;
    }

    if (client->state == WS_STATE_CONNECTED)
    {
        websocket_process_frames(client);
    }
}

static uint32_t ws_rotl(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32u - bits));
}

static void ws_sha1_process_block(ws_sha1_ctx *ctx, const uint8_t *block)
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }

    for (int i = 16; i < 80; ++i)
    {
        w[i] = ws_rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];

    for (int i = 0; i < 80; ++i)
    {
        uint32_t f;
        uint32_t k;

        if (i < 20)
        {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = ws_rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ws_rotl(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

static void ws_sha1_init(ws_sha1_ctx *ctx)
{
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->bitcount = 0;
    ctx->buffer_len = 0;
}

static void ws_sha1_update(ws_sha1_ctx *ctx, const uint8_t *data, size_t len)
{
    if (!data || len == 0)
    {
        return;
    }

    ctx->bitcount += (uint64_t)len * 8u;

    size_t offset = 0;
    if (ctx->buffer_len > 0)
    {
        size_t to_copy = 64u - ctx->buffer_len;
        if (to_copy > len)
        {
            to_copy = len;
        }
        memcpy(ctx->buffer + ctx->buffer_len, data, to_copy);
        ctx->buffer_len += to_copy;
        offset += to_copy;

        if (ctx->buffer_len == 64u)
        {
            ws_sha1_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    while (offset + 64u <= len)
    {
        ws_sha1_process_block(ctx, data + offset);
        offset += 64u;
    }

    if (offset < len)
    {
        ctx->buffer_len = len - offset;
        memcpy(ctx->buffer, data + offset, ctx->buffer_len);
    }
}

static void ws_sha1_final(ws_sha1_ctx *ctx, uint8_t digest[20])
{
    uint64_t bit_len = ctx->bitcount;
    uint8_t pad = 0x80u;
    uint8_t zero = 0;

    ws_sha1_update(ctx, &pad, 1);
    while (ctx->buffer_len != 56u)
    {
        ws_sha1_update(ctx, &zero, 1);
    }

    uint8_t length_block[8];
    for (int i = 0; i < 8; ++i)
    {
        length_block[7 - i] = (uint8_t)(bit_len >> (i * 8));
    }
    ws_sha1_update(ctx, length_block, sizeof(length_block));

    for (int i = 0; i < 5; ++i)
    {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void websocket_sha1(const uint8_t *data, size_t len, uint8_t digest[20])
{
    ws_sha1_ctx ctx;
    ws_sha1_init(&ctx);
    ws_sha1_update(&ctx, data, len);
    ws_sha1_final(&ctx, digest);
}

static size_t websocket_base64_encode(const uint8_t *data, size_t len, char *out, size_t out_size)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!data || !out)
    {
        return 0;
    }

    size_t required = ((len + 2u) / 3u) * 4u;
    if (out_size <= required)
    {
        return 0;
    }

    size_t out_idx = 0;
    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t chunk = (uint32_t)data[i] << 16;
        if (i + 1 < len)
        {
            chunk |= (uint32_t)data[i + 1] << 8;
        }
        if (i + 2 < len)
        {
            chunk |= (uint32_t)data[i + 2];
        }

        out[out_idx++] = table[(chunk >> 18) & 0x3Fu];
        out[out_idx++] = table[(chunk >> 12) & 0x3Fu];
        out[out_idx++] = (i + 1 < len) ? table[(chunk >> 6) & 0x3Fu] : '=';
        out[out_idx++] = (i + 2 < len) ? table[chunk & 0x3Fu] : '=';
    }

    out[out_idx] = '\0';
    return out_idx;
}

static bool websocket_process_handshake(websocket_client_t *client)
{
    int header_len = websocket_find_double_crlf(client->rx_buffer, client->rx_len);
    if (header_len < 0)
    {
        return true;
    }

    char key[64];
    if (!websocket_extract_key((const char *)client->rx_buffer, (size_t)header_len, key, sizeof(key)))
    {
        websocket_release_client(client, true);
        return false;
    }

    char accept_value[64];
    if (!websocket_compute_accept_key(key, accept_value, sizeof(accept_value)))
    {
        websocket_release_client(client, true);
        return false;
    }

    char response[256];
    int len = snprintf(response,
                       sizeof(response),
                       "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: %s\r\n"
                       "Sec-WebSocket-Version: 13\r\n"
                       "\r\n",
                       accept_value);
    if (len <= 0 || (size_t)len >= sizeof(response))
    {
        websocket_release_client(client, true);
        return false;
    }

    if (tcp_write(client->pcb, response, len, TCP_WRITE_FLAG_COPY) != ERR_OK || tcp_output(client->pcb) != ERR_OK)
    {
        websocket_release_client(client, true);
        return false;
    }

    size_t remaining = client->rx_len - (size_t)header_len;
    memmove(client->rx_buffer, client->rx_buffer + header_len, remaining);
    client->rx_len = remaining;

    client->state = WS_STATE_CONNECTED;
    printf("WebSocket handshake completed\n");
    return true;
}

static bool websocket_process_frames(websocket_client_t *client)
{
    size_t offset = 0;

    while (client->rx_len - offset >= 2)
    {
        const uint8_t *data = client->rx_buffer + offset;
        uint8_t byte0 = data[0];
        uint8_t byte1 = data[1];

        bool fin = (byte0 & 0x80u) != 0;
        uint8_t op = byte0 & 0x0Fu;
        bool masked = (byte1 & 0x80u) != 0;
        uint64_t payload_length = (byte1 & 0x7Fu);
        size_t header_len = 2;

        if (!masked)
        {
            websocket_release_client(client, true);
            return false;
        }

        if (payload_length == 126)
        {
            if (client->rx_len - offset < 4)
            {
                break;
            }
            payload_length = (uint16_t)((data[2] << 8) | data[3]);
            header_len += 2;
        }
        else if (payload_length == 127)
        {
            websocket_release_client(client, true);
            return false;
        }

        if (client->rx_len - offset < header_len + 4)
        {
            break;
        }

        const uint8_t *mask_key = data + header_len;
        header_len += 4;

        if (client->rx_len - offset < header_len + payload_length)
        {
            break;
        }

        uint8_t payload_buffer[WS_RX_BUFFER];
        size_t copy_len = payload_length;
        if (copy_len > sizeof(payload_buffer))
        {
            copy_len = sizeof(payload_buffer);
        }

        for (size_t i = 0; i < copy_len; ++i)
        {
            payload_buffer[i] = data[header_len + i] ^ mask_key[i % 4];
        }

        if (!fin)
        {
            websocket_release_client(client, true);
            return false;
        }

        if (!websocket_handle_frame(client, op, payload_buffer, copy_len))
        {
            websocket_release_client(client, true);
            return false;
        }

        offset += header_len + payload_length;
    }

    if (offset > 0)
    {
        size_t remaining = client->rx_len - offset;
        memmove(client->rx_buffer, client->rx_buffer + offset, remaining);
        client->rx_len = remaining;
    }

    return true;
}

static int websocket_find_double_crlf(const uint8_t *buffer, size_t length)
{
    if (length < 4)
    {
        return -1;
    }

    for (size_t i = 3; i < length; ++i)
    {
        if (buffer[i - 3] == '\r' && buffer[i - 2] == '\n' && buffer[i - 1] == '\r' && buffer[i] == '\n')
        {
            return (int)(i + 1);
        }
    }

    return -1;
}

static bool websocket_extract_key(const char *request, size_t request_len, char *key, size_t key_len)
{
    if (!request || !key)
    {
        return false;
    }

    const size_t copy_len = request_len < WS_RX_BUFFER ? request_len : WS_RX_BUFFER;
    char temp[WS_RX_BUFFER + 1];
    memcpy(temp, request, copy_len);
    temp[copy_len] = '\0';

    const char *line = temp;
    while (line && *line)
    {
        const char *next = strstr(line, "\r\n");
        size_t line_len = next ? (size_t)(next - line) : strlen(line);

        if (line_len == 0)
        {
            break;
        }

        if (line_len > 18 && strncasecmp(line, "Sec-WebSocket-Key:", 18) == 0)
        {
            const char *value_start = line + 18;
            while (*value_start == ' ' || *value_start == '\t')
            {
                ++value_start;
            }

            size_t value_len = line + line_len - value_start;
            if (value_len == 0 || value_len >= key_len)
            {
                return false;
            }

            memcpy(key, value_start, value_len);
            key[value_len] = '\0';
            return true;
        }

        line = next ? next + 2 : NULL;
    }

    return false;
}

static bool websocket_compute_accept_key(const char *client_key, char *accept_key, size_t accept_len)
{
    if (!client_key || !accept_key)
    {
        return false;
    }

    static const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    char combined[128];
    int combined_len = snprintf(combined, sizeof(combined), "%s%s", client_key, GUID);
    if (combined_len <= 0 || (size_t)combined_len >= sizeof(combined))
    {
        return false;
    }

    uint8_t sha1_out[20];
    websocket_sha1((const uint8_t *)combined, (size_t)combined_len, sha1_out);

    size_t out_len = websocket_base64_encode(sha1_out, sizeof(sha1_out), accept_key, accept_len);
    return out_len > 0;
}

static bool websocket_handle_frame(websocket_client_t *client, uint8_t opcode, const uint8_t *payload, size_t payload_len)
{
    (void)client;

    switch (opcode)
    {
    case 0x1:
        if (ws_callbacks.on_receive)
        {
            return ws_callbacks.on_receive(payload, payload_len, ws_callbacks.user_data);
        }
        return true;
    case 0x2:
        return true;
    case 0x8:
        return false;
    case 0x9:
    case 0xA:
        return true;
    default:
        return true;
    }
}

static size_t ws_collect_output(uint8_t *buffer, size_t max_len)
{
    if (!buffer || max_len == 0 || !ws_callbacks.on_output)
    {
        return 0;
    }

    return ws_callbacks.on_output(buffer, max_len, ws_callbacks.user_data);
}

static size_t websocket_build_frame(uint8_t opcode, const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_size)
{
    if (!out || payload_len > 125 || out_size < payload_len + 2)
    {
        return 0;
    }

    out[0] = 0x80u | (opcode & 0x0Fu);
    out[1] = (uint8_t)payload_len;
    if (payload_len > 0 && payload)
    {
        memcpy(out + 2, payload, payload_len);
    }

    return payload_len + 2;
}

static void websocket_flush_clients(void)
{
    for (size_t i = 0; i < WS_MAX_CLIENTS; ++i)
    {
        websocket_client_t *client = &ws_clients[i];
        if (client->pcb && client->state == WS_STATE_CONNECTED)
        {
            if (client->pending_len == 0)
            {
                websocket_prepare_pending_frame(client);
            }
            websocket_push_pending(client);
        }
    }
}

static void websocket_prepare_pending_frame(websocket_client_t *client)
{
    uint8_t payload[WS_FRAME_PAYLOAD];
    size_t payload_len = ws_collect_output(payload, sizeof(payload));
    if (payload_len == 0)
    {
        return;
    }

    size_t frame_len = websocket_build_frame(0x1, payload, payload_len, client->pending_buffer, sizeof(client->pending_buffer));
    if (frame_len == 0)
    {
        return;
    }

    client->pending_len = frame_len;
    client->pending_offset = 0;
}

static void websocket_push_pending(websocket_client_t *client)
{
    if (client->pending_len == 0)
    {
        return;
    }

    while (client->pending_offset < client->pending_len)
    {
        size_t remaining = client->pending_len - client->pending_offset;
        err_t err = tcp_write(client->pcb,
                              client->pending_buffer + client->pending_offset,
                              remaining,
                              TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK)
        {
            if (tcp_output(client->pcb) != ERR_OK)
            {
                break;
            }
            client->pending_offset = client->pending_len;
        }
        else if (err == ERR_MEM)
        {
            break;
        }
        else
        {
            websocket_release_client(client, true);
            printf("WebSocket send failed (%d)\n", err);
            return;
        }
    }

    if (client->pending_offset >= client->pending_len)
    {
        client->pending_len = 0;
        client->pending_offset = 0;
    }
}
