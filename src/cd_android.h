#ifndef CD_ANDROID_H
#define CD_ANDROID_H


#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- logging
#ifndef ANDROID_LOG_INFO
#define ANDROID_LOG_INFO 4
#endif
int __android_log_print(int prio, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CyberDrift", __VA_ARGS__)

// ------------------------------------------------------------------ looper
struct ALooper;
struct AInputQueue;
struct AConfiguration;
typedef struct AConfiguration AConfiguration;
struct ARect;
struct JavaVM_;
typedef struct JavaVM_ JavaVM;
typedef struct ALooper ALooper;
typedef struct AInputQueue AInputQueue;
struct AInputEvent;
typedef struct AInputEvent AInputEvent;

ALooper* ALooper_prepare(int opts);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
void ALooper_wake(ALooper* looper);

// ---------------------------------------------------------------- ANativeWindow
struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;
typedef struct ANativeWindow_Buffer {
    int32_t width, height, stride, format;
    void* bits;
    uint32_t reserved[6];
} ANativeWindow_Buffer;

int32_t ANativeWindow_getWidth(ANativeWindow* window);
int32_t ANativeWindow_getHeight(ANativeWindow* window);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow* window, int32_t width, int32_t height, int32_t format);

// ------------------------------------------------------------------- input
struct AInputEvent;
enum {
    AINPUT_EVENT_TYPE_KEY = 1,
    AINPUT_EVENT_TYPE_MOTION = 2,
};
enum {
    AINPUT_SOURCE_CLASS_POINTER = 0x00000004,
};
enum {
    AKEY_EVENT_ACTION_DOWN = 0,
    AKEY_EVENT_ACTION_UP = 1,
};
enum {
    AMOTION_EVENT_ACTION_MASK = 0xff,
    AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00,
    AMOTION_EVENT_ACTION_DOWN = 0,
    AMOTION_EVENT_ACTION_UP = 1,
    AMOTION_EVENT_ACTION_MOVE = 2,
    AMOTION_EVENT_ACTION_CANCEL = 3,
    AMOTION_EVENT_ACTION_POINTER_DOWN = 5,
    AMOTION_EVENT_ACTION_POINTER_UP = 6,
};

int32_t AInputEvent_getType(const AInputEvent* event);
int32_t AInputEvent_getSource(const AInputEvent* event);
int32_t AKeyEvent_getAction(const AInputEvent* event);
int32_t AKeyEvent_getKeyCode(const AInputEvent* event);
int32_t AMotionEvent_getAction(const AInputEvent* event);
size_t AMotionEvent_getPointerCount(const AInputEvent* event);
int32_t AMotionEvent_getPointerId(const AInputEvent* event, size_t pointerIndex);
float AMotionEvent_getX(const AInputEvent* event, size_t pointerIndex);
float AMotionEvent_getY(const AInputEvent* event, size_t pointerIndex);
int64_t AMotionEvent_getEventTime(const AInputEvent* event);

#define AKEYCODE_BACK 4
#define AKEYCODE_MENU 82
#define AKEYCODE_P 44
#define AKEYCODE_ESCAPE 111
#define AKEYCODE_ENTER 66
#define AKEYCODE_SPACE 62
#define AKEYCODE_DPAD_LEFT 21
#define AKEYCODE_DPAD_RIGHT 22
#define AKEYCODE_DPAD_UP 19
#define AKEYCODE_DPAD_DOWN 20
#define AKEYCODE_BUTTON_A 96
#define AKEYCODE_BUTTON_B 97

// ---------------------------------------------------------- native_activity
typedef struct ANativeActivity ANativeActivity;
typedef void ANativeActivity_createFunc(ANativeActivity* activity, void* savedState, size_t savedStateSize);

struct ANativeActivity {
    struct ANativeActivityCallbacks {
        void (*onStart)(ANativeActivity* activity);
        void (*onResume)(ANativeActivity* activity);
        void (*onSaveInstanceState)(ANativeActivity* activity, void** outState, size_t* outSavedStateSize);
        void (*onPause)(ANativeActivity* activity);
        void (*onStop)(ANativeActivity* activity);
        void (*onDestroy)(ANativeActivity* activity);
        void (*onWindowFocusChanged)(ANativeActivity* activity, int hasFocus);
        void (*onNativeWindowCreated)(ANativeActivity* activity, ANativeWindow* window);
        void (*onNativeWindowResized)(ANativeActivity* activity, ANativeWindow* window);
        void (*onNativeWindowRedrawNeeded)(ANativeActivity* activity, ANativeWindow* window);
        void (*onNativeWindowDestroyed)(ANativeActivity* activity, ANativeWindow* window);
        void (*onInputQueueCreated)(ANativeActivity* activity, AInputQueue* queue);
        void (*onInputQueueDestroyed)(ANativeActivity* activity, AInputQueue* queue);
        void (*onContentRectChanged)(ANativeActivity* activity, const void* rect);
        void (*onConfigurationChanged)(ANativeActivity* activity);
        void (*onLowMemory)(ANativeActivity* activity);
    }* callbacks;
    void* clazz;
    void* env;
    JavaVM* vm;
    void* _internal_1;
    void* _internal_2;
    ANativeWindow* window;
    void* assets;
    const char* internalDataPath;
    const char* externalDataPath;
    int32_t sdkVersion;
    void* instance;
    void* assetManager;
    const char* obbPath;
};

// --------------------------------------------------------- native app glue
typedef struct android_app android_app;
typedef void (*android_app_cmd_handler)(android_app* app, int32_t cmd);

#define APP_CMD_INPUT_CHANGED 1
#define APP_CMD_INIT_WINDOW 2
#define APP_CMD_TERM_WINDOW 3
#define APP_CMD_WINDOW_RESIZED 5
#define APP_CMD_WINDOW_REDRAW_NEEDED 6
#define APP_CMD_GAINED_FOCUS 8
#define APP_CMD_LOST_FOCUS 9
#define APP_CMD_CONFIG_CHANGED 10
#define APP_CMD_LOW_MEMORY 11
#define APP_CMD_START 12
#define APP_CMD_RESUME 13
#define APP_CMD_SAVE_STATE 14
#define APP_CMD_PAUSE 15
#define APP_CMD_STOP 16
#define APP_CMD_DESTROY 17

struct android_app {
    void* userData;
    void (*onAppCmd)(struct android_app* app, int32_t cmd);
    int32_t (*onInputEvent)(struct android_app* app, AInputEvent* event);
    ANativeActivity* activity;
    AConfiguration* config;
    void* savedState;
    size_t savedStateSize;
    ALooper* looper;
    AInputQueue* inputQueue;
    ANativeWindow* window;
    void* contentRect;
    int32_t activityState;
    int32_t destroyRequested;
    void* mutex;
    void* cond;
    void* msgread;
    void* msgwrite;
    void* thread;
    void* cmdPollSource;
    void* inputPollSource;
    int32_t running;
    void* stateSaved;
    int32_t destroyed;
    int32_t redrawNeeded;
    AInputQueue* pendingInputQueue;
    ANativeWindow* pendingWindow;
    int32_t pendingContentRect;
};

void app_dummy(void);

// ------------------------------------------------------------- jni minimal
typedef struct JNIEnv JNIEnv;
struct _jobject;
typedef struct _jobject* jobject;
typedef struct _jobject* jclass;
typedef struct _jobject* jstring;
typedef struct _jobject* jarray;
typedef int32_t jint;
typedef int64_t jlong;
typedef uint8_t jboolean;
typedef float jfloat;
typedef double jdouble;
typedef void jvoid;
typedef int8_t jbyte;
typedef uint16_t jchar;


#endif
