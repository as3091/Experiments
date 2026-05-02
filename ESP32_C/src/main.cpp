/**
 * main.c — CEC Audio Toggle Controller
 * ─────────────────────────────────────────────────────────────────────────────
 * Platform-agnostic application logic.
 *
 * All hardware access goes through the HAL:
 *   cec_hal_*   →  hal/cec_hal_linux.c   (Pi)  or  hal/cec_hal_esp32.c  (ESP32)
 *   gpio_hal_*  →  hal/gpio_hal_linux.c  (Pi)  or  hal/gpio_hal_esp32.c (ESP32)
 *
 * Build for Pi:
 *   make              →  ./cec_controller
 *   sudo ./cec_controller
 *
 * Port to ESP32:
 *   See hal/cec_hal_esp32.c for instructions.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * VERIFIED CEC SEQUENCES (LG TV + CINEMA SB590):
 *
 *   SAM ON  (audio → soundbar):
 *     TX [05 70 10 00]  TV(0)→SB(5) SAM_REQUEST phys=1.0.0.0
 *     RX [5F 72 01]     SB(5)→all   SET_SAM ON  ✓
 *
 *   SAM OFF (audio → TV speakers):
 *     TX [5F 72 00]     spoof SB(5)→all   SET_SAM OFF
 *     TX [0F 72 00]     TV(0)→all         SET_SAM OFF
 *     TX [0F 82 00 00]  TV(0)→all         ACTIVE_SOURCE(0.0.0.0)
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "cec_controller.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Pi's physical address on the HDMI bus: 1.0.0.0 */
#define PI_PHYS_HIGH  0x10
#define PI_PHYS_LOW   0x00

/* ── Application state ──────────────────────────────────────────────────── */
static app_state_t state = {
    .system_audio_on = 0,
    .volume          = 50,
};

static volatile int _running = 1;

/* ─────────────────────────────────────────────────────────────────────────────
 * CEC high-level commands
 * ───────────────────────────────────────────────────────────────────────────── */

static void cmd_audio_to_soundbar(void) {
    printf("\n[APP] ▶  Audio → SOUNDBAR (SAM ON)\n");

    /* TV(0) → SB(5)  SYSTEM_AUDIO_MODE_REQUEST  phys=1.0.0.0
     * [05 70 10 00]
     * Soundbar replies: [5F 72 01] SET_SAM ON broadcast  ✓ */
    const uint8_t frame[] = {
        (CEC_ADDR_TV << 4) | CEC_ADDR_AUDIO_SYSTEM,  /* 0x05 */
        CEC_OP_SAM_REQUEST,                           /* 0x70 */
        PI_PHYS_HIGH,                                 /* 0x10 */
        PI_PHYS_LOW,                                  /* 0x00 */
    };
    bool ok = cec_hal_tx(frame, sizeof(frame), CEC_OP_SET_SAM);

    if (ok) {
        printf("[APP] ✓  Soundbar confirmed SAM ON\n\n");
    } else {
        /* Fallback: direct broadcast SET_SAM ON */
        printf("[APP] ⚠  No reply — fallback SET_SAM ON broadcast\n");
        const uint8_t fb[] = {
            (CEC_ADDR_TV << 4) | CEC_ADDR_BROADCAST,  /* 0x0F */
            CEC_OP_SET_SAM,                            /* 0x72 */
            0x01,                                       /* ON   */
        };
        cec_hal_tx(fb, sizeof(fb), 0);
        printf("[APP] ✓  SAM ON sent (unconfirmed)\n\n");
    }
    state.system_audio_on = 1;
}

static void cmd_audio_to_tv(void) {
    printf("\n[APP] ◀  Audio → TV SPEAKERS (SAM OFF)\n");

    /* Step 1: Spoof soundbar(5) → broadcast  SET_SAM OFF  [5F 72 00] */
    const uint8_t f1[] = {
        (CEC_ADDR_AUDIO_SYSTEM << 4) | CEC_ADDR_BROADCAST,  /* 0x5F */
        CEC_OP_SET_SAM,                                       /* 0x72 */
        0x00,                                                  /* OFF  */
    };
    cec_hal_tx(f1, sizeof(f1), 0);
    usleep(150000);

    /* Step 2: TV(0) → broadcast  SET_SAM OFF  [0F 72 00] */
    const uint8_t f2[] = {
        (CEC_ADDR_TV << 4) | CEC_ADDR_BROADCAST,  /* 0x0F */
        CEC_OP_SET_SAM,                             /* 0x72 */
        0x00,                                        /* OFF  */
    };
    cec_hal_tx(f2, sizeof(f2), 0);
    usleep(150000);

    /* Step 3: TV(0) → broadcast  ACTIVE_SOURCE(0.0.0.0)  [0F 82 00 00] */
    const uint8_t f3[] = {
        (CEC_ADDR_TV << 4) | CEC_ADDR_BROADCAST,  /* 0x0F */
        CEC_OP_ACTIVE_SOURCE,                       /* 0x82 */
        0x00, 0x00,                                  /* phys = 0.0.0.0 */
    };
    cec_hal_tx(f3, sizeof(f3), 0);

    state.system_audio_on = 0;
    printf("[APP] ✓  Audio → TV speakers\n\n");
}

static void cmd_toggle_audio(void) {
    if (state.system_audio_on)
        cmd_audio_to_tv();
    else
        cmd_audio_to_soundbar();
}

static void cmd_volume_down(void) {
    printf("[APP] ↓  Volume Down\n");
    const uint8_t press[]   = { 0xF5, CEC_OP_UCP, CEC_UI_VOL_DN };
    const uint8_t release[] = { 0xF5, CEC_OP_UCR };
    cec_hal_tx(press,   sizeof(press),   0);
    usleep(120000);
    cec_hal_tx(release, sizeof(release), 0);
    state.volume = (state.volume > 0) ? state.volume - 5 : 0;
    printf("[APP]    ~%d%%\n", state.volume);
}

static void cmd_status(void) {
    printf("\n[APP] ── Status ────────────────────────────────\n");
    printf("[APP]   Audio  : %s\n",
           state.system_audio_on ? "SOUNDBAR (SAM active)" : "TV speakers");
    printf("[APP]   Volume : ~%d%%\n", state.volume);
    printf("[APP] ────────────────────────────────────────────\n\n");
}

/* ─────────────────────────────────────────────────────────────────────────────
 * GPIO callback — fired by gpio_hal_poll() on debounced button press
 * ───────────────────────────────────────────────────────────────────────────── */
static void on_button(gpio_btn_t btn) {
    switch (btn) {
        case GPIO_BTN_SOUNDBAR:
            printf("[BTN] SOUNDBAR pressed\n");
            cmd_audio_to_soundbar();
            break;
        case GPIO_BTN_TV:
            printf("[BTN] TV SPEAKERS pressed\n");
            cmd_audio_to_tv();
            break;
        case GPIO_BTN_VOL_DOWN:
            printf("[BTN] Vol Down\n");
            cmd_volume_down();
            break;
        default:
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Signal handling
 * ───────────────────────────────────────────────────────────────────────────── */
static void _sig_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("[SIG] SIGUSR1 → toggle audio\n");
        cmd_toggle_audio();
    } else if (sig == SIGUSR2) {
        printf("[SIG] SIGUSR2 → force TV speakers\n");
        cmd_audio_to_tv();
    } else {
        _running = 0;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Entry point
 * ───────────────────────────────────────────────────────────────────────────── */
#ifdef PLATFORM_ESP32
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   CEC Audio Toggle Controller  (ESP32)           ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    if (cec_hal_open(NULL) != 0) {
        printf("[APP] Failed to open CEC HAL\n");
    }

    if (gpio_hal_init(on_button) != 0) {
        printf("[APP] Failed to init GPIO\n");
    }

    cmd_status();
}

void loop() {
    gpio_hal_poll();
    delay(5);
}

#else
int main(int argc, char *argv[]) {
    const char *cec_device = (argc > 1) ? argv[1] : "/dev/cec0";

    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   CEC Audio Toggle Controller  (C binary)        ║\n");
    printf("║   GPIO17(Pin11)=SOUNDBAR  GPIO27(Pin13)=TV       ║\n");
    printf("║   GPIO22(Pin15)=VOL-                             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    signal(SIGTERM, _sig_handler);
    signal(SIGINT,  _sig_handler);
    signal(SIGUSR1, _sig_handler);
    signal(SIGUSR2, _sig_handler);

    if (cec_hal_open(cec_device) != 0) {
        fprintf(stderr, "[APP] Failed to open %s — run: sudo ./cec_controller\n", cec_device);
        return 1;
    }

    if (gpio_hal_init(on_button) != 0) {
        fprintf(stderr, "[APP] Failed to init GPIO\n");
        cec_hal_close();
        return 1;
    }

    printf("[APP] Running  PID=%d\n", getpid());
    printf("[APP]   sudo kill -USR1 %d   → toggle audio\n", getpid());
    printf("[APP]   sudo kill -USR2 %d   → force TV speakers\n\n", getpid());
    cmd_status();

    /* Main loop — 5 ms poll interval */
    while (_running) {
        gpio_hal_poll();
        usleep(5000);
    }

    printf("\n[APP] Shutting down...\n");
    gpio_hal_close();
    cec_hal_close();
    return 0;
}
#endif
