#include "OS.h"

enum KeyCodes
{
    KC_ZERO = 0,
    KC_ONE = 1,
    KC_TWO = 2,
    KC_THREE = 3,
    KC_FOUR = 4,
    KC_FIVE = 5,
    KC_SIX = 6,
    KC_SEVEN = 7,
    KC_EIGHT = 8,
    KC_NINE = 9,

    KC_A, KC_B, KC_C, KC_D, KC_E, KC_F, KC_G, KC_H, KC_I, KC_J,
    KC_K, KC_L, KC_M, KC_N, KC_O, KC_P, KC_Q, KC_R, KC_S, KC_T,
    KC_U, KC_V, KC_W, KC_X, KC_Y, KC_Z,


    KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6,
    KC_F12,


    KC_ESC,
    KC_TAB,
    KC_LSHIFT,
    KC_RSHIFT,
    KC_LCTRL,
    KC_RCTRL,
    KC_LALT,
    KC_RALT,
    KC_SPACE,
    KC_ENTER,
    KC_BACKSPACE,


    KC_UP,
    KC_DOWN,
    KC_LEFT,
    KC_RIGHT,
    KC_INSERT,
    KC_DELETE,
    KC_HOME,
    KC_END,
    KC_PAGEUP,
    KC_PAGEDOWN,

    KC_COUNT // Useful for array sizing (e.g., bool keys[KC_COUNT])
};

enum ActionStates
{
    RELEASED = 0,
    PRESSED = 1,
    HELD = 2
};

#define PREVSTATE_OFFSET 4
#define CURRENTSTATE_OFFSET 0

struct GenericKeyAction
{
    char state;

    void Update(int newState)
    {
        state <<= PREVSTATE_OFFSET;
        state &= 0xf0;
        state |= (newState & 0xf);
    }

    int GetCurrentState()
    {
        return (state & 0xf);
    }

    int GetPreviousState()
    {
        return ((state >> PREVSTATE_OFFSET) & 0xf);
    }
};

#define PACK_WINDOW_COORDINATES_EVENT(x, y) (((x&0xffff) << 16) | (y&0xffff))
#define GET_WINDOW_COORDINATES_EVENT_X(packed) ((packed >> 16) &0xffff)
#define GET_WINDOW_COORDINATES_EVENT_Y(packed) (packed&0xffff)

#define PACK_WINDOW_SIZE_EVENT(width, height, minimized, maximized) (((((uint64_t)maximized)&1) << 33) | ((((uint64_t)minimized)&1) << 32) | ((((uint64_t)width)&0xffff) << 16) | (((uint64_t)height)&0xffff))
#define GET_WINDOW_SIZE_EVENT_MAXIMIZED(packed) ((packed >> 33) &1)
#define GET_WINDOW_SIZE_EVENT_MINIMIZED(packed) ((packed >> 32) &1)
#define GET_WINDOW_SIZE_EVENT_WIDTH(packed) ((packed >> 16) &0xffff)
#define GET_WINDOW_SIZE_EVENT_HEIGHT(packed) (packed&0xffff)

#define PACK_KEY_CODE_ACTION(keyCode, keyAction) (((keyCode & KC_COUNT-1) << 3) | (keyAction & 3))
#define GET_KEY_CODE(packed) ((packed >> 3) & KC_COUNT-1)
#define GET_KEY_ACTION(packed) (packed & 3)

enum GenericWindowEventType
{
    WINDOW_EVENT_TYPE_MOUSE_LEFT_BUTTON = 1,
    WINDOW_EVENT_TYPE_MOUSE_COORDINATES = 2,
    WINDOW_EVENT_TYPE_WINDOW_SIZE = 3,
    WINDOW_EVENT_TYPE_RESIZE_REQUESTED = 4,
    WINDOW_EVENT_TYPE_SHOULD_BE_CLOSED = 5,
    WINDOW_EVENT_TYPE_KEY_ACTION = 6

};

struct GenericWindowEventPacked
{
    uint64_t EventType;
    uint64_t EventPacked;
};

struct GenericWindowEventBuffer
{
    void* dataHead;
    size_t dataSize;
    ALIGNAS(64) std::atomic<size_t> dataWrite;
    ALIGNAS(64) std::atomic<size_t> dataRead;
    ALIGNAS(64) std::atomic<size_t> ringLapsSinceLastRead;
    int requestedFullScreen;
};

struct GenericWindowInfo
{
    int shouldBeClosed = 0;
    int minimized = 0;
    int maximized = 0;
    int fullScreen = 0;
    int resizeRequested = 0;
    int width = 0;
    int height = 0;
    unsigned int currentCursorX;
    unsigned int currentCursorY;
    int clicked;
    GenericKeyAction actions[KC_COUNT];

    int HandleResizeRequested()
    {
        int ret = resizeRequested;
        resizeRequested = 0;
        return ret;
    }
};

struct OSWindow
{
	int internalOSHandle;
    GenericWindowEventBuffer eventBuffer;
};

enum OSWindowErrorCode
{
    OS_WINDOW_SUCCESS = 0,
    OS_WINDOW_HANDLE_EXHAUSTED = -1,
    OS_WINDOW_HANDLE_OUT_OF_BOUNDS = -2,
    OS_WINDOW_CREATE_FAILED = -3,
    OS_WINDOW_CLOSE_FAILED = -4
};

struct OSWindowMemoryRequirements
{
    int dataSize;
    int alignment;
};

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows);

void CloseAllWindows();

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows);

int OSCreateWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData);

int OSWindowPollEvents(OSWindow* window, GenericWindowInfo* info);

int OSWindowGetInternalData(OSWindow* window, void* internalDataStruct);

int OSWindowSetText(OSWindow* window, const char* text);

int OSWindowClose(OSWindow* window);

int OSWindowSeedEventBuffer(OSWindow* window, void* bufferMemory, size_t bufferSize);