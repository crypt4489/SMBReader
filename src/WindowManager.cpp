#include "WindowManager.h"
#include <stdexcept>
#include <windows.h>
#include <WinUser.h>
#include <tchar.h>
const TCHAR CLSNAME[] = TEXT("AppViewerClass");
LRESULT CALLBACK winproc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp);



void WindowManager::CreateWindowInstance()
{
	bool good = glfwInit();
	if (!good) throw std::runtime_error("Cannot create window");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(800, 600, "SMB File Viewer", nullptr, nullptr);

}

void WindowManager::SetWindowResizeCallback(GLFWframebuffersizefun callback)
{
	glfwSetFramebufferSizeCallback(window, callback);
}

GLFWwindow* WindowManager::GetWindow() const
{
	return window;
}

void WindowManager::DestroyGLFWWindow()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

bool WindowManager::ShouldCloseWindow()
{
	return windowInfo.shouldBeClosed;
}

void WindowManager::GetWindowSize(int* width, int* height)
{
	glfwGetFramebufferSize(window, width, height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, width, height);
		glfwWaitEvents();
	}
}

int WindowManager::CreateWindowsWindow()
{

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
    wc.lpszClassName = CLSNAME;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, TEXT("Could not register window class"),
            NULL, MB_ICONERROR);
        return 0;
    }

    hwnd = CreateWindowEx(WS_EX_LEFT,
        CLSNAME,
        NULL,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        hInst,
        &windowInfo);


    if (!hwnd) {
        MessageBox(NULL, TEXT("Could not create window"), NULL, MB_ICONERROR);
        return 0;
    }


    ShowWindow(hwnd, 1);
    UpdateWindow(hwnd);

    hWndMain = hwnd;
    hInstance = hInst;

    return 1;

}

int WindowManager::PollEvents()
{
    MSG msg;

    int ret = 0;

    while (PeekMessage(&msg, hWndMain, 0, 0, PM_REMOVE))
    {
         TranslateMessage(&msg);
         DispatchMessage(&msg);   
    }

    return ret;
}

LRESULT CALLBACK winproc(HWND hwnd, UINT wm, WPARAM wp, LPARAM lp)
{
    switch (wm)
    {
    case WM_SIZE:
    {
        UINT width = LOWORD(lp);
        UINT height = HIWORD(lp);
        if (wp == SIZE_MAXIMIZED)
        {

        }
        else if (wp == SIZE_MINIMIZED)
        {

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
        if (infoStruct->cx < 800 || infoStruct->cy  < 600)
        {
            return FALSE;
        }
        return TRUE;
    }
    
    case WM_NCCALCSIZE:
    {
        LRESULT res = 0;
        if (wp)
        {

            NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;

            RECT* rect = (RECT*)&params->rgrc[0];

            UINT dpi = GetDpiForWindow(hwnd);


            int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
            int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
            int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);


            int captionHeight = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);

            rect->left += (frameX + padding);
            rect->top += (captionHeight + padding + frameY);
            rect->bottom -= (padding + frameY);
            rect->right -= (frameX + padding);

            res = WVR_ALIGNLEFT | WVR_REDRAW;
        }
        else {
            RECT* rect = (RECT*)lp;

            UINT dpi = GetDpiForWindow(hwnd);

           
            int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
            int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
            int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

           
            int captionHeight = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);

            rect->left += (frameX + padding);
            rect->top += (captionHeight + padding + frameY);
            rect->bottom -= (padding + frameY);
            rect->right -= (frameX + padding);
        }
        return res;
    }
    case WM_DESTROY:
    {
        GenericWindowInfo* info = (GenericWindowInfo*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        info->shouldBeClosed = true;
        PostQuitMessage(0);
        return 0;
    }
    case WM_KEYUP:
    case WM_KEYDOWN:

    {
        GenericWindowInfo* info = (GenericWindowInfo*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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

        return 0;
    }


    case WM_CREATE:
    {
        CREATESTRUCT* infoStruct = (CREATESTRUCT*)lp;
        if (infoStruct->lpCreateParams)
        {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)infoStruct->lpCreateParams);
        }
        return 0;
    }
    
    case WM_QUIT:
    {
       
        return 0;
    }
    case WM_NCACTIVATE:
      

        return 0;
    }

    return DefWindowProc(hwnd, wm, wp, lp);
}