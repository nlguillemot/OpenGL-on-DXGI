#define COBJMACROS
#define INITGUID
#define CINTERFACE
#define D3D11_NO_HELPERS
#include <intrin.h>
#include <windows.h>
#include <d3d11.h>
#include <gl/GL.h>

#include "glcorearb.h" // https://www.opengl.org/registry/api/GL/glext.h
#include "wglext.h" // https://www.opengl.org/registry/api/GL/wglext.h

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "opengl32.lib")

#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)
#define AssertHR(hr) Assert(SUCCEEDED(hr))

static GLuint colorRbuf;
static GLuint dsRbuf;
static GLuint fbuf;

static HANDLE dxDevice;
static HANDLE dxColor;
static HANDLE dxDepthStencil;

static ID3D11Device* device;
static ID3D11DeviceContext* context;
static IDXGISwapChain* swapChain;
static ID3D11RenderTargetView* colorView;
static ID3D11DepthStencilView* dsView;

static PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
static PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
static PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
static PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
static PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
static PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;

static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;

static HWND temp;
static HDC tempdc;
static HGLRC temprc;

static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

static void Create(HWND window)
{
    // GL context on temporary window, no drawing will happen to this window
    {
        temp = CreateWindowA("STATIC", "temp", WS_OVERLAPPED,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, NULL, NULL);
        Assert(temp);

        tempdc = GetDC(temp);
        Assert(tempdc);

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_SUPPORT_OPENGL;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int format = ChoosePixelFormat(tempdc, &pfd);
        Assert(format);

        DescribePixelFormat(tempdc, format, sizeof(pfd), &pfd);
        BOOL set = SetPixelFormat(tempdc, format, &pfd);
        Assert(set);

        temprc = wglCreateContext(tempdc);
        Assert(temprc);

        BOOL make = wglMakeCurrent(tempdc, temprc);
        Assert(make);

        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

        int attrib[] =
        {
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0,
        };

        HGLRC newrc = wglCreateContextAttribsARB(tempdc, NULL, attrib);
        Assert(newrc);

        make = wglMakeCurrent(tempdc, newrc);
        Assert(make);

        wglDeleteContext(temprc);
        temprc = newrc;

        PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)wglGetProcAddress("glDebugMessageCallback");
        glDebugMessageCallback(DebugCallback, 0);

        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    *(void**)&wglDXOpenDeviceNV = (void*)wglGetProcAddress("wglDXOpenDeviceNV");
    *(void**)&wglDXCloseDeviceNV = (void*)wglGetProcAddress("wglDXCloseDeviceNV");

    *(void**)&wglDXRegisterObjectNV = (void*)wglGetProcAddress("wglDXRegisterObjectNV");
    *(void**)&wglDXUnregisterObjectNV = (void*)wglGetProcAddress("wglDXUnregisterObjectNV");

    *(void**)&wglDXLockObjectsNV = (void*)wglGetProcAddress("wglDXLockObjectsNV");
    *(void**)&wglDXUnlockObjectsNV = (void*)wglGetProcAddress("wglDXUnlockObjectsNV");

    *(void**)&glGenFramebuffers = (void*)wglGetProcAddress("glGenFramebuffers");
    *(void**)&glDeleteFramebuffers = (void*)wglGetProcAddress("glDeleteFramebuffers");

    *(void**)&glGenRenderbuffers = (void*)wglGetProcAddress("glGenRenderbuffers");
    *(void**)&glDeleteRenderbuffers = (void*)wglGetProcAddress("glDeleteRenderbuffers");

    *(void**)&glBindFramebuffer = (void*)wglGetProcAddress("glBindFramebuffer");
    *(void**)&glFramebufferRenderbuffer = (void*)wglGetProcAddress("glFramebufferRenderbuffer");

    RECT rect;
    GetClientRect(window, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Numerator = 1;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL,
        D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, 0,
        D3D11_SDK_VERSION, &desc, &swapChain, &device, NULL, &context);
    AssertHR(hr);

    dxDevice = wglDXOpenDeviceNV(device);
    Assert(dxDevice);

    glGenRenderbuffers(1, &colorRbuf);
    glGenRenderbuffers(1, &dsRbuf);
    glGenFramebuffers(1, &fbuf);
    glBindFramebuffer(GL_FRAMEBUFFER, fbuf);
}

static void Destroy()
{
    ID3D11DeviceContext_ClearState(context);

    wglDXUnregisterObjectNV(dxDevice, dxColor);
    wglDXUnregisterObjectNV(dxDevice, dxDepthStencil);

    glDeleteFramebuffers(1, &fbuf);
    glDeleteRenderbuffers(1, &colorRbuf);
    glDeleteRenderbuffers(1, &dsRbuf);

    wglDXCloseDeviceNV(dxDevice);

    wglMakeCurrent(tempdc, NULL);
    wglDeleteContext(temprc);
    ReleaseDC(temp, tempdc);

    ID3D11RenderTargetView_Release(colorView);
    ID3D11DepthStencilView_Release(dsView);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    IDXGISwapChain_Release(swapChain);
}

static void Resize(int width, int height)
{
    HRESULT hr;

    if (colorView)
    {
        wglDXUnregisterObjectNV(dxDevice, dxColor);
        wglDXUnregisterObjectNV(dxDevice, dxDepthStencil);

        ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);
        ID3D11RenderTargetView_Release(colorView);
        ID3D11DepthStencilView_Release(dsView);

        hr = IDXGISwapChain_ResizeBuffers(swapChain, 1, width, height, DXGI_FORMAT_UNKNOWN, 0);
        AssertHR(hr);
    }

    ID3D11Texture2D* colorBuffer;
    hr = IDXGISwapChain_GetBuffer(swapChain, 0, *(_GUID*)(&IID_ID3D11Texture2D), (void**)&colorBuffer);
    AssertHR(hr);

    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)colorBuffer, NULL, &colorView);
    AssertHR(hr);

    ID3D11Texture2D_Release(colorBuffer);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* dsBuffer;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &dsBuffer);
    AssertHR(hr);

    hr = ID3D11Device_CreateDepthStencilView(device, (ID3D11Resource*)dsBuffer, NULL, &dsView);
    AssertHR(hr);

    dxColor = wglDXRegisterObjectNV(dxDevice, colorBuffer, colorRbuf, GL_RENDERBUFFER, WGL_ACCESS_READ_WRITE_NV);
    Assert(dxColor);

    dxDepthStencil = wglDXRegisterObjectNV(dxDevice, dsBuffer, dsRbuf, GL_RENDERBUFFER, WGL_ACCESS_READ_WRITE_NV);
    Assert(dxDepthStencil);

    ID3D11Texture2D_Release(dsBuffer);

    D3D11_VIEWPORT view = {};
    view.TopLeftX = 0.f,
    view.TopLeftY = 0.f,
    view.Width = (float)width,
    view.Height = (float)height,
    view.MinDepth = 0.f,
    view.MaxDepth = 1.f,
    ID3D11DeviceContext_RSSetViewports(context, 1, &view);

    glViewport(0, 0, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbuf);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dsRbuf);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsRbuf);
}

static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        Create(window);
        return 0;

    case WM_DESTROY:
        Destroy();
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        Resize(LOWORD(lparam), HIWORD(lparam));
        return 0;
    }

    return DefWindowProcA(window, msg, wparam, lparam);
}

int main()
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "DXGL";

    ATOM atom = RegisterClassA(&wc);
    Assert(atom);

    HWND window = CreateWindowA(wc.lpszClassName, "DXGL",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, NULL, NULL);
    Assert(window);

    int running = 1;
    for (;;)
    {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!running)
        {
            break;
        }

        // render with D3D
        {
            FLOAT cornflowerBlue[] = { 100.f / 255.f, 149.f / 255.f, 237.f / 255.f, 1.f };
            ID3D11DeviceContext_OMSetRenderTargets(context, 1, &colorView, dsView);
            ID3D11DeviceContext_ClearRenderTargetView(context, colorView, cornflowerBlue);
            ID3D11DeviceContext_ClearDepthStencilView(context, dsView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0, 0);
        }

        HANDLE dxObjects[] = { dxColor, dxDepthStencil };
        wglDXLockObjectsNV(dxDevice, _countof(dxObjects), dxObjects);

        // render with GL
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbuf);

            glBegin(GL_TRIANGLES);
            glColor3f(1, 0, 0);
            glVertex2f(0.f, -0.5f);
            glColor3f(0, 1, 0);
            glVertex2f(0.5f, 0.5f);
            glColor3f(0, 0, 1);
            glVertex2f(-0.5f, 0.5f);
            glEnd();

            glBindFramebuffer(GL_FRAMEBUFFER, fbuf);
        }

        wglDXUnlockObjectsNV(dxDevice, _countof(dxObjects), dxObjects);

        HRESULT hr = IDXGISwapChain_Present(swapChain, 1, 0);
        Assert(SUCCEEDED(hr));
    }
}
