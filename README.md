# OpenGL-on-DXGI
How to use WGL_NV_DX_interop2 to use OpenGL in a DXGI window

## NVIDIA

Tested on GTX 970.

On NVIDIA, this example works fine with a `DXGI_SWAP_EFFECT_DISCARD` swap chain. It seems NVIDIA drivers are currently not updated for Windows 10 swap chains (like `DXGI_SWAP_EFFECT_FLIP_DISCARD`).

Currently, `FLIP_DISCARD` seems to only work for the swap chain's first buffer (strangely), resulting in flickering. The screen is black for all frames that aren't using the first buffer of the swap chain.

Also, there are some exceptions being thrown when the swap chain buffer is registered as an OpenGL resource, which you can see logged in the debug output window. It's spammy, but seems you can ignore them safely.

Also, I'm getting errors in the framebuffer's status. Not sure why yet. The error is different between the first frame of rendering and subsequent frames.

(Last updated: 2016)

## Intel

Same error as AMD (below). Tested on Intel Iris 540.

(Last updated: 2016)

## AMD

Tested on R9 380.

~~Currently has an error and returns null when I call `wglDXRegisterObjectNV`. The error message says "The system cannot open the device or file specified." Haven't yet figured out how to get around this.~~

Update: Apparently this error no longer happens, and it works now. Tested on driver version 18.9.1.

(Last updated: 2018)

## Conclusions

This extension's support doesn't yet match the capabilities of using DXGI with plain D3D, mostly because the implementation of the extension has not been updated for Windows 10 style FLIP swap chains. It works okay with older swap chain types.

Since most of the bugginess comes from trying to access swap chain buffers from OpenGL, you might be able to get away with using this extension by not trying to wrap the swap chain buffers and instead just doing a copy at the end of your frame. Unfortunately, it's easy to introduce extra presentation latency that way.
