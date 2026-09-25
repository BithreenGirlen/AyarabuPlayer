#ifndef MEDIA_SETTING_DIALOGUE_H_
#define MEDIA_SETTING_DIALOGUE_H_

#include <Windows.h>

class CMediaSettingDialogue
{
public:
	CMediaSettingDialogue();
	~CMediaSettingDialogue();

	bool open(HINSTANCE hInstance, HWND hWnd, void* pMediaPlayer, const wchar_t* pwzWindowName);
	int messageLoop();

	HWND getHwnd()const noexcept { return m_hWnd; }
private:
	const wchar_t* m_swzClassName = L"Media player setting dialogue";
	HINSTANCE m_hInstance = nullptr;
	HWND m_hWnd = nullptr;

	void* m_pMediaPlayer = nullptr;

	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT onCreate(HWND hWnd);
	LRESULT onDestroy();
	LRESULT onClose();
	LRESULT onPaint();
	LRESULT onSize();
	LRESULT onVScroll(WPARAM wParam, LPARAM lParam);
	LRESULT onCommand(WPARAM wParam, LPARAM lParam);

	enum Constants { kFontSize = 16, kTextWidth = 70};
	enum Controls{kVolumeSlider = 1, kRateSkuder};
	HFONT m_hFont = nullptr;
	HWND m_hVolumeSlider = nullptr;
	HWND m_hVolumeText = nullptr;
	HWND m_hRateSlider = nullptr;
	HWND m_hRateText = nullptr;

	void createSliders();
	void setSliderPosition();
	static BOOL CALLBACK SetFontCallback(HWND hWnd, LPARAM lParam);
};

#endif //MEDIA_SETTING_DIALOGUE_H_
