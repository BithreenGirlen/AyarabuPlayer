
#include <Windows.h>
#include <CommCtrl.h>


#include "main_window.h"
#include "win_filesystem.h"
#include "win_dialogue.h"
#include "win_text.h"
#include "win_image.h"
#include "media_setting_dialogue.h"
#include "ayarabu.h"


#pragma comment(lib, "Comctl32.lib")

CMainWindow::CMainWindow()
{

}

CMainWindow::~CMainWindow()
{

}

bool CMainWindow::Create(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    //wcex.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_APP));
    wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = ::GetSysColorBrush(COLOR_BTNFACE);
    //wcex.lpszMenuName = MAKEINTRESOURCEW(IDI_ICON_APP);
    wcex.lpszClassName = m_swzClassName;
    //wcex.hIconSm = ::LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON_APP));

    if (::RegisterClassExW(&wcex))
    {
        m_hInstance = hInstance;

        m_hWnd = ::CreateWindowW(m_swzClassName, m_wstrWindowName.c_str(), WS_OVERLAPPEDWINDOW & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
            CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, nullptr, nullptr, hInstance, this);
        if (m_hWnd != nullptr)
        {
            return true;
        }
        else
        {
            std::wstring wstrMessage = L"CreateWindowExW failed; code: " + std::to_wstring(::GetLastError());
            ::MessageBoxW(nullptr, wstrMessage.c_str(), L"Error", MB_ICONERROR);
        }
    }
    else
    {
        std::wstring wstrMessage = L"RegisterClassW failed; code: " + std::to_wstring(::GetLastError());
        ::MessageBoxW(nullptr, wstrMessage.c_str(), L"Error", MB_ICONERROR);
    }

    return false;
}

int CMainWindow::MessageLoop()
{
    MSG msg;

    for (;;)
    {
        BOOL bRet = ::GetMessageW(&msg, 0, 0, 0);
        if (bRet > 0)
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        else if (bRet == 0)
        {
            /*ループ終了*/
            return static_cast<int>(msg.wParam);
        }
        else
        {
            /*ループ異常*/
            std::wstring wstrMessage = L"GetMessageW failed; code: " + std::to_wstring(::GetLastError());
            ::MessageBoxW(nullptr, wstrMessage.c_str(), L"Error", MB_ICONERROR);
            return -1;
        }
    }
    return 0;
}
/*C CALLBACK*/
LRESULT CMainWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CMainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE)
    {
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = reinterpret_cast<CMainWindow*>(pCreateStruct->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }

    pThis = reinterpret_cast<CMainWindow*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (pThis != nullptr)
    {
        return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
    }

    return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
/*メッセージ処理*/
LRESULT CMainWindow::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        return OnCreate(hWnd);
    case WM_DESTROY:
        return OnDestroy();
    case WM_CLOSE:
        return OnClose();
    case WM_PAINT:
        return OnPaint();
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYUP:
        return OnKeyUp(wParam, lParam);
    case WM_COMMAND:
        return OnCommand(wParam, lParam);
    case WM_TIMER:
        return OnTimer(wParam);
    case WM_MOUSEWHEEL:
        return OnMouseWheel(wParam, lParam);
    case WM_LBUTTONDOWN:
        return OnLButtonDown(wParam, lParam);
    case WM_LBUTTONUP:
        return OnLButtonUp(wParam, lParam);
    case WM_MBUTTONUP:
        return OnMButtonUp(wParam, lParam);
    case EventMessage::kAudioPlayer:
        OnAudioPlayerEvent(static_cast<unsigned long>(lParam));
        break;
    case EventMessage::kVideoPlayer:
        OnVideoPlayerEvent(static_cast<unsigned long>(lParam));
        break;
    default:

        break;
    }

    return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
/*WM_CREATE*/
LRESULT CMainWindow::OnCreate(HWND hWnd)
{
    m_hWnd = hWnd;

    InitialiseMenuBar();

    m_videoTimer.SetCallback(std::bind(&CMainWindow::UpdateScreen, this));

    m_pD2ImageDrawer = new CD2ImageDrawer(m_hWnd);

    m_pAudioPlayer = new CMfMediaPlayer();
    m_pAudioPlayer->SetPlaybackWindow(m_hWnd, EventMessage::kAudioPlayer);

    m_pVideoTransferor = new CMfVideoTransferor();
    m_pVideoTransferor->SetPlaybackWindow(m_hWnd, EventMessage::kVideoPlayer);
    m_pVideoTransferor->SwitchLoop();

    m_pD2TextWriter = new CD2TextWriter(m_pD2ImageDrawer->GetD2Factory(), m_pD2ImageDrawer->GetD2DeviceContext());
    m_pD2TextWriter->SetupOutLinedDrawing(L"C:\\Windows\\Fonts\\yumindb.ttf");

    m_pViewManager = new CViewManager(m_hWnd);

    return 0;
}
/*WM_DESTROY*/
LRESULT CMainWindow::OnDestroy()
{
    ::PostQuitMessage(0);

    return 0;
}
/*WM_CLOSE*/
LRESULT CMainWindow::OnClose()
{
    m_videoTimer.End();

    ::KillTimer(m_hWnd, Timer::kText);

    if (m_pD2TextWriter != nullptr)
    {
        delete m_pD2TextWriter;
        m_pD2TextWriter = nullptr;
    }

    if (m_pD2ImageDrawer != nullptr)
    {
        delete m_pD2ImageDrawer;
        m_pD2ImageDrawer = nullptr;
    }

    if (m_pAudioPlayer != nullptr)
    {
        delete m_pAudioPlayer;
        m_pAudioPlayer = nullptr;
    }

    if (m_pVideoTransferor != nullptr)
    {
        delete m_pVideoTransferor;
        m_pVideoTransferor = nullptr;
    }

    ::DestroyWindow(m_hWnd);
    ::UnregisterClassW(m_swzClassName, m_hInstance);

    return 0;
}
/*WM_PAINT*/
LRESULT CMainWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = ::BeginPaint(m_hWnd, &ps);

    if (m_pD2ImageDrawer == nullptr || m_pVideoTransferor == nullptr || m_pD2TextWriter == nullptr 
        || m_pViewManager == nullptr || m_nPaintIndex >= m_paintData.size())
    {
        ::EndPaint(m_hWnd, &ps);
        return 0;
    }

    bool bRet = false;
    const adv::PaintDatum& paintDatum = m_paintData.at(m_nPaintIndex);
    if (paintDatum.bIsVideo)
    {
        CMfVideoTransferor::SVideoFrame sVideoFrame{};
        bRet = m_pVideoTransferor->TransferVideoFrame(&sVideoFrame);
        if (bRet && sVideoFrame.pPixels != nullptr)
        {
            if (m_pViewManager != nullptr)
            {
                ImageInfo sImageInfo;
                sImageInfo.uiWidth = sVideoFrame.iWidth;
                sImageInfo.uiHeight = sVideoFrame.iHeight;
                sImageInfo.iStride = sVideoFrame.uiStride;
                sImageInfo.pixels.resize(sVideoFrame.nPixelSize);
                memcpy(sImageInfo.pixels.data(), sVideoFrame.pPixels, sVideoFrame.nPixelSize);

                bRet = m_pD2ImageDrawer->Draw(sImageInfo, { static_cast<float>(m_pViewManager->GetXOffset()), static_cast<float>(m_pViewManager->GetYOffset()) }, m_pViewManager->GetScale());

                if (bRet)
                {
                    StoreVideoFrame(sVideoFrame.llCurrentTime, sImageInfo);
                }
            }
            free(sVideoFrame.pPixels);
        }
        else
        {
            long long llCurrentTime = m_pVideoTransferor->GetCurrentTimeInMilliSeconds();
            ImageInfo* s = RestoreVideoFrame(llCurrentTime);
            if (s != nullptr)
            {
                bRet = m_pD2ImageDrawer->Draw(*s, { m_pViewManager->GetXOffset(), m_pViewManager->GetYOffset() }, m_pViewManager->GetScale());
            }
        }
    }
    else
    {
        const auto& iter = m_imageMap.find(paintDatum.wstrFilePath);
        if (iter != m_imageMap.cend())
        {
            bRet = m_pD2ImageDrawer->Draw(iter->second, { m_pViewManager->GetXOffset(), m_pViewManager->GetYOffset() }, m_pViewManager->GetScale());
        }
    }

    if (bRet)
    {
        if (!m_bTextHidden && m_pD2TextWriter != nullptr)
        {
            const std::wstring wstr = FormatCurrentText();
            m_pD2TextWriter->OutLinedDraw(wstr.c_str(), static_cast<unsigned long>(wstr.size()));
        }
        m_pD2ImageDrawer->Display();
    }

    ::EndPaint(m_hWnd, &ps);

    return 0;
}
/*WM_SIZE*/
LRESULT CMainWindow::OnSize()
{

    return 0;
}
/*WM_KEYUP*/
LRESULT CMainWindow::OnKeyUp(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case VK_ESCAPE:
        ::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
        break;
    case VK_UP:
        MenuOnForeFile();
        break;
    case VK_DOWN:
        MenuOnNextFile();
        break;
    case 'C':
        if (m_pD2TextWriter != nullptr)
        {
            m_pD2TextWriter->SwitchTextColour();
            UpdateScreen();
        }
        break;
    case 'T':
        m_bTextHidden ^= true;
        UpdateScreen();
        break;
    }
    return 0;
}
/*WM_COMMAND*/
LRESULT CMainWindow::OnCommand(WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmKind = LOWORD(lParam);
    if (wmKind == 0)
    {
        /*Menus*/
        switch (wmId)
        {
        case Menu::kOpenFile:
            MenuOnOpenFile();
            break;
        case Menu::kNextFile:
            MenuOnNextFile();
            break;
        case Menu::kForeFile:
            MenuOnForeFile();
            break;
        case Menu::kAudioLoop:
            MenuOnAudioLoop();
            break;
        case Menu::kAudioSetting:
            MenuOnAudioSetting();
            break;
        case Menu::kVideoPause:
            MenuOnVideoPause();
            break;
        case Menu::kVideoSetting:
            MenuOnVideoSetting();
            break;
        default:

            break;
        }
    }
    else
    {
        /*Controls*/
    }

    return 0;
}
/*WM_TIMER*/
LRESULT CMainWindow::OnTimer(WPARAM wParam)
{
    switch (wParam)
    {
    case Timer::kText:
        if (m_pAudioPlayer != nullptr)
        {
            if (m_pAudioPlayer->IsEnded())
            {
                AutoTexting();
            }
        }
        break;
    default:
        break;
    }
    return 0;
}
/*WM_MOUSEWHEEL*/
LRESULT CMainWindow::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    int iScroll = -static_cast<short>(HIWORD(wParam)) / WHEEL_DELTA;
    WORD usKey = LOWORD(wParam);

    if (usKey == 0)
    {
        if (m_pViewManager != nullptr)
        {
            m_pViewManager->Rescale(iScroll > 0);
        }
    }

    if (usKey == MK_LBUTTON)
    {

    }

    if (usKey == MK_RBUTTON)
    {
        ShiftText(iScroll > 0);
    }

    return 0;
}
/*WM_LBUTTONDOWN*/
LRESULT CMainWindow::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    ::GetCursorPos(&m_CursorPos);

    /*When menu item is selected, WM_LBUTTONDOWN does not happen, but does WM_LBUTTONUP.*/
    m_bLeftDowned = true;

    return 0;
}
/*WM_LBUTTONUP*/
LRESULT CMainWindow::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    WORD usKey = LOWORD(wParam);

    if (usKey == MK_RBUTTON && m_bBarHidden)
    {
        ::PostMessage(m_hWnd, WM_SYSCOMMAND, SC_MOVE, 0);
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_DOWN;
        ::SendInput(1, &input, sizeof(input));
    }

    if (usKey == 0 && m_bLeftDowned)
    {
        POINT pt{};
        ::GetCursorPos(&pt);
        int iX = m_CursorPos.x - pt.x;
        int iY = m_CursorPos.y - pt.y;

        if (iX == 0 && iY == 0)
        {
            ShiftPaintData(true);
        }
        {
            if (m_pViewManager != nullptr)
            {
                m_pViewManager->SetOffset(iX, iY);
            }
        }
    }

    m_bLeftDowned = false;

    return 0;
}
/*WM_MBUTTONUP*/
LRESULT CMainWindow::OnMButtonUp(WPARAM wParam, LPARAM lParam)
{
    WORD usKey = LOWORD(wParam);
    if (usKey == 0)
    {
        if (m_pViewManager != nullptr)
        {
            m_pViewManager->ResetZoom();
        }
    }

    if (usKey == MK_RBUTTON)
    {
        SwitchWindowMode();
    }

    return 0;
}
/*操作欄作成*/
void CMainWindow::InitialiseMenuBar()
{
    HMENU hMenuFile = nullptr;
    HMENU hMenuAudio = nullptr;
    HMENU hMenuVideo = nullptr;
    HMENU hMenuBar = nullptr;
    BOOL iRet = FALSE;

    if (m_hMenuBar != nullptr)return;

    /*ファイル*/
    hMenuFile = ::CreateMenu();
    if (hMenuFile == nullptr)goto failed;
    iRet = ::AppendMenuA(hMenuFile, MF_STRING, Menu::kOpenFile, "Open");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuFile, MF_STRING, Menu::kNextFile, "Next");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuFile, MF_STRING, Menu::kForeFile, "Back");
    if (iRet == 0)goto failed;

    /*音声*/
    hMenuAudio = ::CreateMenu();
    if (hMenuAudio == nullptr)goto failed;

    iRet = ::AppendMenuA(hMenuAudio, MF_STRING, Menu::kAudioLoop, "Loop");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuAudio, MF_STRING, Menu::kAudioSetting, "Setting");
    if (iRet == 0)goto failed;

    /*動画*/
    hMenuVideo = ::CreateMenu();
    if (hMenuVideo == nullptr)goto failed;

    iRet = ::AppendMenuA(hMenuVideo, MF_STRING, Menu::kVideoPause, "Pause");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuVideo, MF_STRING, Menu::kVideoSetting, "Setting");
    if (iRet == 0)goto failed;

    /*分類*/
    hMenuBar = ::CreateMenu();
    if (hMenuBar == nullptr) goto failed;
    iRet = ::AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hMenuFile), "File");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hMenuAudio), "Audio");
    if (iRet == 0)goto failed;
    iRet = ::AppendMenuA(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hMenuVideo), "Video");
    if (iRet == 0)goto failed;

    iRet = ::SetMenu(m_hWnd, hMenuBar);
    if (iRet == 0)goto failed;

    m_hMenuBar = hMenuBar;

    /*正常終了*/
    return;

failed:
    std::wstring wstrMessage = L"Failed to create menu; code: " + std::to_wstring(::GetLastError());
    ::MessageBoxW(nullptr, wstrMessage.c_str(), L"Error", MB_ICONERROR);
    /*SetMenu成功後はウィンドウ破棄時に破棄されるが、今は紐づけ前なのでここで破棄する。*/
    if (hMenuFile != nullptr)
    {
        ::DestroyMenu(hMenuFile);
    }
    if (hMenuAudio != nullptr)
    {
        ::DestroyMenu(hMenuAudio);
    }
    if (hMenuVideo != nullptr)
    {
        ::DestroyMenu(hMenuVideo);
    }
    if (hMenuBar != nullptr)
    {
        ::DestroyMenu(hMenuBar);
    }

}
/*ファイル選択*/
void CMainWindow::MenuOnOpenFile()
{
    std::wstring wstrPickedFile = win_dialogue::SelectOpenFile(L"script file", L"*3.evsc*", nullptr, m_hWnd);
    if (!wstrPickedFile.empty())
    {
        bool bRet = SetupScenario(wstrPickedFile.c_str());
        if (bRet)
        {
            m_scriptFilePaths.clear();
            m_nScriptFilePathIndex = 0;
            win_filesystem::GetFilePathListAndIndex(wstrPickedFile, L"*3.evsc*", m_scriptFilePaths, &m_nScriptFilePathIndex);
        }
    }
}
/*次ファイルに移動*/
void CMainWindow::MenuOnNextFile()
{
    if (m_scriptFilePaths.empty())return;

    ++m_nScriptFilePathIndex;
    if (m_nScriptFilePathIndex >= m_scriptFilePaths.size())m_nScriptFilePathIndex = 0;
    SetupScenario(m_scriptFilePaths.at(m_nScriptFilePathIndex).c_str());
}
/*前ファイルに移動*/
void CMainWindow::MenuOnForeFile()
{
    if (m_scriptFilePaths.empty())return;

    --m_nScriptFilePathIndex;
    if (m_nScriptFilePathIndex >= m_scriptFilePaths.size())m_nScriptFilePathIndex = m_scriptFilePaths.size() - 1;
    SetupScenario(m_scriptFilePaths.at(m_nScriptFilePathIndex).c_str());
}
/*音声ループ設定変更*/
void CMainWindow::MenuOnAudioLoop()
{
    if (m_pAudioPlayer != nullptr)
    {
        HMENU hMenuBar = ::GetMenu(m_hWnd);
        if (hMenuBar != nullptr)
        {
            HMENU hMenu = ::GetSubMenu(hMenuBar, MenuBar::kAudio);
            if (hMenu != nullptr)
            {
                BOOL iRet = m_pAudioPlayer->SwitchLoop();
                ::CheckMenuItem(hMenu, Menu::kAudioLoop, iRet == TRUE ? MF_CHECKED : MF_UNCHECKED);
            }
        }
    }
}
/*音声設定画面呼び出し*/
void CMainWindow::MenuOnAudioSetting()
{
    if (m_pAudioPlayer != nullptr)
    {
        CMediaSettingDialogue* pMediaSettingDialogue = new CMediaSettingDialogue();
        if (pMediaSettingDialogue != nullptr)
        {
            pMediaSettingDialogue->Open(m_hInstance, m_hWnd, m_pAudioPlayer, L"Audio");

            delete pMediaSettingDialogue;
        }
    }
}
/*動画一時停止*/
void CMainWindow::MenuOnVideoPause()
{
    if (m_pVideoTransferor != nullptr)
    {
        HMENU hMenuBar = ::GetMenu(m_hWnd);
        if (hMenuBar != nullptr)
        {
            HMENU hMenu = ::GetSubMenu(hMenuBar, MenuBar::kVideo);
            if (hMenu != nullptr)
            {
                BOOL iRet = m_pVideoTransferor->SwitchPause();
                ::CheckMenuItem(hMenu, Menu::kVideoPause, iRet == TRUE ? MF_CHECKED : MF_UNCHECKED);
            }
        }
    }
}
/*動画設定画面呼び出し*/
void CMainWindow::MenuOnVideoSetting()
{
    if (m_pVideoTransferor != nullptr)
    {
        CMediaSettingDialogue* pMediaSettingDialogue = new CMediaSettingDialogue();
        if (pMediaSettingDialogue != nullptr)
        {
            pMediaSettingDialogue->Open(m_hInstance, m_hWnd, m_pVideoTransferor, L"Video");

            delete pMediaSettingDialogue;
        }
    }
}
/*標題変更*/
void CMainWindow::ChangeWindowTitle(const wchar_t* pzTitle)
{
    std::wstring wstr;
    if (pzTitle != nullptr)
    {
        std::wstring wstrTitle = pzTitle;
        size_t pos = wstrTitle.find_last_of(L"\\/");
        wstr = pos == std::wstring::npos ? wstrTitle : wstrTitle.substr(pos + 1);
    }

    ::SetWindowTextW(m_hWnd, wstr.empty() ? m_wstrWindowName.c_str() : wstr.c_str());
}
/*表示形式変更*/
void CMainWindow::SwitchWindowMode()
{
    if (!m_bPlayReady)return;

    LONG lStyle = ::GetWindowLong(m_hWnd, GWL_STYLE);

    m_bBarHidden ^= true;

    if (m_bBarHidden)
    {
        RECT rect;
        ::GetWindowRect(m_hWnd, &rect);

        ::SetWindowLong(m_hWnd, GWL_STYLE, lStyle & ~WS_CAPTION & ~WS_SYSMENU);
        ::SetWindowPos(m_hWnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
        ::SetMenu(m_hWnd, nullptr);
    }
    else
    {
        ::SetWindowLong(m_hWnd, GWL_STYLE, lStyle | WS_CAPTION | WS_SYSMENU);
        ::SetMenu(m_hWnd, m_hMenuBar);
    }

    if (m_pViewManager != nullptr)
    {
        m_pViewManager->OnStyleChanged();
    }
}
/*寸劇構築*/
bool CMainWindow::SetupScenario(const wchar_t* pwzFilePath)
{
    if (pwzFilePath == nullptr)return false;

    m_videoTimer.End();

    ClearScenarioInfo();
    ClearStoeredVideoFrame();

    bool bRet = ayarabu::LoadScenario(pwzFilePath, m_textData, m_paintData);
    if (!bRet)return false;

    CreateImageMap();
    UpdateText();
    UpdatePaintData();

    m_bPlayReady = bRet;

    ChangeWindowTitle(m_bPlayReady ? pwzFilePath : nullptr);

    return bRet;
}
/*寸劇情報消去*/
void CMainWindow::ClearScenarioInfo()
{
    m_textData.clear();
    m_nTextIndex = 0;

    m_paintData.clear();
    m_nPaintIndex = 0;

    ClearImageMap();
    ClearStoeredVideoFrame();

    m_bFirstPaintLoaded = false;
}
/*再描画要求*/
void CMainWindow::UpdateScreen()
{
    ::InvalidateRect(m_hWnd, nullptr, FALSE);
}
/*表示図画送り・戻し*/
void CMainWindow::ShiftPaintData(bool bForward)
{
    if (bForward)
    {
        ++m_nPaintIndex;
        if (m_nPaintIndex >= m_paintData.size())m_nPaintIndex = 0;
    }
    else
    {
        --m_nPaintIndex;
        if (m_nPaintIndex >= m_paintData.size())m_nPaintIndex = m_paintData.size() - 1;
    }

    UpdatePaintData();
}
/*図画データ更新*/
void CMainWindow::UpdatePaintData()
{
    if (m_nPaintIndex >= m_paintData.size())return;

    const adv::PaintDatum& paintDatum = m_paintData.at(m_nPaintIndex);
    if (paintDatum.bIsVideo)
    {
        ClearStoeredVideoFrame();

        if (m_pVideoTransferor != nullptr)
        {
            m_pVideoTransferor->Play(paintDatum.wstrFilePath.c_str());

            m_videoTimer.Start();
        }
    }
    else
    {
        m_videoTimer.End();
        if (!m_bFirstPaintLoaded)
        {
            const auto& iter = m_imageMap.find(paintDatum.wstrFilePath);
            if (iter != m_imageMap.cend())
            {
                m_pViewManager->SetBaseSize(iter->second.uiWidth, iter->second.uiHeight);
                m_pViewManager->ResetZoom();
                m_bFirstPaintLoaded = true;
            }
        }
        UpdateScreen();
    }
}
/*文章送り・戻し*/
void CMainWindow::ShiftText(bool bForward)
{
    if (bForward)
    {
        ++m_nTextIndex;
        if (m_nTextIndex >= m_textData.size())m_nTextIndex = 0;
    }
    else
    {
        --m_nTextIndex;
        if (m_nTextIndex >= m_textData.size())m_nTextIndex = m_textData.size() - 1;
    }
    UpdateText();
}
/*文章更新*/
void CMainWindow::UpdateText()
{
    if (m_nTextIndex < m_textData.size())
    {
        const adv::TextDatum& t = m_textData.at(m_nTextIndex);
        if (!t.wstrVoicePath.empty())
        {
            if (m_pAudioPlayer != nullptr)
            {
                m_pAudioPlayer->Play(t.wstrVoicePath.c_str());
            }
        }
        constexpr unsigned int kTimerInterval = 2000;
        ::SetTimer(m_hWnd, Timer::kText, kTimerInterval, nullptr);
    }

    UpdateScreen();
}
/*自動送り*/
void CMainWindow::AutoTexting()
{
    if (m_nTextIndex < m_textData.size() - 1)ShiftText(true);
}
/*表示文作成*/
std::wstring CMainWindow::FormatCurrentText()
{
    if (m_nTextIndex > m_textData.size() - 1)return std::wstring();

    const adv::TextDatum& t = m_textData.at(m_nTextIndex);
    std::wstring wstr = t.wstrText;
    if (!wstr.empty() && wstr.back() != L'\n')wstr.push_back(L'\n');
    wstr += std::to_wstring(m_nTextIndex + 1) + L"/" + std::to_wstring(m_textData.size());
    return wstr;
}

/*転送動画溜め置き*/
void CMainWindow::StoreVideoFrame(long long llCurrentTime, const ImageInfo& imageInfo)
{
    constexpr int kMaxBufferMilliSeconds = 200;
    if (llCurrentTime < kMaxBufferMilliSeconds)
    {
        m_storedVideoFrames.insert({ llCurrentTime, imageInfo });
    }
}
/*溜め置き動画消去*/
void CMainWindow::ClearStoeredVideoFrame()
{
    m_storedVideoFrames.clear();
}
/*溜め置き転送動画取り出し*/
ImageInfo* CMainWindow::RestoreVideoFrame(long long llCurrentTime)
{
    const auto iter = m_storedVideoFrames.find(llCurrentTime);
    if (iter != m_storedVideoFrames.cend())
    {
        return &iter->second;
    }
    return nullptr;
}
/*静画メモリ取り込み*/
void CMainWindow::CreateImageMap()
{
    for (const auto& paintData : m_paintData)
    {
        if (!paintData.bIsVideo)
        {
            const auto& iter = m_imageMap.find(paintData.wstrFilePath);
            if (iter == m_imageMap.cend())
            {
                ImageInfo s{};
                bool bRet = win_image::LoadImageToMemory(paintData.wstrFilePath.c_str(), &s, 1.f);
                if (bRet)
                {
                    m_imageMap.insert({ paintData.wstrFilePath, s });
                }
            }
        }
    }
}
/*静画メモリ消去*/
void CMainWindow::ClearImageMap()
{
    m_imageMap.clear();
}
/*IMFMediaEngineNotify::EventNotify*/
void CMainWindow::OnAudioPlayerEvent(unsigned long ulEvent)
{
    switch (ulEvent)
    {
    case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:

        break;
    case MF_MEDIA_ENGINE_EVENT_ENDED:
        AutoTexting();
        break;
    default:
        break;
    }
}

void CMainWindow::OnVideoPlayerEvent(unsigned long ulEvent)
{
    switch (ulEvent)
    {
    case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
        if (m_pVideoTransferor != nullptr)
        {
            if (!m_bFirstPaintLoaded)
            {
                unsigned long uiWidth = 0;
                unsigned long uiHeight = 0;
                bool bRet = m_pVideoTransferor->GetVideoSize(&uiWidth, &uiHeight);
                if (bRet)
                {
                    if (m_pViewManager != nullptr)
                    {
                        m_pViewManager->SetBaseSize(uiWidth, uiHeight);
                        m_pViewManager->ResetZoom();
                    }
                    m_bFirstPaintLoaded = true;
                }
            }
        }
        break;
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:

        break;
    case MF_MEDIA_ENGINE_EVENT_ENDED:

        break;
    default:
        break;
    }
}