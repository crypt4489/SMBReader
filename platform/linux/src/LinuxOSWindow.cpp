
#include "OSWindow.h"
#include "LinuxOSWindow.h"
#include <atomic>

#include <sys/mman.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;

#if defined(WINDOW_USE_WAYLAND)

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-protocol.c>
#include <xdg-decoration-client-protocol.h>
#include <xdg-decoration-client-protocol.c>

struct pool_data {
    int fd;
    unsigned capacity;
    unsigned size;
};

static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct wl_surface *surface = NULL;
static struct wl_shm_pool *pool = NULL;
static struct wl_buffer *buffer;
static struct pool_data data;
static int memfd = -1;
static void *shm_data; 
static struct xdg_wm_base* xdg_wm_base;
static struct zxdg_decoration_manager_v1* decoration_manager;

static char framebuffer[1 * 1024 * 1024];

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
    return -1;
}

static void RegistryGlobalHandler
(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
) {

    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = (wl_compositor*)wl_registry_bind(registry, name,
            &wl_compositor_interface, 4);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = (wl_shm*)wl_registry_bind(registry, name,
            &wl_shm_interface, 1);
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = (struct xdg_wm_base*)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        decoration_manager = (zxdg_decoration_manager_v1*)wl_registry_bind(
            registry, name, &zxdg_decoration_manager_v1_interface, 1);
    }


   // printf("interface: '%s', version: %u, name: %u\n", interface, version, name);
}

void RegistryGlobalRemoveHandler
(
    void *data,
    struct wl_registry *registry,
    uint32_t name
) 
{
    printf("removed: %u\n", name);
}

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows)
{
    OSWindowMemoryRequirements memReqs{ 0, alignof(void*) };

    return memReqs;
}

void CloseAllWindows()
{
    zxdg_decoration_manager_v1_destroy(decoration_manager);

    wl_buffer_destroy(buffer);

    wl_surface_destroy(surface);

    wl_shm_pool_destroy(pool);

    munmap(shm_data, 1024 * 1024);

    close(memfd);

    xdg_wm_base_destroy(xdg_wm_base);
    wl_shm_destroy(shm);
    wl_compositor_destroy(compositor);
    wl_display_disconnect(display);
}


void hello_create_memory_pool()
{
    memfd = memfd_create("whatever", 0);

    ftruncate(memfd , 1024*1024);

    data.capacity = 1024 * 1024;
    data.size = 0;
    data.fd = memfd;

    shm_data = mmap(NULL, 1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);

    pool = wl_shm_create_pool(shm, data.fd, data.capacity);
}

static void XdgWmBasePing(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = XdgWmBasePing,
};

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows)
{
    if (!display)
    {
        display = wl_display_connect(NULL);

        struct wl_registry *registry = wl_display_get_registry(display);

        struct wl_registry_listener registry_listener = {
            .global = RegistryGlobalHandler,
            .global_remove = RegistryGlobalRemoveHandler
        };

        wl_registry_add_listener(registry, &registry_listener, NULL);

        wl_display_roundtrip(display);

        if (!compositor || !shm || !xdg_wm_base || !decoration_manager) {
            printf("Missing required globals: decomanager\n",
            (void*)decoration_manager);
             fflush(stdout);
        }

        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);

        wl_registry_destroy(registry);

        hello_create_memory_pool();
    }

    return 0;
}

static const uint32_t PIXEL_FORMAT_ID = WL_SHM_FORMAT_ARGB8888;

struct wl_buffer *hello_create_buffer(struct wl_shm_pool *pool,
    unsigned width, unsigned height)
{
    buffer = wl_shm_pool_create_buffer(pool,
        data.size, width, height,
        width*sizeof(uint32_t), PIXEL_FORMAT_ID);

    if (buffer == NULL)
    {
        data.size += width*height*sizeof(uint32_t);
    }

    return buffer;
}

static bool surface_configured = false;

static void XdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
    surface_configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = XdgSurfaceConfigure,
};

static void XdgToplevelConfigure(void *data, struct xdg_toplevel *toplevel,
                                  int32_t width, int32_t height, struct wl_array *states) {}
static void XdgToplevelClose(void *data, struct xdg_toplevel *toplevel) { /* set running=false */ }

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = XdgToplevelConfigure,
    .close = XdgToplevelClose,
};

int CreateOSWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    surface = wl_compositor_create_surface(compositor);

    if (surface == NULL)
    {
        return -1;
    }

    struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

    xdg_toplevel_set_title(xdg_toplevel, "My Window");

    struct zxdg_toplevel_decoration_v1 *decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(decoration_manager, xdg_toplevel);
    
    zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    wl_surface_set_user_data(surface, NULL);

    wl_surface_commit(surface); 
    wl_display_flush(display);
    
    while (!surface_configured) 
    {
        wl_display_dispatch(display);    
    }

    buffer = hello_create_buffer(pool, requestedDimensionX, requestDimensionY);

    memset(shm_data, 0xFF, requestDimensionY * requestedDimensionX * sizeof(uint32_t));
    
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, requestedDimensionX, requestDimensionY);
    wl_surface_commit(surface);
    wl_display_flush(display);

    return 0;
}

int PollOSWindowEvents(OSWindow* window)
{
    int ret = 0;

    wl_display_dispatch_pending(display);
    wl_display_flush(display);

    return ret;
}

int GetInternalOSData(OSWindow* window, void* internalDataStruct)
{
    return 0;
}

int SetOSWindowText(OSWindow* window, const char* text)
{
    return 0;
}

#elif defined(WINDOW_USE_X11)

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
    return -1;
}

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows)
{
    OSWindowMemoryRequirements memReqs{ 0, alignof(void*) };

    return memReqs;
}

void CloseAllWindows()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
}

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows)
{
    return 0;
}

int CreateOSWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    return 0;
}

int PollOSWindowEvents(OSWindow* window)
{
    int ret = 0;

    return ret;
}

int GetInternalOSData(OSWindow* window, void* internalDataStruct)
{
    return 0;
}

int SetOSWindowText(OSWindow* window, const char* text)
{
    return 0;
}

#endif