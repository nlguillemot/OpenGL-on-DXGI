#include <dxgi1_5.h>
#include <d3d11.h>

#include "glcorearb.h"
#include "wglext.h"

#include <cassert>
#include <cstdio>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "opengl32.lib")

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

void APIENTRY DebugCallbackGL(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    OutputDebugStringA("DebugCallbackGL: ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        ExitProcess(0);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    bool ok;
    HRESULT hr;

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = TEXT("WindowClass");
    ok = RegisterClassExW(&wc) != NULL;
    assert(ok);

    // Determine size of window based on window style
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD dwExStyle = 0;
    RECT wr = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    ok = AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle) != FALSE;
    assert(ok);

    TCHAR* title = TEXT("OpenGL on DXGI");

    // Create window
    HWND hWnd = CreateWindowEx(
        dwExStyle, TEXT("WindowClass"), title, dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);
    assert(hWnd);

    // Unhide the window
    ShowWindow(hWnd, SW_SHOWDEFAULT);

    // Create window that will be used to create a GL context
    HWND gl_hWnd = CreateWindowEx(0, TEXT("WindowClass"), 0, 0, 0, 0, 0, 0, 0, 0, hInstance, 0);
    assert(gl_hWnd);

    HDC gl_hDC = GetDC(gl_hWnd);
    assert(gl_hDC);

    // set pixelformat for window that supports OpenGL
    PIXELFORMATDESCRIPTOR gl_pfd = {};
    gl_pfd.nSize = sizeof(gl_pfd);
    gl_pfd.nVersion = 1;
    gl_pfd.dwFlags = PFD_SUPPORT_OPENGL;

    int chosenPixelFormat = ChoosePixelFormat(gl_hDC, &gl_pfd);
    ok = SetPixelFormat(gl_hDC, chosenPixelFormat, &gl_pfd) != FALSE;
    assert(ok);

    // Create dummy GL context that will be used to create the real context
    HGLRC dummy_hGLRC = wglCreateContext(gl_hDC);
    assert(dummy_hGLRC);

    // Use the dummy context to get function to create a better context
    ok = wglMakeCurrent(gl_hDC, dummy_hGLRC) != FALSE;
    assert(ok);

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    int contextFlagsGL = 0;
#ifdef _DEBUG
    contextFlagsGL |= WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

    int contextAttribsGL[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, contextFlagsGL,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    HGLRC hGLRC = wglCreateContextAttribsARB(gl_hDC, NULL, contextAttribsGL);
    assert(hGLRC);

    // Switch to the new context and ditch the old one
    ok = wglMakeCurrent(gl_hDC, hGLRC) != FALSE;
    assert(ok);
    ok = wglDeleteContext(dummy_hGLRC) != FALSE;
    assert(ok);

    // Grab WGL functions
    PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
    PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
    PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
    PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
    PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");

    // Fall back to GetProcAddress to get GL 1 functions. wglGetProcAddress returns NULL on those.
    HMODULE hOpenGL32 = LoadLibrary(TEXT("OpenGL32.dll"));

    // Grab OpenGL functions
    PFNGLENABLEPROC glEnable = (PFNGLENABLEPROC)GetProcAddress(hOpenGL32, "glEnable");
    PFNGLDISABLEPROC glDisable = (PFNGLDISABLEPROC)GetProcAddress(hOpenGL32, "glDisable");
    PFNGLCLEARPROC glClear = (PFNGLCLEARPROC)GetProcAddress(hOpenGL32, "glClear");
    PFNGLCLEARCOLORPROC glClearColor = (PFNGLCLEARCOLORPROC)GetProcAddress(hOpenGL32, "glClearColor");
    PFNGLSCISSORPROC glScissor = (PFNGLSCISSORPROC)GetProcAddress(hOpenGL32, "glScissor");
    PFNGLGENTEXTURESPROC glGenTextures = (PFNGLGENTEXTURESPROC)GetProcAddress(hOpenGL32, "glGenTextures");
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)wglGetProcAddress("glCheckFramebufferStatus");

    // Enable OpenGL debugging
#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)wglGetProcAddress("glDebugMessageCallback");
    glDebugMessageCallback(DebugCallbackGL, 0);
#endif

    // create D3D11 device, context and swap chain.
    ID3D11Device *device;
    ID3D11DeviceContext *devCtx;
    IDXGISwapChain *swapChain;
    HANDLE hFrameLatencyWaitableObject;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferCount = 3;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    UINT flags = 0;
#if _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDeviceAndSwapChain(NULL,                        // pAdapter
        D3D_DRIVER_TYPE_HARDWARE,    // DriverType
        NULL,                        // Software
        flags,                       // Flags (Do not set D3D11_CREATE_DEVICE_SINGLETHREADED)
        NULL,                        // pFeatureLevels
        0,                           // FeatureLevels
        D3D11_SDK_VERSION,           // SDKVersion
        &scd,                        // pSwapChainDesc
        &swapChain,                  // ppSwapChain
        &device,                     // ppDevice
        NULL,                        // pFeatureLevel
        &devCtx);                    // ppImmediateContext
    assert(SUCCEEDED(hr));

    // get frame latency waitable object
    IDXGISwapChain2* swapChain2;
    hr = swapChain->QueryInterface(&swapChain2);
    hFrameLatencyWaitableObject = swapChain2->GetFrameLatencyWaitableObject();

    // Create depth stencil texture
    ID3D11Texture2D *dxDepthBuffer;
    hr = device->CreateTexture2D(
        &CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R24G8_TYPELESS, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 1, D3D11_BIND_DEPTH_STENCIL),
        NULL,
        &dxDepthBuffer);
    assert(SUCCEEDED(hr));

    // Create depth stencil view
    ID3D11DepthStencilView *depthBufferView;
    hr = device->CreateDepthStencilView(
        dxDepthBuffer,
        &CD3D11_DEPTH_STENCIL_VIEW_DESC(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D24_UNORM_S8_UINT, 0, 0, 1),
        &depthBufferView);
    assert(SUCCEEDED(hr));

    // Register D3D11 device with GL
    HANDLE gl_handleD3D;
    gl_handleD3D = wglDXOpenDeviceNV(device);
    assert(gl_handleD3D); // check GetLastError() if this fails

    // register the Direct3D depth/stencil buffer as texture2d in opengl
    GLuint dsvNameGL;
    glGenTextures(1, &dsvNameGL);

    HANDLE dsvHandleGL = wglDXRegisterObjectNV(gl_handleD3D, dxDepthBuffer, dsvNameGL, GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
    assert(dsvHandleGL); // check GetLastError() if this fails

    // Initialize GL FBO
    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    // attach the Direct3D depth buffer to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dsvNameGL, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, dsvNameGL, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // GL RTV will be recreated every frame to use the FLIP swap chain
    GLuint rtvNameGL;
    glGenTextures(1, &rtvNameGL);

    // main loop
    while (true)
    {
        // Handle all events
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Wait until the previous frame is presented before drawing the next frame
        ok = WaitForSingleObject(hFrameLatencyWaitableObject, INFINITE) == WAIT_OBJECT_0;
        assert(ok);

        // Fetch the current swapchain backbuffer from the FLIP swap chain
        ID3D11Texture2D *dxColorBuffer;
        hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&dxColorBuffer);
        assert(SUCCEEDED(hr));

        // Create RTV for swapchain backbuffer
        ID3D11RenderTargetView *colorBufferView;
        hr = device->CreateRenderTargetView(
            dxColorBuffer,
            &CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM),
            &colorBufferView);
        assert(SUCCEEDED(hr));

        // register current backbuffer
        HANDLE rtvHandleGL = wglDXRegisterObjectNV(gl_handleD3D, dxColorBuffer, rtvNameGL, GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
        assert(rtvHandleGL); // check GetLastError() if this fails

        // Attach Direct3D color buffer to FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rtvNameGL, 0);
        GLenum fbostatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        assert(fbostatus == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Attach back buffer and depth texture to redertarget for the device.
        devCtx->OMSetRenderTargets(1, &colorBufferView, depthBufferView);

        // Direct3d renders to the render targets
        float dxClearColor[] = { 0.5f, 0.0f, 0.0f, 1.0f };
        devCtx->ClearRenderTargetView(colorBufferView, dxClearColor);

        // lock the dsv/rtv for GL access
        wglDXLockObjectsNV(gl_handleD3D, 1, &dsvHandleGL);
        wglDXLockObjectsNV(gl_handleD3D, 1, &rtvHandleGL);

        // OpenGL renders to the render targets
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        // clear half the screen, so half the screen will be from DX and half from GL
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT);
        glClearColor(0.0f, 0.5f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // unlock the dsv/rtv
        wglDXUnlockObjectsNV(gl_handleD3D, 1, &dsvHandleGL);
        wglDXUnlockObjectsNV(gl_handleD3D, 1, &rtvHandleGL);

        // DXGI presents the results on the screen
        hr = swapChain->Present(0, 0);
        assert(SUCCEEDED(hr));

        // release current backbuffer back to the swap chain
        wglDXUnregisterObjectNV(gl_handleD3D, rtvHandleGL);
        colorBufferView->Release();
        dxColorBuffer->Release();
    }
}