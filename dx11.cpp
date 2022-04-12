#include "dx11.hpp"

ComPtr<ID3D11Device> g_pd3dDevice = NULL;
ComPtr<IDXGIDevice> g_pdxgiDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
ComPtr<IDXGISwapChain1> g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
ComPtr<IDCompositionDesktopDevice> g_pdcompDevice = NULL;
ComPtr<IDCompositionDevice3> g_pdcompDevice3 = NULL;

void HR(HRESULT const result)
{
	if (S_OK != result)
	{
		throw ComException(result);
	}
}

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	HR(D3D11CreateDevice(nullptr,    // Adapter
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,    // Module
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, // Highest available feature level
		D3D11_SDK_VERSION,
		&g_pd3dDevice,
		nullptr,    // Actual feature level
		&g_pd3dDeviceContext));  // Device context

	HR(g_pd3dDevice.As(&g_pdxgiDevice));

	ComPtr<IDXGIFactory2> dxFactory;
	HR(CreateDXGIFactory2(
		DXGI_CREATE_FACTORY_DEBUG,
		__uuidof(dxFactory),
		reinterpret_cast<void**>(dxFactory.GetAddressOf())));

	DXGI_SWAP_CHAIN_DESC1 description = {};
	description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	description.BufferCount = 2;
	description.SampleDesc.Count = 1;
	description.SampleDesc.Quality = 0;
	description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	description.Scaling = DXGI_SCALING_STRETCH;

	RECT rect = {};
	GetClientRect(hWnd, &rect);
	description.Width = rect.right - rect.left;
	description.Height = rect.bottom - rect.top;

	HR(dxFactory->CreateSwapChainForComposition(g_pdxgiDevice.Get(),
		&description,
		nullptr, // Don’t restrict
		g_pSwapChain.GetAddressOf()));

	HR(DCompositionCreateDevice3(
		g_pdxgiDevice.Get(),
		__uuidof(g_pdcompDevice),
		reinterpret_cast<void**>(g_pdcompDevice.GetAddressOf())));

	HR(g_pdcompDevice->QueryInterface(__uuidof(IDCompositionDevice3), &g_pdcompDevice3));

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}
