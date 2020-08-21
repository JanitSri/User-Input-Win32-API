#include <windows.h>
#include <windowsX.h>
#include <d2d1.h>
#include <stdio.h>
#pragma comment(lib, "d2d1")

#include "basewin.h"

/*
 - Direct2D is an immediate-mode API
 - Immediate-mode API is procedural;
    - Each time a new frame is drawn, the application directly issues the drawing commands.
      The graphics library does not store a scene model between frames.
      Instead, the application keeps track of the scene
*/

/*
 - D2D1 namespace contains helper functions and classes, are not
   strictly part of the Direct2D API — you can program Direct2D without using them — but
   they help simplify your code
 - D2D1 namespace contains:
    - A ColorF class for constructing color values
    - A Matrix3x2F for constructing transformation matrices
    - A set of functions to initialize Direct2D structures
*/

/*
 - render target is the location where the program will draw, it is typically the window (specifically, the client area of the window)
    - render target is represented by the 'ID2D1RenderTarget' interface
 - resource is an object the program uses for drawing, i.e. brush, stroke style, geometry, mesh
    - every resource type is represented by an interface that derives from 'ID2D1Resource'
*/


template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}


class DPIScale
{
    /*
     - helper class that converts pixels into DIPs 
     - Mouse coordinates are given in physical pixels, but Direct2D expects device-independent pixels (DIPs)
     - To handle high-DPI settings correctly, you must translate the pixel coordinates into DIPs
    */

    static float scaleX;
    static float scaleY;

public:
    static void Initialize(HWND hWnd)
    {
        FLOAT dpiX, dpiY;

        dpiX = dpiY = static_cast<FLOAT>(GetDpiForWindow(hWnd));
        scaleX = dpiX / 96.0f;
        scaleY = dpiY / 96.0f;
    }

    template <typename T>
    static D2D1_POINT_2F PixelsToDips(T x, T y)
    {
        return D2D1::Point2F(static_cast<float>(x) / scaleX, static_cast<float>(y) / scaleY);
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;


class MainWindow : public BaseWindow<MainWindow>
{
    // 'pFactory' is a factory object to create other objects; render targets and device-independent resources, such as stroke styles and geometries
    ID2D1Factory* pFactory;

    // Device - dependent resources, such as brushesand bitmaps, are created by the render target object
    ID2D1HwndRenderTarget* pRenderTarget; // render target pointer
    ID2D1SolidColorBrush* pBrush; // brush pointer
    D2D1_ELLIPSE ellipse;
    D2D1_POINT_2F ptMouse; // stores the mouse-down position while the user is dragging the mouse


    void CalculateLayout();
    HRESULT CreateGraphicsResources();
    void DiscardGraphicsResources();
    void OnPaint();
    void Resize();
    void OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void OnLButtonUp();
    void OnMouseMove(int pixelX, int pixelY, DWORD flags);

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL), 
        ellipse(D2D1::Ellipse(D2D1::Point2F(), 0, 0)), ptMouse(D2D1::Point2F()) {}

    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};


// Recalculate drawing layout when the size of the window changes 
void MainWindow::CalculateLayout(){}

// create the two resources, i.e. render target and brush
HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        /*
         - 'CreateHwndRenderTarget' creates the render target
            - first param, specifies options that are common to any type of render target, pass in default options by calling the helper function 'D2D1::RenderTargetProperties'
            - second param, specifies the handle to the window plus the size of the render target, in pixels
            - third param, receives an 'ID2D1HwndRenderTarget' pointer
        */

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            // create solid-color brush
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 0, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);

            if (SUCCEEDED(hr))
            {
                CalculateLayout();
            }
        }
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        // 'ID2D1RenderTarget' interface is used for all drawing operations

        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw(); // signals the start of drawing 

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::BlanchedAlmond)); // fill the render target with a solid color 
        pRenderTarget->FillEllipse(ellipse, pBrush); // draws a filled ellipse, using the specified brush for the fill

        hr = pRenderTarget->EndDraw(); //  signals the completion of drawing for this frame

        /*
         - BeginDraw, Clear, and FillEllipse methods all have a void return type
         - if an error occurs during the execution of any of these methods, the error is signaled through the return
           value of the EndDraw method
        */


        // Direct2D signals a lost device by returning the error code 'D2DERR_RECREATE_TARGET' from the EndDraw method
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc); // get the new size of the client area 

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size); // updates the size of the render target, also specified in pixels
        CalculateLayout();
        InvalidateRect(m_hwnd, NULL, FALSE); // forces a repaint by adding the entire client area to the window's update region
    }
}


void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    // begin capturing the mouse
    SetCapture(m_hwnd);  

    // store the position of the mouse in the ptMouse variable, position defines the upper left corner of the bounding box for the ellipse
    ellipse.point = ptMouse = DPIScale::PixelsToDips(pixelX, pixelY); 
    
    // reset the ellipse structure 
    ellipse.radiusX = ellipse.radiusY = 1.0f;

    // force the window to be repainted 
    InvalidateRect(m_hwnd, NULL, FALSE);
}


void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    // check whether left mouse button is down, if it is, recalculate the ellipse and repaint the window
    if (flags & MK_LBUTTON) 
    {
        const D2D1_POINT_2F dips = DPIScale::PixelsToDips(pixelX, pixelY);

        const float width = (dips.x - ptMouse.x) / 2;
        const float height = (dips.y - ptMouse.y) / 2;
        const float x1 = ptMouse.x + width;
        const float y1 = ptMouse.y + height;

        // ellipse is defined by the center point and x - and y - radii
        ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);

        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnLButtonUp()
{
    ReleaseCapture();
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Draw Circle", WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    ShowWindow(win.Window(), nCmdShow);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}


// 'MainWindow::HandleMessage' is used to implement the window procedure
LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    wchar_t msg[32];
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory))) // create Direct2D factory object 
            /*
             - first param is the flag that specifies creation objects
                - 'D2D1_FACTORY_TYPE_SINGLE_THREADED' flag means that you will not call Direct2D from multiple threads
                - to support calls from multiple threads, specify 'D2D1_FACTORY_TYPE_MULTI_THREADED'
             - second param, receives a pointer to the 'ID2D1Factory' interface
            */
        {
            return -1;  // Fail CreateWindowEx.
        }
        DPIScale::Initialize(m_hwnd);
        return 0;
    
    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp();
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;
    
    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_SIZE:
        Resize();
        return 0;

    case WM_SYSKEYDOWN:
        /*
         - indicates a system key, which is a key stroke that invokes a system command
         - two types
            - ALT + any key -> various combinations invoke system commands
            - F10 -> activates the menu bar of the window 
         - if WM_SYSKEYDOWN message is intercepted, call DefWindowProc afterward 
        */
        swprintf_s(msg, L"WM_SYSKEYDOWN: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_SYSCHAR:
        /*
         - generated from WM_SYSKEYDOWN messages
         - indicates a system character
         - pass message directly to DefWindowProc (do not treat as text that the user has typed)
        */
        swprintf_s(msg, L"WM_SYSCHAR: %c\n", (wchar_t)wParam);
        OutputDebugString(msg);
        break;

    case WM_SYSKEYUP: // key release
        swprintf_s(msg, L"WM_SYSKEYUP: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_KEYDOWN:
        /*
         - could implement keyboard shortcuts by handling individual WM_KEYDOWN messages, but accelerator tables provide a better solution
        */
        swprintf_s(msg, L"WM_KEYDOWN: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_KEYUP: // key release
        swprintf_s(msg, L"WM_KEYUP: 0x%x\n", wParam);
        OutputDebugString(msg);
        break;

    case WM_CHAR:
        /*
         - generated from WM_KEYDOWN messages
         - character input 
         - data type id wchar_t
         - avoid using WM_CHAR to implement keyboard shortcuts
        */
        swprintf_s(msg, L"WM_CHAR: %c\n", (wchar_t)wParam);
        OutputDebugString(msg);
        break;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

