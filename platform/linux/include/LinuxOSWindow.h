#pragma once

#if defined(WINDOW_USE_WAYLAND)

struct OSWindowInternalData
{
    int placeholder;
};

#elif defined(WINDOW_USE_X11)

struct OSWindowInternalData
{
    int placeholder;
};

#endif