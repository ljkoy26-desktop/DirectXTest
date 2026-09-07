#include <windows.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define FULLSCREEN_MODE 0  // 1 = 풀스크린, 0 = 창 모드

IDXGISwapChain*         g_lpSwapChain        = NULL;
ID3D11Device*           g_lpD3DDevice        = NULL;
ID3D11DeviceContext*    g_lpD3DContext       = NULL;
ID3D11RenderTargetView* g_lpRenderTargetView = NULL;
HWND                    g_hWnd               = NULL;
BOOL                    g_bActive            = FALSE;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirect3D(HWND hWnd);
bool CreateRenderTarget();
void ReleaseRenderTarget();
void ResizeSwapChain(UINT uWidth, UINT uHeight);
void RenderFrame();
void ReleaseDirect3D();

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    if (!InitWindow(hInstance, SW_SHOWDEFAULT))
    {
        return 1;
    }

    if (!InitDirect3D(g_hWnd))
    {
        ReleaseDirect3D();
        return 1;
    }

    MSG msg = { 0 };

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        if (!g_bActive)
        {
            WaitMessage();
            continue;
        }

        RenderFrame();
    }

    ReleaseDirect3D();
    return static_cast<int>(msg.wParam);
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = TEXT("DirectX11SampleWndClass");

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

#if FULLSCREEN_MODE
    DWORD dwStyle = WS_POPUP;
    RECT rcWindow = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
#else
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rcWindow = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    AdjustWindowRect(&rcWindow, dwStyle, FALSE);
#endif

    g_hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        TEXT("DirectX11 Sample"),
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
        NULL, NULL, hInstance, NULL);

    if (g_hWnd == NULL)
    {
        return false;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    g_bActive = TRUE;
    return true;
}

bool InitDirect3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC scd = { 0 };
    scd.BufferCount                        = 1;
    scd.BufferDesc.Width                   = SCREEN_WIDTH;
    scd.BufferDesc.Height                  = SCREEN_HEIGHT;
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hWnd;
    scd.SampleDesc.Count                   = 1;
    scd.SampleDesc.Quality                 = 0;
    scd.Windowed                           = !FULLSCREEN_MODE;
    scd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &scd,
        &g_lpSwapChain,
        &g_lpD3DDevice,
        NULL,
        &g_lpD3DContext);

    if (FAILED(hr))
    {
        return false;
    }

    if (!CreateRenderTarget())
    {
        return false;
    }

    return true;
}

bool CreateRenderTarget()
{
    if (g_lpSwapChain == NULL || g_lpD3DDevice == NULL)
    {
        return false;
    }

    ID3D11Texture2D* lpBackBuffer = NULL;
    HRESULT hr = g_lpSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&lpBackBuffer));

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpD3DDevice->CreateRenderTargetView(lpBackBuffer, NULL, &g_lpRenderTargetView);
    lpBackBuffer->Release();

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void ReleaseRenderTarget()
{
    if (g_lpRenderTargetView == NULL)
    {
        return;
    }

    g_lpRenderTargetView->Release();
    g_lpRenderTargetView = NULL;
}

void ResizeSwapChain(UINT uWidth, UINT uHeight)
{
    if (g_lpSwapChain == NULL)
    {
        return;
    }

    if (uWidth == 0 || uHeight == 0)
    {
        return;
    }

    ReleaseRenderTarget();
    g_lpD3DContext->OMSetRenderTargets(0, NULL, NULL);

    HRESULT hr = g_lpSwapChain->ResizeBuffers(0, uWidth, uHeight, DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(hr))
    {
        return;
    }

    CreateRenderTarget();
}

void RenderFrame()
{
    if (g_lpD3DContext == NULL || g_lpRenderTargetView == NULL)
    {
        return;
    }

    static float fColorIndex = 0.0f;
    fColorIndex += 1.0f;

    if (fColorIndex > 255.0f)
    {
        fColorIndex = 0.0f;
    }

    const float fClearColor[4] =
    {
        fColorIndex / 255.0f,
        0.0f,
        1.0f - (fColorIndex / 255.0f),
        1.0f
    };

    D3D11_VIEWPORT viewport = { 0 };
    viewport.Width    = static_cast<float>(SCREEN_WIDTH);
    viewport.Height   = static_cast<float>(SCREEN_HEIGHT);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    g_lpD3DContext->RSSetViewports(1, &viewport);
    g_lpD3DContext->OMSetRenderTargets(1, &g_lpRenderTargetView, NULL);
    g_lpD3DContext->ClearRenderTargetView(g_lpRenderTargetView, fClearColor);

    g_lpSwapChain->Present(1, 0);
}

void ReleaseDirect3D()
{
    ReleaseRenderTarget();

    if (g_lpSwapChain != NULL)
    {
        g_lpSwapChain->Release();
        g_lpSwapChain = NULL;
    }

    if (g_lpD3DContext != NULL)
    {
        g_lpD3DContext->Release();
        g_lpD3DContext = NULL;
    }

    if (g_lpD3DDevice != NULL)
    {
        g_lpD3DDevice->Release();
        g_lpD3DDevice = NULL;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
    {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
        return 0;
    }

    if (uMsg == WM_SIZE)
    {
        ResizeSwapChain(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }

    if (uMsg == WM_ACTIVATEAPP)
    {
        g_bActive = static_cast<BOOL>(wParam);
        return 0;
    }

    if (uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
