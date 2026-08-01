#include <Windows.h>
#include <windowsx.h>
#include "OSWindow.h"
#include "WinOSWindow.h"

static HINSTANCE* instancePointers;
static HWND* windowPtrs;
static MPMCQueueData* freeList;
static int maxFreeListEntry = 0;

ALIGNAS(64) static std::atomic<int> boundedLinearAllocator{ 0 };
ALIGNAS(64) static std::atomic<size_t> enqueuePos{ 0 };
ALIGNAS(64) static std::atomic<size_t> dequeuePos{ 0 };

static LRESULT CALLBACK winproc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp);

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

static GenericWindowEventPacked* GetWindowEventPacked(GenericWindowEventBuffer* buffer)
{
    size_t currentWrite = buffer->dataWrite.load(std::memory_order_relaxed);
    size_t currentRead = buffer->dataRead.load(std::memory_order_relaxed);

    void* currentWritePtr = (void*)((uintptr_t)buffer->dataHead + (currentWrite & (buffer->dataSize - 1)));

    if ((currentWrite-currentRead) >= buffer->dataSize)
    {
        buffer->ringLapsSinceLastRead.fetch_add(1, std::memory_order_relaxed);
    }

    return (GenericWindowEventPacked*)(currentWritePtr);
}

static void CommitWindowEventPacked(GenericWindowEventBuffer* buffer)
{
    buffer->dataWrite.fetch_add(sizeof(GenericWindowEventPacked), std::memory_order_release);
}

static int PumpWindowEventsPacked(GenericWindowEventBuffer* buffer, GenericWindowInfo* info)
{
    int packedEventsCount = 0;

    size_t writeHead = buffer->dataWrite.load(std::memory_order_acquire);
    size_t readPos = buffer->dataRead.load(std::memory_order_relaxed);

    while (readPos < writeHead)
    {
        GenericWindowEventPacked* currentPacked = (GenericWindowEventPacked*)((uintptr_t)buffer->dataHead + (readPos & (buffer->dataSize - 1)));

        switch (currentPacked->EventType)
        {
        case WINDOW_EVENT_TYPE_MOUSE_LEFT_BUTTON:
            info->clicked = currentPacked->EventPacked;
            break;
        case WINDOW_EVENT_TYPE_RESIZE_REQUESTED:
            info->resizeRequested = currentPacked->EventPacked;
            break;
        case WINDOW_EVENT_TYPE_SHOULD_BE_CLOSED:
            info->shouldBeClosed = currentPacked->EventPacked;
            break;
        case WINDOW_EVENT_TYPE_WINDOW_SIZE:
            info->width = GET_WINDOW_SIZE_EVENT_WIDTH(currentPacked->EventPacked);
            info->height = GET_WINDOW_SIZE_EVENT_HEIGHT(currentPacked->EventPacked);
            info->maximized = GET_WINDOW_SIZE_EVENT_MAXIMIZED(currentPacked->EventPacked);
            info->minimized = GET_WINDOW_SIZE_EVENT_MINIMIZED(currentPacked->EventPacked);
            break;
        case WINDOW_EVENT_TYPE_MOUSE_COORDINATES:
            info->currentCursorX = GET_WINDOW_COORDINATES_EVENT_X(currentPacked->EventPacked);
            info->currentCursorY = GET_WINDOW_COORDINATES_EVENT_Y(currentPacked->EventPacked);
            break;
        case WINDOW_EVENT_TYPE_KEY_ACTION:
            info->actions[GET_KEY_CODE(currentPacked->EventPacked)].Update(GET_KEY_ACTION(currentPacked->EventPacked));
            break;
        }

        readPos += sizeof(GenericWindowEventPacked);
        packedEventsCount++;
    }

    buffer->dataRead.store(readPos, std::memory_order_release);

    if (buffer->ringLapsSinceLastRead.load(std::memory_order_acquire))
        packedEventsCount = -packedEventsCount;

    buffer->ringLapsSinceLastRead.store(0, std::memory_order_relaxed);

    return packedEventsCount;
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

int OSWindowSeedEventBuffer(OSWindow* window, void* bufferMemory, size_t bufferSize)
{
    if (bufferSize & (bufferSize - 1))
    {
        return -1;
    }

    window->eventBuffer.dataHead = bufferMemory;
    window->eventBuffer.dataSize = bufferSize;
    window->eventBuffer.dataRead = 0;
    window->eventBuffer.dataWrite = 0;
    window->eventBuffer.ringLapsSinceLastRead = 0;
    window->eventBuffer.requestedFullScreen = 0;

    return 0;
}

int OSCreateWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    int windowIndex = FindFreeIndex();

    if (windowIndex < 0)
    {
        return OS_WINDOW_HANDLE_EXHAUSTED;
    }

    HINSTANCE hInst = GetModuleHandle(NULL); 

    HWND hwnd;

    static bool registerOnce = false;
    
    if (!registerOnce)
    {
        WNDCLASSEX wc = { };

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

        registerOnce = true;
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
        &windowData->eventBuffer);


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

int OSWindowPollEvents(OSWindow* window, GenericWindowInfo* info)
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

    int count = PumpWindowEventsPacked(&window->eventBuffer, info);

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
    GenericWindowEventBuffer* info = (GenericWindowEventBuffer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (wm)
    {
    case WM_LBUTTONDOWN:
    {
        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_MOUSE_LEFT_BUTTON;
        packed->EventPacked = 1;
        CommitWindowEventPacked(info);
        break;
    }
    case WM_LBUTTONUP:
    {
        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_MOUSE_LEFT_BUTTON;
        packed->EventPacked = 0;
        CommitWindowEventPacked(info);
        break;
    }
    case WM_MOUSEMOVE:
    {
        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_MOUSE_COORDINATES;
        packed->EventPacked = PACK_WINDOW_COORDINATES_EVENT(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        CommitWindowEventPacked(info);
        break;
    }
    case WM_SIZE:
    {
        WORD width = LOWORD(lp);
        WORD height = HIWORD(lp);
        
        int maximized = 0;
        int minimized = 0;

        if (wp == SIZE_MAXIMIZED)
        {
            maximized = 1;
            minimized = 0;
        }
        else if (wp == SIZE_MINIMIZED)
        {
            maximized = 0;
            minimized = 1;
        }
        else if (wp == SIZE_RESTORED)
        {
            maximized = 0;
            minimized = 0;
        }

        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_WINDOW_SIZE;
        packed->EventPacked = PACK_WINDOW_SIZE_EVENT(width, height, minimized, maximized);
        CommitWindowEventPacked(info);
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

        if (infoStruct->lpCreateParams)
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)infoStruct->lpCreateParams);
        }

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

        if (info && wp)
        {
            NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;

            rect = (RECT*)&params->rgrc[0];

            GenericWindowEventPacked* packed = GetWindowEventPacked(info);
            packed->EventType = WINDOW_EVENT_TYPE_RESIZE_REQUESTED;
            packed->EventPacked = 1;
            CommitWindowEventPacked(info);
            res = WVR_REDRAW;
        }
        else {
            rect = (RECT*)lp;
        }

        if (!info || !info->requestedFullScreen) {
            UpdateWindowRECT(rect, dpi);
        }

        return res;
    }
    case WM_DESTROY:
    {
        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_SHOULD_BE_CLOSED;
        packed->EventPacked = 1;
        CommitWindowEventPacked(info);
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

        int keyCode = 0;
        int keyAction = isKeyReleased ? RELEASED : wasKeyDown ? HELD : PRESSED;

        switch (vkCode)
        {
            case '0': { keyCode = KC_ZERO; break; }
            case '1': { keyCode = KC_ONE; break; }
            case '2': { keyCode = KC_TWO; break; }
            case '3': { keyCode = KC_THREE; break; }
            case '4': { keyCode = KC_FOUR; break; }
            case '5': { keyCode = KC_FIVE; break; }
            case '6': { keyCode = KC_SIX; break; }
            case '7': { keyCode = KC_SEVEN; break; }
            case '8': { keyCode = KC_EIGHT; break; }
            case '9': { keyCode = KC_NINE; break; }

            case 'A': { keyCode = KC_A; break; }
            case 'B': { keyCode = KC_B; break; }
            case 'C': { keyCode = KC_C; break; }
            case 'D': { keyCode = KC_D; break; }
            case 'E': { keyCode = KC_E; break; }
            case 'F': { keyCode = KC_F; break; }
            case 'G': { keyCode = KC_G; break; }
            case 'H': { keyCode = KC_H; break; }
            case 'I': { keyCode = KC_I; break; }
            case 'J': { keyCode = KC_J; break; }
            case 'K': { keyCode = KC_K; break; }
            case 'L': { keyCode = KC_L; break; }
            case 'M': { keyCode = KC_M; break; }
            case 'N': { keyCode = KC_N; break; }
            case 'O': { keyCode = KC_O; break; }
            case 'P': { keyCode = KC_P; break; }
            case 'Q': { keyCode = KC_Q; break; }
            case 'R': { keyCode = KC_R; break; }
            case 'S': { keyCode = KC_S; break; }
            case 'T': { keyCode = KC_T; break; }
            case 'U': { keyCode = KC_U; break; }
            case 'V': { keyCode = KC_V; break; }
            case 'W': { keyCode = KC_W; break; }
            case 'X': { keyCode = KC_X; break; }
            case 'Y': { keyCode = KC_Y; break; }
            case 'Z': { keyCode = KC_Z; break; }

            case VK_F1: { keyCode = KC_F1; break; }
            case VK_F2: { keyCode = KC_F2; break; }
            case VK_F3: { keyCode = KC_F3; break; }
            case VK_F4: { keyCode = KC_F4; break; }
            case VK_F5: { keyCode = KC_F5; break; }
            case VK_F6: { keyCode = KC_F6; break; }
            case VK_F12: { keyCode = KC_F12; break; }

            case VK_ESCAPE: { keyCode = KC_ESC; break; }
            case VK_TAB: { keyCode = KC_TAB; break; }
            case VK_SHIFT: { keyCode = KC_LSHIFT; break; }
            case VK_CONTROL: { keyCode = KC_LCTRL; break; }
            case VK_MENU: { keyCode = KC_LALT; break; }
            case VK_SPACE: { keyCode = KC_SPACE; break; }
            case VK_RETURN: { keyCode = KC_ENTER; break; }
            case VK_BACK: { keyCode = KC_BACKSPACE; break; }

            case VK_UP: { keyCode = KC_UP; break; }
            case VK_DOWN: { keyCode = KC_DOWN; break; }
            case VK_LEFT: { keyCode = KC_LEFT; break; }
            case VK_RIGHT: { keyCode = KC_RIGHT; break; }
            case VK_INSERT: { keyCode = KC_INSERT; break; }
            case VK_DELETE: { keyCode = KC_DELETE; break; }
            case VK_HOME: { keyCode = KC_HOME; break; }
            case VK_END: { keyCode = KC_END; break; }
            case VK_PRIOR: { keyCode = KC_PAGEUP; break; }
            case VK_NEXT: { keyCode = KC_PAGEDOWN; break; }
            default: break;
        }

        GenericWindowEventPacked* packed = GetWindowEventPacked(info);
        packed->EventType = WINDOW_EVENT_TYPE_KEY_ACTION;
        packed->EventPacked = PACK_KEY_CODE_ACTION(keyCode, keyAction);
        CommitWindowEventPacked(info);
        break;
    }
    case WM_CREATE:
    {
        
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