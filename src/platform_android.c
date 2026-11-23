// Cyber Drift — Android platform layer (pure native, no Java, no NDK glue).
//
// The APK is a NativeActivity app with android:hasCode="false": Android loads
// libcyberdrift.so and calls ANativeActivity_onCreate(). We install our own
// activity callbacks, own the EGL surface, and pump the ALooper for input.
#include "cd_game.h"
#include "cd_gl.h"
#include "cd_android.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// A few extra NDK entry points we need but keep out of the shim header.
extern int AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent);
extern int AInputQueue_preDispatchEvent(AInputQueue* queue, AInputEvent* event);
extern void AInputQueue_finishEvent(AInputQueue* queue, AInputEvent* event, int handled);
extern void AInputQueue_attachLooper(AInputQueue* queue, ALooper* looper, int ident,
                                     void (*callback)(int fd, int events, void* data), void* data);
extern void AInputQueue_detachLooper(AInputQueue* queue);
extern void ANativeActivity_setWindowFlags(ANativeActivity* activity, uint32_t addFlags, uint32_t removeFlags);
#define AWINDOW_FLAG_FULLSCREEN 0x00000400
#define AWINDOW_FLAG_KEEP_SCREEN_ON 0x00000080

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

typedef struct {
    cd_game game;
    ANativeActivity* activity;
    AInputQueue* input_queue;
    ALooper* looper;
    ANativeWindow* window;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    int width, height;
    int has_surface;
    int resumed;
    int focused;
    int destroy_requested;
    int renderer_ready;
    double last_time;
    int joy_pointer;
    float joy_origin_x, joy_origin_y;
    int nitro_pointer;
    int brake_pointer;
    int key_left, key_right, key_nitro, key_brake;
} platform_state;

static platform_state g_ps;

// ---------------------------------------------------------------------- EGL
static int egl_start(platform_state* ps) {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) { LOGI("EGL: no display"); return 0; }
    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) { LOGI("EGL: initialize failed"); return 0; }
    LOGI("EGL %d.%d", major, minor);
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 0, EGL_NONE};
    EGLConfig config = 0;
    EGLint num = 0;
    if (!eglChooseConfig(dpy, attribs, &config, 1, &num) || num < 1) { LOGI("EGL: no config"); return 0; }
    ps->display = dpy;
    ps->config = config;
    EGLint format = 0;
    eglGetConfigAttrib(dpy, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(ps->window, 0, 0, format);
    EGLSurface surf = eglCreateWindowSurface(dpy, config, (EGLNativeWindowType)ps->window, NULL);
    if (surf == EGL_NO_SURFACE) { LOGI("EGL: window surface failed %d", (int)eglGetError()); return 0; }
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx == EGL_NO_CONTEXT) { LOGI("EGL: context failed"); return 0; }
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { LOGI("EGL: make current failed"); return 0; }
    ps->surface = surf;
    ps->context = ctx;
    ps->has_surface = 1;
    ps->width = ANativeWindow_getWidth(ps->window);
    ps->height = ANativeWindow_getHeight(ps->window);
    LOGI("EGL: surface %dx%d", ps->width, ps->height);
    return 1;
}

static void egl_stop(platform_state* ps) {
    if (ps->display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(ps->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (ps->context != EGL_NO_CONTEXT) eglDestroyContext(ps->display, ps->context);
    if (ps->surface != EGL_NO_SURFACE) eglDestroySurface(ps->display, ps->surface);
    ps->context = EGL_NO_CONTEXT;
    ps->surface = EGL_NO_SURFACE;
    ps->has_surface = 0;
}

// -------------------------------------------------------------------- input
static void start_game(platform_state* ps) {
    cd_game_reset(&ps->game);
    ps->game.state = CD_STATE_PLAY;
    ps->last_time = now_seconds();
}

static void touch_down(platform_state* ps, int id, float x, float y) {
    cd_game* g = &ps->game;
    if (g->state == CD_STATE_MENU) {
        if (x >= g->start_btn_x && x <= g->start_btn_x + g->start_btn_w &&
            y >= g->start_btn_y && y <= g->start_btn_y + g->start_btn_h) {
            start_game(ps);
        }
        return;
    }
    if (g->state == CD_STATE_GAMEOVER) {
        if (x >= g->restart_btn_x && x <= g->restart_btn_x + g->restart_btn_w &&
            y >= g->restart_btn_y && y <= g->restart_btn_y + g->restart_btn_h) {
            start_game(ps);
        }
        return;
    }
    if (g->state == CD_STATE_PAUSE) {
        g->state = CD_STATE_PLAY;
        ps->last_time = now_seconds();
        return;
    }
    float dx = x - g->pause_btn_x, dy = y - g->pause_btn_y;
    if (dx * dx + dy * dy <= g->pause_btn_r * g->pause_btn_r) {
        g->state = CD_STATE_PAUSE;
        return;
    }
    dx = x - g->nitro_cx; dy = y - g->nitro_cy;
    if (dx * dx + dy * dy <= (g->nitro_r * 1.3f) * (g->nitro_r * 1.3f)) {
        ps->nitro_pointer = id;
        return;
    }
    dx = x - g->brake_cx; dy = y - g->brake_cy;
    if (dx * dx + dy * dy <= (g->brake_r * 1.5f) * (g->brake_r * 1.5f)) {
        ps->brake_pointer = id;
        return;
    }
    if (x < (float)ps->width * 0.5f) {
        ps->joy_pointer = id;
        ps->joy_origin_x = x;
        ps->joy_origin_y = y;
        return;
    }
    ps->nitro_pointer = id;   // right-half tap also boosts
}

static void touch_move(platform_state* ps, int id, float x, float y) {
    cd_game* g = &ps->game;
    if (id != ps->joy_pointer) return;
    float dx = x - ps->joy_origin_x;
    float dy = y - ps->joy_origin_y;
    float max_r = g->joy_r > 1.f ? g->joy_r * 0.82f : 120.f;
    float mag = sqrtf(dx * dx + dy * dy);
    if (mag < 1e-4f) return;
    float norm = clampf(mag / max_r, 0.f, 1.f);
    float sx = dx / mag * norm;
    float sy = dy / mag * norm;
    g->in_steer = clampf(sx * 1.7f, -1.f, 1.f);
    g->joy_dx = sx;
    g->joy_dy = sy;
    g->in_throttle = clampf(-sy * 1.25f, 0.f, 1.f);
    // Let the stick travel: re-centre the origin when the finger goes far.
    if (mag > max_r) {
        ps->joy_origin_x += dx / mag * (mag - max_r);
        ps->joy_origin_y += dy / mag * (mag - max_r);
    }
}

static void touch_up(platform_state* ps, int id) {
    cd_game* g = &ps->game;
    if (id == ps->joy_pointer) {
        ps->joy_pointer = -1;
        g->in_steer = 0.f;
        g->joy_dx = 0.f;
        g->joy_dy = 0.f;
        if (ps->nitro_pointer < 0) g->in_throttle = 0.f;
    }
    if (id == ps->nitro_pointer) {
        ps->nitro_pointer = -1;
        if (ps->joy_pointer < 0) g->in_throttle = 0.f;
    }
    if (id == ps->brake_pointer) {
        ps->brake_pointer = -1;
        g->in_brake = 0;
    }
}

static int handle_input_event(platform_state* ps, AInputEvent* ev) {
    cd_game* g = &ps->game;
    int32_t type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(ev);
        int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
        size_t count = AMotionEvent_getPointerCount(ev);
        if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            size_t index = (masked == AMOTION_EVENT_ACTION_POINTER_DOWN)
                               ? (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8)
                               : 0;
            if (index < count) {
                touch_down(ps, AMotionEvent_getPointerId(ev, index), AMotionEvent_getX(ev, index),
                           AMotionEvent_getY(ev, index));
            }
        } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
            for (size_t i = 0; i < count; i++) {
                touch_move(ps, AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i),
                           AMotionEvent_getY(ev, i));
            }
        } else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP) {
            size_t index = (masked == AMOTION_EVENT_ACTION_POINTER_UP)
                               ? (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8)
                               : 0;
            if (index < count) touch_up(ps, AMotionEvent_getPointerId(ev, index));
        } else if (masked == AMOTION_EVENT_ACTION_CANCEL) {
            touch_up(ps, ps->joy_pointer);
            touch_up(ps, ps->nitro_pointer);
            touch_up(ps, ps->brake_pointer);
        }
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(ev);
        int32_t code = AKeyEvent_getKeyCode(ev);
        int down = action == AKEY_EVENT_ACTION_DOWN;
        switch (code) {
            case AKEYCODE_DPAD_LEFT: ps->key_left = down; break;
            case AKEYCODE_DPAD_RIGHT: ps->key_right = down; break;
            case AKEYCODE_BUTTON_A: case AKEYCODE_SPACE: case AKEYCODE_DPAD_UP:
                ps->key_nitro = down;
                break;
            case AKEYCODE_DPAD_DOWN: case AKEYCODE_BUTTON_B: ps->key_brake = down; break;
            case AKEYCODE_BACK: case AKEYCODE_P:
                if (down) {
                    if (g->state == CD_STATE_PLAY) g->state = CD_STATE_PAUSE;
                    else if (g->state == CD_STATE_PAUSE) g->state = CD_STATE_PLAY;
                    else if (g->state == CD_STATE_MENU) start_game(ps);
                    else if (g->state == CD_STATE_GAMEOVER) start_game(ps);
                }
                break;
            case AKEYCODE_ENTER:
                if (down && (g->state == CD_STATE_MENU || g->state == CD_STATE_GAMEOVER)) start_game(ps);
                break;
            default: break;
        }
        if (ps->key_left || ps->key_right) {
            float bias = (ps->key_right ? 1.f : 0.f) - (ps->key_left ? 1.f : 0.f);
            if (ps->joy_pointer < 0) g->in_steer = bias;
        } else if (ps->joy_pointer < 0) {
            g->in_steer = 0.f;
        }
        if (ps->key_nitro) g->in_throttle = 1.f;
        else if (ps->nitro_pointer < 0 && ps->joy_pointer < 0) g->in_throttle = 0.f;
        g->in_brake = ps->key_brake ? 1 : 0;
        return 1;
    }
    return 0;
}

// --------------------------------------------------------------- lifecycle
static void on_input_queue_created_cb(int fd, int events, void* data) {
    (void)fd; (void)events; (void)data;
    platform_state* ps = &g_ps;
    AInputEvent* ev = NULL;
    while (AInputQueue_getEvent(ps->input_queue, &ev) >= 0) {
        if (AInputQueue_preDispatchEvent(ps->input_queue, ev)) continue;
        int handled = handle_input_event(ps, ev);
        AInputQueue_finishEvent(ps->input_queue, ev, handled);
    }
}

static void on_input_queue_created(ANativeActivity* activity, AInputQueue* queue) {
    (void)activity;
    platform_state* ps = &g_ps;
    ps->input_queue = queue;
    AInputQueue_attachLooper(queue, ps->looper, 1, on_input_queue_created_cb, NULL);
    LOGI("input queue attached");
}

static void on_input_queue_destroyed(ANativeActivity* activity, AInputQueue* queue) {
    (void)activity;
    platform_state* ps = &g_ps;
    AInputQueue_detachLooper(queue);
    ps->input_queue = NULL;
}

static void on_window_created(ANativeActivity* activity, ANativeWindow* window) {
    (void)activity;
    platform_state* ps = &g_ps;
    ps->window = window;
    LOGI("window created %p", (void*)window);
    if (!egl_start(ps)) {
        LOGI("EGL setup failed");
        return;
    }
    if (!ps->renderer_ready) {
        // cd_game_init() clears the game struct, so it must run *before*
        // cd_render_init() fills in the renderer (shaders, atlas, buffers).
        cd_game_init(&ps->game, ps->width, ps->height);
        if (!cd_render_init(&ps->game.renderer, ps->width, ps->height)) {
            LOGI("renderer init failed");
            return;
        }
        ps->renderer_ready = 1;
        LOGI("engine booted %dx%d", ps->width, ps->height);
    } else {
        cd_game_resize(&ps->game, ps->width, ps->height);
    }
    ps->last_time = now_seconds();
}

static void on_window_resized(ANativeActivity* activity, ANativeWindow* window) {
    (void)activity;
    platform_state* ps = &g_ps;
    ps->window = window;
    if (!ps->renderer_ready) return;
    int w = ANativeWindow_getWidth(window);
    int h = ANativeWindow_getHeight(window);
    if (w > 0 && h > 0 && (w != ps->width || h != ps->height)) {
        ps->width = w;
        ps->height = h;
        cd_game_resize(&ps->game, w, h);
        LOGI("resized %dx%d", w, h);
    }
}

static void on_window_destroyed(ANativeActivity* activity, ANativeWindow* window) {
    (void)activity; (void)window;
    platform_state* ps = &g_ps;
    egl_stop(ps);
    ps->window = NULL;
    LOGI("window destroyed");
}

static void on_pause(ANativeActivity* activity) {
    (void)activity;
    platform_state* ps = &g_ps;
    ps->resumed = 0;
    if (ps->game.state == CD_STATE_PLAY) ps->game.state = CD_STATE_PAUSE;
    LOGI("paused");
}

static void on_resume(ANativeActivity* activity) {
    (void)activity;
    platform_state* ps = &g_ps;
    ps->resumed = 1;
    ps->last_time = now_seconds();
    LOGI("resumed");
}

static void on_focus_changed(ANativeActivity* activity, int has_focus) {
    (void)activity;
    g_ps.focused = has_focus;
    if (!has_focus && g_ps.game.state == CD_STATE_PLAY) g_ps.game.state = CD_STATE_PAUSE;
}

static void on_destroy(ANativeActivity* activity) {
    (void)activity;
    g_ps.destroy_requested = 1;
    LOGI("destroy requested");
}

// ------------------------------------------------------------------- frame
static void render_frame(platform_state* ps) {
    cd_game* g = &ps->game;
    double t = now_seconds();
    float dt = (float)(t - ps->last_time);
    if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
    ps->last_time = t;
    g->fps = g->fps > 0.f ? lerpf(g->fps, 1.f / dt, 0.08f) : 1.f / dt;

    if (ps->nitro_pointer >= 0 || ps->key_nitro) g->in_throttle = 1.f;
    if (ps->brake_pointer >= 0) g->in_brake = 1;

    cd_game_update(g, dt);
    cd_game_render(g);
    if (g->state == CD_STATE_MENU) {
        extern void cd_ui_menu(cd_game* g);
        cd_render_begin_ui(&g->renderer, ps->width, ps->height);
        cd_ui_menu(g);
        cd_render_end_ui(&g->renderer);
    }
    if (!eglSwapBuffers(ps->display, ps->surface)) {
        LOGI("swap failed %d", (int)eglGetError());
    }
}

// Entry point called by Android when the activity starts.
__attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState,
                                                                     size_t savedStateSize) {
    (void)savedState; (void)savedStateSize;
    memset(&g_ps, 0, sizeof(g_ps));
    g_ps.activity = activity;
    g_ps.joy_pointer = -1;
    g_ps.nitro_pointer = -1;
    g_ps.brake_pointer = -1;
    g_ps.resumed = 1;

    activity->callbacks->onNativeWindowCreated = on_window_created;
    activity->callbacks->onNativeWindowResized = on_window_resized;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity->callbacks->onInputQueueCreated = on_input_queue_created;
    activity->callbacks->onInputQueueDestroyed = on_input_queue_destroyed;
    activity->callbacks->onPause = on_pause;
    activity->callbacks->onResume = on_resume;
    activity->callbacks->onWindowFocusChanged = on_focus_changed;
    activity->callbacks->onDestroy = on_destroy;

    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

    g_ps.looper = ALooper_prepare(0);
    LOGI("ANativeActivity_onCreate: Cyber Drift starting (sdk %d)", activity->sdkVersion);

    // Main loop (NativeActivity runs this on its own UI thread).
    while (!g_ps.destroy_requested) {
        int fd = -1;
        int events = 0;
        void* data = NULL;
        // Block when nothing can be drawn, poll otherwise.
        int timeout = (g_ps.has_surface && g_ps.resumed) ? 0 : 60;
        ALooper_pollOnce(timeout, &fd, &events, &data);
        if (g_ps.has_surface && g_ps.resumed && g_ps.renderer_ready) {
            render_frame(&g_ps);
        } else {
            sleep_ms(8);
        }
    }
    egl_stop(&g_ps);
    LOGI("main loop exited");
}
