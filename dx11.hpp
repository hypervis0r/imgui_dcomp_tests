#pragma once
#include <imgui.h>
#include <stdio.h>
#include <imgui_impl_win32.h>
#include "imgui_impl_dx11.h"
#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <wrl.h>
#include <dxgi1_3.h>
#include <d3d11_2.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <dcomp.h>
#include <dxgidebug.h>

#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Microsoft::WRL;

extern ComPtr<ID3D11Device> g_pd3dDevice;
extern ComPtr<IDXGIDevice> g_pdxgiDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ComPtr<IDXGISwapChain1> g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

extern ComPtr<IDCompositionDesktopDevice> g_pdcompDevice;
extern ComPtr<IDCompositionDevice3> g_pdcompDevice3;

struct ComException
{
    HRESULT result;
    ComException(HRESULT const value) :
        result(value)
    {}
};

void HR(HRESULT const result);
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();