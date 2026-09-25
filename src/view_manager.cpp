

#include "view_manager.h"


void CViewManager::setBaseSize(HWND hRenderTargetWindow, unsigned int width, unsigned int height)
{
	m_hRenderTargetWnd = hRenderTargetWindow;
	m_baseWidth = width;
	m_baseHeight = height;

	workOutDefaultScale();
}

unsigned int CViewManager::getBaseWidth() const noexcept
{
	return m_baseWidth;
}

unsigned int CViewManager::getBaseHeight() const noexcept
{
	return m_baseHeight;
}

void CViewManager::setScale(float fScale)
{
	m_fScale = fScale;
}

float CViewManager::getScale() const noexcept
{
	return m_fScale;
}

void CViewManager::rescale(bool upscale)
{
	static constexpr float fScaleMin = 0.25f;
	static constexpr float fScalePortion = 0.05f;
	if (upscale)
	{
		m_fScale += fScalePortion;
	}
	else
	{
		m_fScale -= fScalePortion;
		if (m_fScale < fScaleMin) m_fScale = fScaleMin;
	}

	resizeWindow();
}

void CViewManager::addOffset(int iX, int iY)
{
#if defined(LEFT_TOP_CORNER_SCALING)
	m_fOffsetX += iX / m_fScale;
	m_fOffsetY += iY / m_fScale;
#else /* Centre scaling */
	m_fOffsetX += iX * m_fScale;
	m_fOffsetY += iY * m_fScale;
#endif

	adjustOffset();
	requestRedraw();
}

float CViewManager::getOffsetX() const noexcept
{
	return m_fOffsetX;
}

float CViewManager::getOffsetY() const noexcept
{
	return m_fOffsetY;
}

/* 原寸表示 */
void CViewManager::resetScale(float scale)
{
	m_fScale = scale;
	m_fOffsetX = 0;
	m_fOffsetY = 0;

	resizeWindow();
}

float CViewManager::getDefaultScale() const noexcept
{
	return m_fDefaultScale;
}
/* 表示形式変更通知 */
void CViewManager::onStyleChanged()
{
	resizeWindow();
}
/* 基準尺度算出 */
void CViewManager::workOutDefaultScale()
{
	/* 基準寸法がモニタ解像度より大きい場合には予め縮小する */

	unsigned int monitorWidth = UINT_MAX;
	unsigned int monitorHeight = UINT_MAX;
	HMONITOR hMonitor = ::MonitorFromWindow(m_hRenderTargetWnd, MONITOR_DEFAULTTONEAREST);
	if (hMonitor != nullptr)
	{
		MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
		BOOL iRet = ::GetMonitorInfoW(hMonitor, &monitorInfo);
		if (iRet)
		{
			monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
			monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
		}
	}
	if (m_baseWidth > monitorWidth || m_baseHeight > monitorHeight)
	{
		float scaleX = m_fDefaultScale = static_cast<float>(monitorWidth) / m_baseWidth;
		float scaleY = m_fDefaultScale = static_cast<float>(monitorHeight) / m_baseHeight;

		m_fDefaultScale = monitorWidth > monitorHeight ? scaleY : scaleX;
	}
	else
	{
		m_fDefaultScale = ::GetDpiForWindow(m_hRenderTargetWnd) / 96.f;
	}

	m_fScale = m_fDefaultScale;
}
void CViewManager::adjustOffset()
{
	if (m_hRenderTargetWnd != nullptr)
	{
		const int scaledWidth = static_cast<int>(m_baseWidth * m_fScale);
		const int scaledHeight = static_cast<int>(m_baseHeight * m_fScale);

		RECT rc;
		::GetClientRect(m_hRenderTargetWnd, &rc);

		const int targetWidth = rc.right - rc.left;
		const int targetHeight = rc.bottom - rc.top;

#if defined(LEFT_TOP_CORNER_SCALING)
		const int maxOffsetX = scaledWidth > targetWidth ? static_cast<int>((scaledWidth - targetWidth) / m_fScale) : 0;
		const int maxOffsetY = scaledHeight > targetHeight ? static_cast<int>((scaledHeight - targetHeight) / m_fScale) : 0;

		if (m_fOffsetX < 0) m_fOffsetX = 0;
		if (m_fOffsetY < 0) m_fOffsetY = 0;

		if (m_fOffsetX > maxOffsetX)m_fOffsetX = static_cast<float>(maxOffsetX);
		if (m_fOffsetY > maxOffsetY)m_fOffsetY = static_cast<float>(maxOffsetY);
#else /* Centre scaling */
		const float fMaxOffsetX = static_cast<float>(scaledWidth - targetWidth);
		const float fMaxOffsetY = static_cast<float>(scaledHeight - targetHeight);

		m_fOffsetX = (m_fOffsetX < -fMaxOffsetX ? -fMaxOffsetX : m_fOffsetX);
		m_fOffsetY = (m_fOffsetY < -fMaxOffsetY ? -fMaxOffsetY : m_fOffsetY);

		m_fOffsetX = (m_fOffsetX > fMaxOffsetX ? fMaxOffsetX : m_fOffsetX);
		m_fOffsetY = (m_fOffsetY > fMaxOffsetY ? fMaxOffsetY : m_fOffsetY);
#endif
	}
}
/* 窓寸法調整 */
void CViewManager::resizeWindow()
{
	if (m_hRenderTargetWnd != nullptr)
	{
		RECT rect;
		::GetWindowRect(m_hRenderTargetWnd, &rect);
		int windowWidth = static_cast<int>(m_baseWidth * m_fScale);
		int windowHeight = static_cast<int>(m_baseHeight * m_fScale);

		int monitorWidth = INT_MAX;
		int monitorHeight = INT_MAX;
		HMONITOR hMonitor = ::MonitorFromWindow(m_hRenderTargetWnd, MONITOR_DEFAULTTONEAREST);
		if (hMonitor != nullptr)
		{
			MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
			BOOL iRet = ::GetMonitorInfoW(hMonitor, &monitorInfo);
			if (iRet)
			{
				monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
				monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
			}
		}

		windowWidth = windowWidth > monitorWidth ? monitorWidth : windowWidth;
		windowHeight = windowHeight > monitorHeight ? monitorHeight : windowHeight;

		rect.right = windowWidth + rect.left;
		rect.bottom = windowHeight + rect.top;

		LONG lStyle = ::GetWindowLong(m_hRenderTargetWnd, GWL_STYLE);
		const auto IsWidowBarHidden = [&lStyle]()
			-> bool
			{
				return !((lStyle & WS_CAPTION) && (lStyle & WS_SYSMENU));
			};
		::AdjustWindowRect(&rect, lStyle, IsWidowBarHidden() ? FALSE : TRUE);
		::SetWindowPos(m_hRenderTargetWnd, HWND_TOP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
	}

	adjustOffset();
	requestRedraw();
}
/* 再描画要求 */
void CViewManager::requestRedraw() const noexcept
{
	if (m_hRenderTargetWnd != nullptr)
	{
		::InvalidateRect(m_hRenderTargetWnd, nullptr, FALSE);
	}
}