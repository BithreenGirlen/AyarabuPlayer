
#include <atlbase.h>

#include "d2_image_drawer.h"

#pragma comment (lib,"D2d1.lib")
#pragma comment (lib,"d3d11.lib")
#pragma comment (lib,"dxguid.lib")

CD2ImageDrawer::CD2ImageDrawer(HWND hWnd)
{
	CComPtr<ID3D11Device>pD3d11Device;
	HRESULT hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED, nullptr, 0, D3D11_SDK_VERSION,
		&pD3d11Device, nullptr, nullptr);
	if (FAILED(hr))return;

	CComPtr<IDXGIDevice1> pDxgDevice1;
	hr = pD3d11Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&pDxgDevice1);
	if (FAILED(hr))return;

	hr = pDxgDevice1->SetMaximumFrameLatency(1);
	if (FAILED(hr))return;

	CComPtr<IDXGIAdapter> pDxgiAdapter;
	hr = pDxgDevice1->GetAdapter(&pDxgiAdapter);
	if (FAILED(hr))return;

	CComPtr<IDXGIFactory2> pDxgiFactory2;
	hr = pDxgiAdapter->GetParent(IID_PPV_ARGS(&pDxgiFactory2));
	if (FAILED(hr))return;

	hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2d1Factory1);
	if (FAILED(hr))return;

	CComPtr<ID2D1Device> pD2d1Device;
	hr = m_pD2d1Factory1->CreateDevice(pDxgDevice1, &pD2d1Device);
	if (FAILED(hr))return;

	hr = pD2d1Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2d1DeviceContext);
	if (FAILED(hr))return;

	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

	hr = pDxgiFactory2->CreateSwapChainForHwnd(pDxgDevice1, hWnd, &desc, nullptr, nullptr, &m_pDxgiSwapChain1);
	if (FAILED(hr))return;

	m_pD2d1DeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	m_pD2d1DeviceContext->SetUnitMode(D2D1_UNIT_MODE_DIPS);
	m_pD2d1DeviceContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);

	D2D1_RENDERING_CONTROLS renderingControls{};
	m_pD2d1DeviceContext->GetRenderingControls(&renderingControls);

	renderingControls.bufferPrecision = D2D1_BUFFER_PRECISION_8BPC_UNORM_SRGB;
	m_pD2d1DeviceContext->SetRenderingControls(renderingControls);
}

CD2ImageDrawer::~CD2ImageDrawer()
{
#ifdef _DEBUG
	releaseCpuImageBuffer();
#endif

	if (m_pDxgiSwapChain1 != nullptr)
	{
		m_pDxgiSwapChain1->Release();
		m_pDxgiSwapChain1 = nullptr;
	}

	if (m_pD2d1DeviceContext != nullptr)
	{
		m_pD2d1DeviceContext->Release();
		m_pD2d1DeviceContext = nullptr;
	}

	if (m_pD2d1Factory1 != nullptr)
	{
		m_pD2d1Factory1->Release();
		m_pD2d1Factory1 = nullptr;
	}
}

void CD2ImageDrawer::clear(const D2D1::ColorF& colour)
{
	if (m_pD2d1DeviceContext == nullptr) return;

	m_pD2d1DeviceContext->BeginDraw();
	m_pD2d1DeviceContext->Clear(colour);
	m_pD2d1DeviceContext->EndDraw();
}
#ifdef _DEBUG
bool CD2ImageDrawer::drawCpuImage(const void* srcData, const UINT32 width, const UINT32 height, const UINT32 stride, const D2D1_POINT_2F& targetOffset, const D2D_VECTOR_2F& sourceOffset, float fScale)
{
	if (m_pD2d1DeviceContext == nullptr || m_pDxgiSwapChain1 == nullptr)return false;

	if (srcData == nullptr || width == 0 || height == 0)return false;

	bool bRet = checkCpuImageBufferSize(width, height);
	if (!bRet)return false;

	D2D1_RECT_U rc = { 0, 0, width, height };
	HRESULT hr = m_pCpuImageBuffer->CopyFromMemory(&rc, srcData, stride);
	if (FAILED(hr))return false;

	CComPtr<ID2D1Effect> pD2d1Effect;
	hr = m_pD2d1DeviceContext->CreateEffect(CLSID_D2D1Scale, &pD2d1Effect);

	pD2d1Effect->SetInput(0, m_pCpuImageBuffer);
	hr = pD2d1Effect->SetValue(D2D1_SCALE_PROP_CENTER_POINT, sourceOffset);
	hr = pD2d1Effect->SetValue(D2D1_SCALE_PROP_SCALE, D2D1::Vector2F(fScale, fScale));

	m_pD2d1DeviceContext->BeginDraw();
	m_pD2d1DeviceContext->DrawImage(pD2d1Effect, targetOffset, D2D1::RectF(sourceOffset.x, sourceOffset.y, width * fScale, height * fScale), D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, D2D1_COMPOSITE_MODE_SOURCE_OVER);
	m_pD2d1DeviceContext->EndDraw();

	return true;
}
#endif
bool CD2ImageDrawer::draw(ID2D1Bitmap* pD2d1Bitmap, const D2D1_POINT_2F* targetOffset, const D2D1_RECT_F* srcRect)
{
	if (pD2d1Bitmap == nullptr)return false;

	m_pD2d1DeviceContext->BeginDraw();
	m_pD2d1DeviceContext->DrawImage(pD2d1Bitmap, targetOffset, srcRect, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, D2D1_COMPOSITE_MODE_SOURCE_OVER);
	m_pD2d1DeviceContext->EndDraw();

	return true;
}

bool CD2ImageDrawer::scaledDraw(ID2D1Bitmap* pD2d1Bitmap, const D2D1_POINT_2F& targetOffset, const D2D_VECTOR_2F& sourceOffset, float fScale)
{
	if (pD2d1Bitmap == nullptr)return false;

	CComPtr<ID2D1Effect> pD2d1Effect;
	HRESULT hr = m_pD2d1DeviceContext->CreateEffect(CLSID_D2D1Scale, &pD2d1Effect);

	pD2d1Effect->SetInput(0, pD2d1Bitmap);
	hr = pD2d1Effect->SetValue(D2D1_SCALE_PROP_CENTER_POINT, sourceOffset);
	hr = pD2d1Effect->SetValue(D2D1_SCALE_PROP_SCALE, D2D1::Vector2F(fScale, fScale));

	D2D1_SIZE_U s = pD2d1Bitmap->GetPixelSize();

	m_pD2d1DeviceContext->BeginDraw();
	m_pD2d1DeviceContext->DrawImage(pD2d1Effect, targetOffset, D2D1::RectF(sourceOffset.x, sourceOffset.y, s.width * fScale, s.height * fScale), D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, D2D1_COMPOSITE_MODE_SOURCE_OVER);
	m_pD2d1DeviceContext->EndDraw();

	return SUCCEEDED(hr);
}

void CD2ImageDrawer::display()
{
	if (m_pDxgiSwapChain1 != nullptr)
	{
		DXGI_PRESENT_PARAMETERS params{};
		m_pDxgiSwapChain1->Present1(1, 0, &params);
	}
}

void CD2ImageDrawer::onResize() noexcept
{
	checkBufferSize();
}

bool CD2ImageDrawer::checkBufferSize()
{
	if (m_pDxgiSwapChain1 != nullptr)
	{
		HWND hWnd = nullptr;
		m_pDxgiSwapChain1->GetHwnd(&hWnd);
		if (hWnd != nullptr)
		{
			RECT rc;
			::GetClientRect(hWnd, &rc);

			UINT clientWidth = rc.right - rc.left;
			UINT clientHeight = rc.bottom - rc.top;

			if (m_bufferWidth != clientWidth || m_bufferHeight != clientHeight)
			{
				return resizeBuffer(clientWidth, clientHeight);
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

bool CD2ImageDrawer::resizeBuffer(UINT width, UINT height)
{
	if (m_pDxgiSwapChain1 != nullptr && m_pD2d1DeviceContext != nullptr)
	{
		m_pD2d1DeviceContext->SetTarget(nullptr);

		HRESULT hr = m_pDxgiSwapChain1->ResizeBuffers(0, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		if (FAILED(hr))return false;
		
		m_bufferWidth = width;
		m_bufferHeight = height;

		CComPtr<IDXGISurface> pDxgiSurface;
		hr = m_pDxgiSwapChain1->GetBuffer(0, IID_PPV_ARGS(&pDxgiSurface));
		if (FAILED(hr))return false;

		CComPtr<ID2D1Bitmap1> pD2d1Bitmap1;
		hr = m_pD2d1DeviceContext->CreateBitmapFromDxgiSurface(pDxgiSurface, nullptr, &pD2d1Bitmap1);
		if (FAILED(hr))return false;

		m_pD2d1DeviceContext->SetTarget(pD2d1Bitmap1);

		return SUCCEEDED(hr);
	}

	return false;
}
#ifdef _DEBUG
void CD2ImageDrawer::releaseCpuImageBuffer()
{
	if (m_pCpuImageBuffer != nullptr)
	{
		m_pCpuImageBuffer->Release();
		m_pCpuImageBuffer = nullptr;
	}
}

bool CD2ImageDrawer::checkCpuImageBufferSize(UINT32 width, UINT32 height)
{
	if (m_pCpuImageBuffer == nullptr)
	{
		return createCouImageBuffer(width, height);
	}
	else
	{
		const D2D1_SIZE_U& bufferSize = m_pCpuImageBuffer->GetPixelSize();
		if (width > bufferSize.width && height > bufferSize.height)
		{
			return createCouImageBuffer(width, height);
		}
		else
		{
			return true;
		}
	}

	return false;
}

bool CD2ImageDrawer::createCouImageBuffer(UINT32 width, UINT32 height)
{
	releaseCpuImageBuffer();

	HRESULT hr = m_pD2d1DeviceContext->CreateBitmap(
		D2D1::SizeU(width, height),
		D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
		&m_pCpuImageBuffer);

	return SUCCEEDED(hr);
}
#endif /* _DEBUG */
