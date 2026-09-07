#include <windows.h>
#include <d3d8.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "d3d8.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SQUARE_SIZE   32
#define FULLSCREEN_MODE 0  // 1 = 풀스크린, 0 = 창 모드

LPDIRECT3D8           g_lpD3D         = NULL;
LPDIRECT3DDEVICE8     g_lpD3DDevice   = NULL;
LPDIRECTINPUT8        g_lpDI8         = NULL;
LPDIRECTINPUTDEVICE8  g_lpDIDKeyboard = NULL;
HWND                  g_hWnd          = NULL;
BOOL                  g_bActive       = FALSE;
int                   g_nSquareX      = 0;
int                   g_nSquareY      = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirect3D(HWND hWnd);
bool InitDirectInput(HWND hWnd);
bool RestoreDevice();
void UpdateInput();
void RenderFrame();
void ReleaseDirect3D();
void ReleaseDirectInput();

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

    if (!InitDirectInput(g_hWnd))
    {
        ReleaseDirectInput();
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

        UpdateInput();
        RenderFrame();
    }

    ReleaseDirectInput();
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
    wc.lpszClassName = TEXT("DirectX8SampleWndClass");

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
        TEXT("DirectX8 Sample"),
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
    g_lpD3D = Direct3DCreate8(D3D_SDK_VERSION);

    if (g_lpD3D == NULL)
    {
        return false;
    }

    D3DDISPLAYMODE d3ddm = { 0 };
    HRESULT hr = g_lpD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

    if (FAILED(hr))
    {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed         = !FULLSCREEN_MODE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth  = SCREEN_WIDTH;
    d3dpp.BackBufferHeight = SCREEN_HEIGHT;

    hr = g_lpD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_lpD3DDevice);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool InitDirectInput(HWND hWnd)
{
    HRESULT hr = DirectInput8Create(
        GetModuleHandle(NULL),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&g_lpDI8),
        NULL);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDI8->CreateDevice(GUID_SysKeyboard, &g_lpDIDKeyboard, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDIDKeyboard->SetDataFormat(&c_dfDIKeyboard);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDIDKeyboard->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    if (FAILED(hr))
    {
        return false;
    }

    g_lpDIDKeyboard->Acquire();

    g_nSquareX = (SCREEN_WIDTH - SQUARE_SIZE) / 2;
    g_nSquareY = (SCREEN_HEIGHT - SQUARE_SIZE) / 2;
    return true;
}

bool RestoreDevice()
{
    if (g_lpD3DDevice == NULL)
    {
        return false;
    }

    D3DDISPLAYMODE d3ddm = { 0 };
    HRESULT hr = g_lpD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

    if (FAILED(hr))
    {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed         = !FULLSCREEN_MODE;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth  = SCREEN_WIDTH;
    d3dpp.BackBufferHeight = SCREEN_HEIGHT;

    hr = g_lpD3DDevice->Reset(&d3dpp);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void UpdateInput()
{
    if (g_lpDIDKeyboard == NULL)
    {
        return;
    }

    BYTE abKeyState[256] = { 0 };
    HRESULT hr = g_lpDIDKeyboard->GetDeviceState(sizeof(abKeyState), abKeyState);

    if (FAILED(hr))
    {
        g_lpDIDKeyboard->Acquire();
        return;
    }

    if (abKeyState[DIK_ESCAPE] & 0x80)
    {
        PostMessage(g_hWnd, WM_CLOSE, 0, 0);
        return;
    }

    if (abKeyState[DIK_LEFT] & 0x80)
    {
        g_nSquareX -= 4;
    }

    if (abKeyState[DIK_RIGHT] & 0x80)
    {
        g_nSquareX += 4;
    }

    if (abKeyState[DIK_UP] & 0x80)
    {
        g_nSquareY -= 4;
    }

    if (abKeyState[DIK_DOWN] & 0x80)
    {
        g_nSquareY += 4;
    }

    if (g_nSquareX < 0)
    {
        g_nSquareX = 0;
    }

    if (g_nSquareX > SCREEN_WIDTH - SQUARE_SIZE)
    {
        g_nSquareX = SCREEN_WIDTH - SQUARE_SIZE;
    }

    if (g_nSquareY < 0)
    {
        g_nSquareY = 0;
    }

    if (g_nSquareY > SCREEN_HEIGHT - SQUARE_SIZE)
    {
        g_nSquareY = SCREEN_HEIGHT - SQUARE_SIZE;
    }
}

void RenderFrame()
{
    if (g_lpD3DDevice == NULL)
    {
        return;
    }

    HRESULT hr = g_lpD3DDevice->TestCooperativeLevel();

    if (hr == D3DERR_DEVICELOST)
    {
        return;
    }

    if (hr == D3DERR_DEVICENOTRESET)
    {
        RestoreDevice();
        return;
    }

    static DWORD dwColorIndex = 0;
    dwColorIndex = (dwColorIndex + 1) % 256;

    g_lpD3DDevice->Clear(
        0, NULL,
        D3DCLEAR_TARGET,
        D3DCOLOR_XRGB(dwColorIndex, 0, 255 - dwColorIndex),
        1.0f, 0);

    D3DRECT rcSquare = { g_nSquareX, g_nSquareY, g_nSquareX + SQUARE_SIZE, g_nSquareY + SQUARE_SIZE };

    g_lpD3DDevice->Clear(
        1, &rcSquare,
        D3DCLEAR_TARGET,
        D3DCOLOR_XRGB(255, 255, 0),
        1.0f, 0);

    if (FAILED(g_lpD3DDevice->BeginScene()))
    {
        return;
    }

    g_lpD3DDevice->EndScene();
    g_lpD3DDevice->Present(NULL, NULL, NULL, NULL);
}

void ReleaseDirect3D()
{
    if (g_lpD3DDevice != NULL)
    {
        g_lpD3DDevice->Release();
        g_lpD3DDevice = NULL;
    }

    if (g_lpD3D != NULL)
    {
        g_lpD3D->Release();
        g_lpD3D = NULL;
    }
}

void ReleaseDirectInput()
{
    if (g_lpDIDKeyboard != NULL)
    {
        g_lpDIDKeyboard->Unacquire();
        g_lpDIDKeyboard->Release();
        g_lpDIDKeyboard = NULL;
    }

    if (g_lpDI8 != NULL)
    {
        g_lpDI8->Release();
        g_lpDI8 = NULL;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
    {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
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
