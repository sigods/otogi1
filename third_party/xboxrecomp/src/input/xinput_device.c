/**
 * Xbox Input Compatibility Layer
 *
 * Translates the Xbox controller API to a host gamepad backend.
 * Handles the structural differences between the Xbox gamepad
 * (analog buttons as bytes, separate trigger channels) and the host
 * (digital face buttons, trigger axes).
 *
 *   _WIN32 -> Windows XInput
 *   POSIX  -> SDL2 GameController
 */

#include "xinput_xbox.h"
#include "keyboard.h"
#include "mkb_input.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#include <time.h>
#endif

/* Merge the keyboard on top of a pad on port 0 rather than only standing in
 * for a missing one.
 *
 * The first version put the keyboard behind XInput's failure, on the
 * assumption that with nothing plugged in the call would fail. It does
 * not: under Wine XInputGetState returns ERROR_SUCCESS and a gamepad
 * with every button at rest, so the fallback was unreachable and
 * pressing a key did nothing at all.
 *
 * Merging is also the better rule. A real pad keeps working -- its
 * buttons are already in pState and the keyboard only adds to them --
 * and there is no special case left to get wrong. */
static void keyboard_merge(XBOX_INPUT_STATE *pState)
{
    XBOX_INPUT_STATE kb;
    int i;

    if (!xbox_InputKeyboardPoll(&kb))
        return;
    pState->Gamepad.wButtons |= kb.Gamepad.wButtons;
    for (i = 0; i < 8; i++)
        if (kb.Gamepad.bAnalogButtons[i] > pState->Gamepad.bAnalogButtons[i])
            pState->Gamepad.bAnalogButtons[i] = kb.Gamepad.bAnalogButtons[i];
    if (kb.Gamepad.sThumbLX) pState->Gamepad.sThumbLX = kb.Gamepad.sThumbLX;
    if (kb.Gamepad.sThumbLY) pState->Gamepad.sThumbLY = kb.Gamepad.sThumbLY;
    if (kb.Gamepad.sThumbRX) pState->Gamepad.sThumbRX = kb.Gamepad.sThumbRX;
    if (kb.Gamepad.sThumbRY) pState->Gamepad.sThumbRY = kb.Gamepad.sThumbRY;
    /* The input layer records edges, so an unchanged packet number is
     * read as the same state and the press never happens. */
    pState->dwPacketNumber = kb.dwPacketNumber;
}

/* ======================================================================== */
#if defined(_WIN32)
/* ====================  XInput backend  ================================== */
/* ======================================================================== */

#include <xinput.h>
#pragma comment(lib, "xinput.lib")

static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS] = { FALSE };
static DWORD g_last_packet[XBOX_MAX_CONTROLLERS] = { 0 };

/* The keyboard on port 0 (RECOMP_KEYBOARD=1) comes from keyboard.c, shared
 * with the SDL backend; the GDI window in fb_present.c records the keys. */

void xbox_InputInit(void)
{
    for (DWORD i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        g_controller_connected[i] = (result == ERROR_SUCCESS);
    }
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    XINPUT_STATE xi_state;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    /* Bring-up aid: hold START (and A) down so a title parked on a
     * "press start" logo/menu can be driven past it without a pad. */
    if (dwPort == 0 && getenv("RECOMP_AUTOPRESS")) {
        static DWORD synth_packet;
        memset(pState, 0, sizeof *pState);
        pState->dwPacketNumber = ++synth_packet;
        pState->Gamepad.wButtons = XBOX_GAMEPAD_START;
        pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] = 255;
        g_controller_connected[dwPort] = TRUE;
        return ERROR_SUCCESS;
    }

    result = XInputGetState(dwPort, &xi_state);
    if (result != ERROR_SUCCESS) {
        g_controller_connected[dwPort] = FALSE;
        if (dwPort == 0) {
            /* Keyboard and mouse stand in for an absent pad, so the game is
             * playable with no controller plugged in. */
            int kb;
            memset(pState, 0, sizeof *pState);
            kb = xbox_InputKeyboardPoll(pState);
            if (mkb_merge(dwPort, pState))
                g_controller_connected[dwPort] = TRUE;
            if (kb || mkb_enabled())
                return ERROR_SUCCESS;
        }
        return result;
    }

    g_controller_connected[dwPort] = TRUE;
    g_last_packet[dwPort] = xi_state.dwPacketNumber;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = xi_state.dwPacketNumber;
    pState->Gamepad.wButtons = xi_state.Gamepad.wButtons & 0x00FF;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = xi_state.Gamepad.bLeftTrigger;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = xi_state.Gamepad.bRightTrigger;

    pState->Gamepad.sThumbLX = xi_state.Gamepad.sThumbLX;
    pState->Gamepad.sThumbLY = xi_state.Gamepad.sThumbLY;
    pState->Gamepad.sThumbRX = xi_state.Gamepad.sThumbRX;
    pState->Gamepad.sThumbRY = xi_state.Gamepad.sThumbRY;

    if (dwPort == 0)
        keyboard_merge(pState);

    /* Mouse+keyboard merges on top of a real pad too: the mouse owns the
     * aim stick while it is moving, WASD owns movement while held. */
    if (dwPort == 0)
        mkb_merge(dwPort, pState);

    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    XINPUT_VIBRATION xi_vib;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    xi_vib.wLeftMotorSpeed = pVibration->wLeftMotorSpeed;
    xi_vib.wRightMotorSpeed = pVibration->wRightMotorSpeed;
    return XInputSetState(dwPort, &xi_vib);
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    const char *kb;
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    kb = getenv("RECOMP_KEYBOARD");
    if (dwPort == 0 && kb && *kb && *kb != '0') return TRUE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    XINPUT_CAPABILITIES xi_caps;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetCapabilities(dwPort, dwFlags, &xi_caps);
    if (result != ERROR_SUCCESS) return result;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type = xi_caps.Type;
    pCaps->SubType = xi_caps.SubType;
    pCaps->Flags = xi_caps.Flags;
    return ERROR_SUCCESS;
}

int xbox_InputPadRefresh(void)
{
    int connected = 0;
    DWORD i;

    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XBOX_INPUT_STATE st;
        if (xbox_InputGetState(i, &st) == ERROR_SUCCESS)
            connected |= 1 << i;
    }
    return connected;
}

BOOL xbox_InputPadSnapshot(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return FALSE;
    if (xbox_InputGetState(dwPort, pState) != ERROR_SUCCESS)
        memset(pState, 0, sizeof *pState);
    return TRUE;
}

/* ======================================================================== */
#else /* !_WIN32 */
/* ====================  SDL2 GameController backend  ===================== */
/* ======================================================================== */

#include <SDL.h>

static SDL_GameController *g_pads[XBOX_MAX_CONTROLLERS];
static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS];
static DWORD g_packet[XBOX_MAX_CONTROLLERS];

/* Set while the framebuffer window's main thread pumps SDL events
 * (src/video/fb_present_sdl.c). The pump already updates the controllers,
 * and SDL_GameControllerUpdate pumps events itself, which on macOS must only
 * happen on the main thread; so then this side only reads them. */
static volatile int g_main_pump;

void xbox_InputSetMainPump(int on)
{
    g_main_pump = on;
}

/* Guards g_pads and the refreshed snapshot below: the host refresh thread and
 * a caller on a guest thread can both poll. */
static pthread_mutex_t g_pad_lock = PTHREAD_MUTEX_INITIALIZER;

/* Last state each port reported, kept by xbox_InputPadRefresh. */
static XBOX_INPUT_STATE g_pad_state[XBOX_MAX_CONTROLLERS];

static void input_ensure_ready(void);

/* Whether any controller is open: SDL must stay up for it at shutdown. */
int xbox_InputAnyOpen(void)
{
    int any = 0;
    pthread_mutex_lock(&g_pad_lock);
    for (int i = 0; i < XBOX_MAX_CONTROLLERS; i++)
        if (g_pads[i]) {
            any = 1;
            break;
        }
    pthread_mutex_unlock(&g_pad_lock);
    return any;
}

static SDL_GameController *pad_get(int port)
{
    SDL_GameController *c;
    pthread_mutex_lock(&g_pad_lock);
    c = g_pads[port];
    pthread_mutex_unlock(&g_pad_lock);
    return c;
}

/* Open up to XBOX_MAX_CONTROLLERS attached game controllers. */
static void open_controllers(void)
{
    int slot = 0;

    pthread_mutex_lock(&g_pad_lock);
    for (int i = 0; i < SDL_NumJoysticks() && slot < XBOX_MAX_CONTROLLERS; i++) {
        if (!SDL_IsGameController(i))
            continue;
        if (!g_pads[slot]) {
            g_pads[slot] = SDL_GameControllerOpen(i);
            g_controller_connected[slot] = (g_pads[slot] != NULL);
            if (g_pads[slot])
                fprintf(stderr, "  [INPUT] controller %d: %s\n", slot,
                        SDL_GameControllerName(g_pads[slot])
                            ? SDL_GameControllerName(g_pads[slot]) : "gamepad");
        }
        slot++;
    }
    pthread_mutex_unlock(&g_pad_lock);
}

/* ── Keeping the pad state fresh without a caller ────────────────────────
 *
 * Nothing in this runtime polls the pad on its own. The guest reaches a
 * controller only through the emulated USB host controller, and that bridge
 * is opt-in and uninitialized here, so xbox_InputGetState is never called:
 * SDL is never initialized, no controller is ever opened, and a pad plugged
 * in after start is never seen.
 *
 * Refresh the state on a timer instead, so whatever bridge does come up
 * (the USB model's interrupt-IN path calls xbox_InputGetState) reads a state
 * that was updated milliseconds ago rather than one that depends on its own
 * polling. The same timer rescans for controllers, which is the only way a
 * hot-plugged pad is noticed without an SDL window pumping events.
 *
 * g_main_pump still wins: when the framebuffer window pumps SDL on the main
 * thread (where SDL_GameControllerUpdate must run on macOS), this thread only
 * reads.
 */
#define PAD_REFRESH_HZ 60
#define PAD_RESCAN_MS  1000

static volatile int g_pad_refresher;

static unsigned long long pad_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000u
         + (unsigned long long)ts.tv_nsec / 1000000u;
}

/* SDL_GameControllerUpdate pumps events, so throttle it: the refresh thread
 * and an on-demand caller would otherwise both do it every frame. */
static void update_pads(void)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static unsigned long long last;
    unsigned long long now;

    if (g_main_pump)
        return;             /* the window's main thread already did it */
    pthread_mutex_lock(&lock);
    now = pad_now_ms();
    if (!last || now - last >= 1000u / PAD_REFRESH_HZ) {
        last = now;
        SDL_GameControllerUpdate();
    }
    pthread_mutex_unlock(&lock);
}

static void rescan_controllers(void)
{
    static unsigned long long last;
    unsigned long long now = pad_now_ms();

    if (last && now - last < PAD_RESCAN_MS)
        return;
    last = now;
    open_controllers();
}

int xbox_InputPadRefresh(void)
{
    int connected = 0;
    DWORD i;

    input_ensure_ready();

    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XBOX_INPUT_STATE st;

        if (xbox_InputGetState(i, &st) != ERROR_SUCCESS)
            memset(&st, 0, sizeof st);
        else
            connected |= 1 << i;

        pthread_mutex_lock(&g_pad_lock);
        g_pad_state[i] = st;
        pthread_mutex_unlock(&g_pad_lock);
    }
    return connected;
}

BOOL xbox_InputPadSnapshot(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return FALSE;

    pthread_mutex_lock(&g_pad_lock);
    *pState = g_pad_state[dwPort];
    pthread_mutex_unlock(&g_pad_lock);
    return TRUE;
}

static void *pad_refresh_thread(void *arg)
{
    struct timespec period = { 0, 1000000000L / PAD_REFRESH_HZ };

    (void)arg;
    while (1) {
        xbox_InputPadRefresh();
        nanosleep(&period, NULL);
    }
    return NULL;
}

static void start_refresher(void)
{
    pthread_t th;

    if (g_pad_refresher)
        return;
    g_pad_refresher = 1;
    if (pthread_create(&th, NULL, pad_refresh_thread, NULL) == 0) {
        pthread_detach(th);
    } else {
        g_pad_refresher = 0;
        fprintf(stderr, "  [INPUT] pad refresh thread unavailable; "
                        "state is only refreshed on demand\n");
    }
}

/* xbox_InputInit is the documented entry point, but nothing calls it: the USB
 * bridge that would consume the state is the only candidate. Bring the layer
 * up on the first poll so it works regardless of who asks first. */
static pthread_once_t g_input_once = PTHREAD_ONCE_INIT;

static void input_bringup(void)
{
    xbox_InputInit();
}

static void input_ensure_ready(void)
{
    pthread_once(&g_input_once, input_bringup);
}

void xbox_InputInit(void)
{
    /* Keep the default SIGINT/SIGTERM: SDL would turn them into an SDL_QUIT
     * that nothing reads without a window, and Ctrl-C would stop working. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    open_controllers();
    start_refresher();
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    input_ensure_ready();
    update_pads();
    rescan_controllers();

    SDL_GameController *c = pad_get((int)dwPort);
    if (!c || !SDL_GameControllerGetAttached(c)) {
        g_controller_connected[dwPort] = FALSE;
        if (dwPort == 0 && xbox_InputKeyboardPoll(pState)) {
            g_controller_connected[0] = TRUE;
            { static long _gk = 0; if ((++_gk % 60) == 1) fprintf(stderr, "[PADK] n=%ld btn=%04X A=%d pkt=%u\n", _gk, pState->Gamepad.wButtons, pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A], pState->dwPacketNumber); }
            return ERROR_SUCCESS;
        }
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    g_controller_connected[dwPort] = TRUE;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = ++g_packet[dwPort];

    WORD btn = 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP))    btn |= XBOX_GAMEPAD_DPAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  btn |= XBOX_GAMEPAD_DPAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  btn |= XBOX_GAMEPAD_DPAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btn |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START))      btn |= XBOX_GAMEPAD_START;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK))       btn |= XBOX_GAMEPAD_BACK;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSTICK))  btn |= XBOX_GAMEPAD_LEFT_THUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) btn |= XBOX_GAMEPAD_RIGHT_THUMB;
    pState->Gamepad.wButtons = btn;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? 255 : 0;

    /* SDL trigger axes are 0..32767 -> Xbox analog button 0..255 */
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);

    /* SDL Y axis points down; the Xbox Y axis points up -- invert.
     * Use (-1 - v) so v = -32768 does not overflow SHORT. */
    pState->Gamepad.sThumbLX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    pState->Gamepad.sThumbLY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    pState->Gamepad.sThumbRX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    pState->Gamepad.sThumbRY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));

    if (dwPort == 0)
        keyboard_merge(pState);
    { static long _gs = 0; if ((++_gs % 300) == 0) fprintf(stderr, "[PADS] n=%ld btn=%04X A=%d pkt=%u\n", _gs, pState->Gamepad.wButtons, pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A], pState->dwPacketNumber); }
    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    input_ensure_ready();

    SDL_GameController *c = pad_get((int)dwPort);
    if (!c) return ERROR_DEVICE_NOT_CONNECTED;

    /* SDL rumble needs a duration; refresh for ~1s on each call (the game
     * polls vibration continuously). */
    SDL_GameControllerRumble(c, pVibration->wLeftMotorSpeed,
                             pVibration->wRightMotorSpeed, 1000);
    return ERROR_SUCCESS;
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    const char *kb;
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    kb = getenv("RECOMP_KEYBOARD");
    if (dwPort == 0 && kb && *kb && *kb != '0') return TRUE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!g_pads[dwPort]) {
        const char *kb = getenv("RECOMP_KEYBOARD");
        if (!(dwPort == 0 && kb && *kb && *kb != '0'))
            return ERROR_DEVICE_NOT_CONNECTED;
    }

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    pCaps->Flags   = 0;
    return ERROR_SUCCESS;
}

#endif /* _WIN32 */
