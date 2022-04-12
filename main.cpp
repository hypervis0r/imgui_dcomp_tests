#include <imgui.h>
#include <stdio.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>

#include "dx11.hpp"

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")

#pragma comment(lib, "D3D11.lib")

enum AccentState
{
	ACCENT_DISABLED = 0,
	ACCENT_ENABLE_GRADIENT = 1,
	ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
	ACCENT_ENABLE_BLURBEHIND = 3,
	ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
	ACCENT_INVALID_STATE = 5
};

struct AccentPolicy
{
	AccentState AccentState;
	unsigned int AccentFlags;
	unsigned int GradientColor;
	unsigned int AnimationId;
};

// Data
static bool g_toolActive = true;
static float my_color[4] = { 0. };

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RenderFrame(HWND hwnd, bool* my_tool_active);

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int
win32_dpi_scale(
	int value,
	UINT dpi
) {
	return (int)((float)value * dpi / 96);
}

static float
get_titlebar_height() 
{
	float res = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
	
	return res;
}

static bool
win32_window_is_maximized(
	HWND handle
) {
	WINDOWPLACEMENT placement = { 0 };
	placement.length = sizeof(WINDOWPLACEMENT);
	if (GetWindowPlacement(handle, &placement)) {
		return placement.showCmd == SW_SHOWMAXIMIZED;
	}
	return false;
}

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND handle, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (ImGui_ImplWin32_WndProcHandler(handle, message, w_param, l_param))
		return true;

	ImGuiIO& io = ImGui::GetIO();

	/*
		This message handling code gets rid of the Windows DWM title bar, and handles 
		all the important things like moving the window and resizing.

		this was all stolen from https://github.com/grassator/win32-window-custom-titlebar/blob/main/main.c
	*/
	switch (message)
	{
	case WM_PAINT:
		RenderFrame(handle, &g_toolActive);
		break;
	case WM_SIZE:
		if (g_pd3dDevice != NULL && w_param != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, LOWORD(l_param), HIWORD(l_param), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();

			RedrawWindow(handle, NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		return 0;
	case WM_NCCALCSIZE: {
		if (!w_param) return DefWindowProc(handle, message, w_param, l_param);
		UINT dpi = GetDpiForWindow(handle);

		int frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
		int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
		int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

		NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)l_param;
		RECT* requested_client_rect = params->rgrc;

		requested_client_rect->right -= frame_x + padding;
		requested_client_rect->left += frame_x + padding;
		requested_client_rect->bottom -= frame_y + padding;

		if (win32_window_is_maximized(handle)) {
			requested_client_rect->top += padding;
		}

		return 0;
	}
	case WM_CREATE: {
		RECT size_rect;
		GetWindowRect(handle, &size_rect);

		// Inform the application of the frame change to force redrawing with the new
		// client area that is extended into the title bar
		SetWindowPos(
			handle, NULL,
			size_rect.left, size_rect.top,
			size_rect.right - size_rect.left, size_rect.bottom - size_rect.top,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
		);

		// Initialize Direct3D
		if (!CreateDeviceD3D(handle))
		{
			CleanupDeviceD3D();
			//::UnregisterClass(wc.lpszClassName, wc.hInstance);
			return 1;
		}

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(handle);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
		break;
	}
	case WM_NCHITTEST: {
		// Let the default procedure handle resizing areas
		LRESULT hit = DefWindowProc(handle, message, w_param, l_param);
		switch (hit) {
		case HTNOWHERE:
		case HTRIGHT:
		case HTLEFT:
		case HTTOPLEFT:
		case HTTOP:
		case HTTOPRIGHT:
		case HTBOTTOMRIGHT:
		case HTBOTTOM:
		case HTBOTTOMLEFT:
			return hit;
		}

		// Looks like adjustment happening in NCCALCSIZE is messing with the detection
		// of the top hit area so manually fixing that.
		UINT dpi = GetDpiForWindow(handle);
		int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
		int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
		POINT cursor_point = { 0 };
		cursor_point.x = LOWORD(l_param);
		cursor_point.y = HIWORD(l_param);
		ScreenToClient(handle, &cursor_point);
		if (cursor_point.y > 0 && cursor_point.y < frame_y + padding) {
			return HTTOP;
		}

		// Since we are drawing our own caption, this needs to be a custom test
		if (cursor_point.y < get_titlebar_height()) {
			return HTCAPTION;
		}

		return HTCLIENT;
	}
	case WM_NCLBUTTONDBLCLK:
		return 0;
	// Map button clicks to the right messages for the window
	case WM_NCLBUTTONDOWN: {
		/*io.MouseDown[0] = true;
		ImGui::Render();
		printf("my ass\n");*/
		POINTS l = *(POINTS*)&l_param;

		//LRESULT a = SendMessageA(handle, WM_LBUTTONDOWN, MK_LBUTTON, MAKELONG(l.x, l.y));

		::DefWindowProc(handle, message, w_param, l_param);

		//ReleaseCapture();
		return 0;
	}
	case WM_NCLBUTTONUP: {
		/*io.MouseDown[0] = true;
		ImGui::Render();
		printf("my ass\n");*/
		POINTS l = *(POINTS*)&l_param;

		//LRESULT a = SendMessageA(handle, WM_LBUTTONUP, MK_LBUTTON, MAKELONG(l.x, l.y));
		break;
	}
	case WM_NCMOUSEMOVE:
		POINTS l = *(POINTS*)&l_param;
		SendMessageA(handle, WM_MOUSEMOVE, 0, MAKELONG(l.x, l.y));
		break;
	case WM_NCMOUSELEAVE:
	//case WM_NCLBUTTONUP:
		SendMessageA(handle, WM_MOUSELEAVE, 0, 0);
		break;
	case WM_SYSCOMMAND:
		if ((w_param & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}

	return ::DefWindowProc(handle, message, w_param, l_param);
}

void RenderFrame(HWND hwnd, bool* my_tool_active)
{
	ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
	RECT main_window_rect = { 0 };
	LONG main_window_width = 0;
	LONG main_window_height = 0;

	//SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), RGB(255, 255, 255), LWA_COLORKEY | LWA_ALPHA);

	POINT ptSrc = { 0,0 };

	// Create a window called "My First Tool", with a menu bar.
	GetWindowRect(hwnd, &main_window_rect);
	main_window_width = main_window_rect.right - main_window_rect.left;
	main_window_height = main_window_rect.bottom - main_window_rect.top;
	SIZE wndSize = { main_window_width, main_window_height };

	//HDC hdcScreen = GetDC(NULL);
	//HDC hdcWindow = GetDC(hwnd);
	//HDC hdcWindowMem = CreateCompatibleDC(hdcWindow);
	//HBITMAP memBitmap = ::CreateCompatibleBitmap(hdcWindow, main_window_width, main_window_height);
	//HGDIOBJ hOldBmp = SelectObject(hdcWindowMem, memBitmap);
	//BitBlt(hdcWindowMem, 0, 0, main_window_width, main_window_height, hdcWindow, 0, 0, SRCCOPY);
	////SelectObject(hdcWindowMem, hOldBmp);

	//BLENDFUNCTION blend = { AC_SRC_OVER, 0,  0, AC_SRC_ALPHA };
	//UpdateLayeredWindow(hwnd, hdcScreen, &ptSrc, &wndSize, hdcWindowMem, &ptSrc, RGB(0, 0, 0), &blend, ULW_ALPHA);

	//DeleteDC(hdcWindowMem);
	//ReleaseDC(hwnd, hdcWindow);
	//DeleteObject(memBitmap);

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0., 0.));
	ImGui::SetNextWindowSize(ImVec2(main_window_width - 15, main_window_height));
	ImGui::SetNextWindowBgAlpha(0.0);
	ImGui::Begin("My First Tool", my_tool_active, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
			if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
			if (ImGui::MenuItem("Close", "Ctrl+W")) { if (my_tool_active) *my_tool_active = false; }
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::Begin("Funny Sub-Window", NULL);
	ImVec2 sub_pos = ImGui::GetWindowPos();

	if (sub_pos.y < get_titlebar_height())
	{
		sub_pos.y = get_titlebar_height();
		ImGui::SetWindowPos(sub_pos);
	}
	ImGui::Text("wawaweewa");
	ImGui::End();

	// Edit a color (stored as ~4 floats)
	ImGui::ColorEdit4("Color", my_color);

	// Plot some values
	const float my_values[] = { 0.2f, 0.1f, 1.0f, 0.5f, 0.9f, 2.2f };
	ImGui::PlotLines("Frame Times", my_values, IM_ARRAYSIZE(my_values));

	// Display contents in a scrolling region
	ImGui::TextColored(ImVec4(1, 1, 0, 1), "Current Main Window Size");
	ImGui::Text("Width: %d", main_window_width);
	ImGui::Text("Height: %d", main_window_height);
	ImGui::End();

	ImGui::Render();
	const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
	g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	g_pSwapChain->Present(1, 0); // Present with vsync
	//g_pSwapChain->Present(0, 0); // Present without vsync
}

int main(void)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	ImGui::GetStyle().WindowRounding = 0.0f;// <- Set this on init or use ImGui::PushStyleVar()
	ImGui::GetStyle().ChildRounding = 0.0f;
	ImGui::GetStyle().FrameRounding = 0.0f;
	ImGui::GetStyle().GrabRounding = 0.0f;
	ImGui::GetStyle().PopupRounding = 0.0f;
	ImGui::GetStyle().ScrollbarRounding = 0.0f;
	ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = ImGui::GetStyle().Colors[ImGuiCol_TitleBg];
	ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
	//ImGui::StyleColorsClassic();
	
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
	::RegisterClassEx(&wc);

	int window_style
		= WS_THICKFRAME   // required for a standard resizeable window
		| WS_SYSMENU      // Explicitly ask for the titlebar to support snapping via Win +  / Win + 
		| WS_MAXIMIZEBOX  // Add maximize button to support maximizing via mouse dragging
						  // to the top of the screen
		| WS_MINIMIZEBOX  // Add minimize button to support minimizing by clicking on the taskbar icon
		| WS_VISIBLE;     // Make window visible after it is created (not important)

	HWND hwnd = ::CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, _T("Dear ImGui DirectX11 Example"), window_style, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, NULL, NULL, NULL, NULL);
	//SetWindowLong(hwnd, GWL_STYLE, WS_THICKFRAME); //remove all window styles, check MSDN for details
	
	
	SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP);
	//SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

	/*struct AccentPolicy accent = { };
	accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
	accent.GradientColor = (50 << 24) | (0x990000 & 0xFFFFFF);;

	DwmSetWindowAttribute(hwnd, 19, &accent, sizeof(accent));*/

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;

	// Main loop
	while (g_toolActive)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, hwnd, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				g_toolActive = false;
		}
		if (!g_toolActive)
			break;

		RenderFrame(hwnd, &g_toolActive);
	}
}