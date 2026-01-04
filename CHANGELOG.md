# Changelog - Build 631 (2026-01-04)

## Memory Management Improvements

### Disk Controller Static Patch Pool
- **Changed**: Replaced dynamic `malloc()` with static pre-allocated patch pool
- **Size**: 1200 sectors (~166KB) for copy-on-write disk modifications
- **Impact**: Prevents heap exhaustion that caused system crashes during heavy disk operations
- **Files**: `Altair8800/pico_88dcdd_flash.h`, `Altair8800/pico_88dcdd_flash.c`

**Details:**
- Pool is shared across all disks (A, B, C, D)
- Pool size configurable via `PATCH_POOL_SIZE` in `pico_88dcdd_flash.h`
- Added exhaustion detection with warning message
- Added stats function `pico_disk_get_patch_stats()` for monitoring

### lwIP Network Stack Optimizations
- **Changed**: Reduced memory allocation for network buffers
- **Files**: `lwipopts.h`

| Setting | Before | After |
|---------|--------|-------|
| `MEM_SIZE` | 16000 | 12000 |
| `PBUF_POOL_SIZE` | 24 | 16 |
| `MEMP_NUM_TCP_SEG` | 32 | 24 |
| `TCP_WND` | 6×MSS | 4×MSS |
| `TCP_SND_BUF` | 6×MSS | 4×MSS |
| `LWIP_ICMP` | 1 | 0 |
| `LWIP_RAW` | 1 | 0 |

---

## Build Configuration Updates

### Removed Pico Display 2.8 Support
- **Removed**: `pico_display28` build target from build scripts
- **Reason**: Display 2.8 requires ~440KB RAM, Pico/Pico W only have 264KB
- **Files**: `build_all_boards.sh`, `.vscode/tasks.json`

**Display 2.8 now requires RP2350-based boards:**
- pico2, pico2_w
- pimoroni_pico_plus2_w_rp2350

---

## Memory Usage (pico2_w with Display 2.8)

| Component | Size |
|-----------|------|
| Framebuffer (RGB565) | 150 KB |
| Disk patch pool | 166 KB |
| Altair memory | 64 KB |
| Other (stack, network, etc.) | ~61 KB |
| **Total RAM used** | 440.7 KB |
| **Free heap** | 79.3 KB |
