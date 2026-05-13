
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

struct GenericKeyAction
{
    int state;
    int prevState;
    void Update(int newState)
    {
        prevState = state;
        state = newState;
    }
};

struct GenericWindowInfo
{
    int shouldBeClosed = 0;
    int minimized = 0;
    int maximized = 0;
    int fullScreen = 0;
    int resizeRequested = 0;
    int width = 0, height = 0;
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
    GenericWindowInfo info;
};

enum OSWindowErrorCode
{
    OPEN_WINDOW_FAILED = -1,
};

struct OSWindowMemoryRequirements
{
    int dataSize;
    int alignment;
};

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows);

void CloseAllWindows();

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows);

int CreateOSWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData);

int PollOSWindowEvents(OSWindow* window);

int GetInternalOSData(OSWindow* window, void* internalDataStruct);

int SetOSWindowText(OSWindow* window, const char* text);