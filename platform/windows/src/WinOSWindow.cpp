#include <Windows.h>
#include <windowsx.h>
#include "OSWindow.h"
#include "WinOSWindow.h"
#include <atomic>

#ifdef _MSC_VER
#define ALIGNAS(x) __declspec(align(x))
#endif

struct MPMCQueueData
{
    std::atomic<size_t> currentSequence;
    int freeIndex;
};

static HINSTANCE* instancePointers;
static HWND* windowPtrs;
static MPMCQueueData* freeList;
static int maxFreeListEntry = 0;

ALIGNAS(64) static std::atomic<int> boundedLinearAllocator;
ALIGNAS(64) static std::atomic<size_t> enqueuePos{ 0 };
ALIGNAS(64) static std::atomic<size_t> dequeuePos{ 0 };

LRESULT CALLBACK winproc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp);

static int PopFromFreeList()
{
    MPMCQueueData* cell;

    size_t pos = dequeuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0)
        {
            if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return -1;
        else
            pos = dequeuePos.load(std::memory_order_relaxed);
    }

    cell->currentSequence.store(pos + maxFreeListEntry, std::memory_order_release);

    int freeListIndex = cell->freeIndex;

    cell->freeIndex = -1;

    return freeListIndex;
}

static void ReturnIndex(int index)
{
    MPMCQueueData* cell;

    size_t pos = enqueuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return;
        else
            pos = enqueuePos.load(std::memory_order_relaxed);
    }

    windowPtrs[index] = NULL;
    instancePointers[index] = NULL;
    cell->freeIndex = index;
    cell->currentSequence.store(pos + 1, std::memory_order_release);
}

static int FindFreeIndex()
{
    int ret = PopFromFreeList();

    if (ret < 0)
    {
        int linearTop = boundedLinearAllocator.load(std::memory_order_acquire);

        while (linearTop < maxFreeListEntry)
        {
            if (boundedLinearAllocator.compare_exchange_weak(linearTop, linearTop + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                ret = linearTop;
                break;
            }
        }
    }

    return ret;
}

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows)
{
    int handlesSize = (maxNumberOfWindows) * sizeof(HINSTANCE);
    int handlesWndSize = (maxNumberOfWindows) * sizeof(HWND);
    int freeListSize = (maxNumberOfWindows) * sizeof(MPMCQueueData);

    OSWindowMemoryRequirements memReqs{ handlesSize + handlesWndSize + freeListSize, alignof(HINSTANCE) };

    return memReqs;
}

void CloseAllWindows()
{
    for (int idx = 0; idx < maxFreeListEntry; idx++)
    {
        if (windowPtrs[idx] != NULL)
        {
            DestroyWindow(windowPtrs[idx]);
            windowPtrs[idx] = NULL;
            instancePointers[idx] = NULL;
        }

        freeList[idx].currentSequence.store(idx, std::memory_order_relaxed);
    }

    enqueuePos.store(0, std::memory_order_relaxed);
    dequeuePos.store(0, std::memory_order_relaxed);
    boundedLinearAllocator.store(0, std::memory_order_relaxed);
}

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows)
{
    uintptr_t dataHead = (uintptr_t)dataSource;
    uintptr_t dataStart = dataHead;

    instancePointers = (HINSTANCE*)dataSource;

    int handleSize = maxNumberOfWindows;

    dataHead += handleSize * sizeof(HINSTANCE);

    windowPtrs = (HWND*)dataHead;
    
    dataHead += sizeof(HWND) * handleSize;

    freeList = (MPMCQueueData*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
        windowPtrs[i] = NULL;
    }

    maxFreeListEntry = handleSize;

    return OS_WINDOW_SUCCESS;
}

int OSCreateWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    int windowIndex = FindFreeIndex();

    if (windowIndex < 0)
    {
        return OS_WINDOW_HANDLE_EXHAUSTED;
    }

    HINSTANCE hInst = GetModuleHandle(NULL);

    WNDCLASSEX wc = { };

    HWND hwnd;

    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = winproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TEXT(name);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        ReturnIndex(windowIndex);
        return OS_WINDOW_CREATE_FAILED;
    }

    RECT wr = { 0, 0, 800, 600 };
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;

    AdjustWindowRectEx(&wr, style, FALSE, exStyle);

    hwnd = CreateWindowEx(exStyle,
        TEXT(name),
        TEXT(name),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        NULL,
        NULL,
        hInst,
        &windowData->info);


    if (!hwnd) 
    {
        ReturnIndex(windowIndex);
        return OS_WINDOW_CREATE_FAILED;
    }

    SetWindowText(hwnd, TEXT(name));
    ShowWindow(hwnd, 1);
    UpdateWindow(hwnd);

    windowPtrs[windowIndex] = hwnd;
    instancePointers[windowIndex] = hInst;
    windowData->internalOSHandle = windowIndex;

    return OS_WINDOW_SUCCESS;
}

int OSWindowClose(OSWindow* window)
{
    int windowIndex = window->internalOSHandle;

    if (windowIndex < 0 || windowIndex >= maxFreeListEntry)
    {
        return OS_WINDOW_HANDLE_OUT_OF_BOUNDS;
    }

    int retCode = OS_WINDOW_SUCCESS;

    if (!DestroyWindow(windowPtrs[windowIndex]))
    {
        retCode = OS_WINDOW_CLOSE_FAILED;
    }

    ReturnIndex(windowIndex);

    window->internalOSHandle = -1;

    return retCode;
}

int OSWindowPollEvents(OSWindow* window)
{
    int windowIndex = window->internalOSHandle;

    if (windowIndex < 0 || windowIndex >= maxFreeListEntry)
    {
        return OS_WINDOW_HANDLE_OUT_OF_BOUNDS;
    }

    HWND hWndMain = windowPtrs[windowIndex];

    MSG msg;

    int ret = OS_WINDOW_SUCCESS;

    while (PeekMessage(&msg, hWndMain, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return ret;
}

static void UpdateWindowRECT(RECT* rect, UINT dpi)
{
    int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
    int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
    int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);


    int captionHeight = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);

    rect->left += (frameX + padding);
    rect->top += (captionHeight + padding + frameY);
    rect->bottom -= (padding + frameY);
    rect->right -= (frameX + padding);
}

LRESULT CALLBACK winproc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp)
{
    GenericWindowInfo* info = (GenericWindowInfo*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (wm)
    {
    case WM_LBUTTONDOWN:
    {
        info->clicked = 1;
        break;
    }
    case WM_LBUTTONUP:
    {
        info->clicked = 0;
        break;
    }
    case WM_MOUSEMOVE:
    {
        info->currentCursorX = GET_X_LPARAM(lp);
        info->currentCursorY = GET_Y_LPARAM(lp);
        break;
    }
    case WM_SIZE:
    {
        UINT width = LOWORD(lp);
        UINT height = HIWORD(lp);
        info->width = width;
        info->height = height;

        if (wp == SIZE_MAXIMIZED)
        {
            info->maximized = true;
            info->minimized = false;
        }
        else if (wp == SIZE_MINIMIZED)
        {
            info->maximized = false;
            info->minimized = true;
        }
        else if (wp == SIZE_RESTORED)
        {
            info->maximized = false;
            info->minimized = false;
        }
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* minMax = (MINMAXINFO*)lp;
        minMax->ptMaxSize.x = 1920;
        minMax->ptMaxSize.y = 1080;
        minMax->ptMaxPosition.x = 0;
        minMax->ptMaxPosition.y = 0;
        minMax->ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
        minMax->ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
        minMax->ptMaxTrackSize.x = GetSystemMetrics(SM_CXMAXTRACK);
        minMax->ptMaxTrackSize.y = GetSystemMetrics(SM_CYMAXTRACK);
        return 0;
    }
    case WM_NCCREATE:
    {
        CREATESTRUCT* infoStruct = (CREATESTRUCT*)lp;
        if (infoStruct->cx < 800 || infoStruct->cy < 600)
        {
            return FALSE;
        }
        return TRUE;
    }

    case WM_NCCALCSIZE:
    {
        LRESULT res = 0;
        UINT dpi = GetDpiForWindow(hwnd);

        RECT* rect = NULL;

        if (wp)
        {
            NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;

            rect = (RECT*)&params->rgrc[0];

            info->resizeRequested = 1;

            res = WVR_REDRAW;
        }
        else {
            rect = (RECT*)lp;
        }

        if (!info || !info->fullScreen) {
            UpdateWindowRECT(rect, dpi);
        }

        return 0;
    }
    case WM_DESTROY:
    {
        info->shouldBeClosed = true;
        PostQuitMessage(0);
        return 0;
    }
    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        WORD vkCode = LOWORD(wp);

        WORD keyFlags = HIWORD(lp);

        WORD scanCode = LOBYTE(keyFlags);
        BOOL isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;

        if (isExtendedKey)
            scanCode = MAKEWORD(scanCode, 0xE0);

        BOOL wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;
        WORD repeatCount = LOWORD(lp);

        BOOL isKeyReleased = (keyFlags & KF_UP) == KF_UP;

        switch (vkCode)
        {

        case '0': { info->actions[KC_ZERO].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '1': { info->actions[KC_ONE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case '2': { info->actions[KC_TWO].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '3': { info->actions[KC_THREE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '4': { info->actions[KC_FOUR].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '5': { info->actions[KC_FIVE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '6': { info->actions[KC_SIX].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '7': { info->actions[KC_SEVEN].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '8': { info->actions[KC_EIGHT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case '9': { info->actions[KC_NINE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }


        case 'A': { info->actions[KC_A].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'B': { info->actions[KC_B].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'C': { info->actions[KC_C].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'D': { info->actions[KC_D].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'E': { info->actions[KC_E].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'F': { info->actions[KC_F].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'G': { info->actions[KC_G].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'H': { info->actions[KC_H].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'I': { info->actions[KC_I].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'J': { info->actions[KC_J].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'K': { info->actions[KC_K].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'L': { info->actions[KC_L].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'M': { info->actions[KC_M].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'N': { info->actions[KC_N].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'O': { info->actions[KC_O].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'P': { info->actions[KC_P].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'Q': { info->actions[KC_Q].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'R': { info->actions[KC_R].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'S': { info->actions[KC_S].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'T': { info->actions[KC_T].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'U': { info->actions[KC_U].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'V': { info->actions[KC_V].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'W': { info->actions[KC_W].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'X': { info->actions[KC_X].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'Y': { info->actions[KC_Y].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }
        case 'Z': { info->actions[KC_Z].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }


        case VK_F1: { info->actions[KC_F1].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F2: { info->actions[KC_F2].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F3: { info->actions[KC_F3].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F4: { info->actions[KC_F4].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F5: { info->actions[KC_F5].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F6: { info->actions[KC_F6].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_F12: { info->actions[KC_F12].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }


        case VK_ESCAPE: { info->actions[KC_ESC].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);       break; }
        case VK_TAB: { info->actions[KC_TAB].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);       break; }
        case VK_SHIFT: { info->actions[KC_LSHIFT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);    break; }
        case VK_CONTROL: { info->actions[KC_LCTRL].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_MENU: { info->actions[KC_LALT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);      break; }
        case VK_SPACE: { info->actions[KC_SPACE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_RETURN: { info->actions[KC_ENTER].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_BACK: { info->actions[KC_BACKSPACE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }


        case VK_UP: { info->actions[KC_UP].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);       break; }
        case VK_DOWN: { info->actions[KC_DOWN].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_LEFT: { info->actions[KC_LEFT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_RIGHT: { info->actions[KC_RIGHT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);    break; }
        case VK_INSERT: { info->actions[KC_INSERT].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_DELETE: { info->actions[KC_DELETE].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_HOME: { info->actions[KC_HOME].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);    break; }
        case VK_END: { info->actions[KC_END].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);     break; }
        case VK_PRIOR: { info->actions[KC_PAGEUP].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED);  break; }
        case VK_NEXT: { info->actions[KC_PAGEDOWN].Update(isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED); break; }

        default: break;
        }
        break;
    }
    case WM_CREATE:
    {
        CREATESTRUCT* infoStruct = (CREATESTRUCT*)lp;
        if (infoStruct->lpCreateParams)
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)infoStruct->lpCreateParams);
        }
        break;
    }
    case WM_QUIT:
    {
        break;
    }
    case WM_NCACTIVATE:
    {
        break;
    }
    }

    return DefWindowProc(hwnd, wm, wp, lp);
}

int OSWindowGetInternalData(OSWindow* window, void* internalDataStruct)
{
    int windowIndex = window->internalOSHandle;

    if (windowIndex < 0 || windowIndex >= maxFreeListEntry)
    {
        return OS_WINDOW_HANDLE_OUT_OF_BOUNDS;
    }

    HWND hWndMain = windowPtrs[windowIndex];
    HINSTANCE hInstMain = instancePointers[windowIndex];

    OSWindowInternalData* data = (OSWindowInternalData*)internalDataStruct;

    data->inst = hInstMain;
    data->wnd = hWndMain;

    return OS_WINDOW_SUCCESS;
}

int OSWindowSetText(OSWindow* window, const char* text)
{
    int windowIndex = window->internalOSHandle;

    if (windowIndex < 0 || windowIndex >= maxFreeListEntry)
    {
        return OS_WINDOW_HANDLE_OUT_OF_BOUNDS;
    }

    HWND hWndMain = windowPtrs[windowIndex];

    SetWindowText(hWndMain, TEXT(text));

    return OS_WINDOW_SUCCESS;
}