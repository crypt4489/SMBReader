#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/include/GLFW/glfw3native.h"
#undef min
#undef max


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
	int shouldBeClosed;
    GenericKeyAction actions[KC_COUNT];
};


struct WindowManager
{
public:
	void CreateWindowInstance();

	void SetWindowResizeCallback(GLFWframebuffersizefun callback);

	GLFWwindow* GetWindow() const;

	void DestroyGLFWWindow();

	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	int CreateWindowsWindow();

	int PollEvents();

	GLFWwindow* window = nullptr;

	HWND hWndMain;
    HINSTANCE hInstance;

	GenericWindowInfo windowInfo{};
};

