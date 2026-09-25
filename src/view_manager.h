#ifndef VIEW_MANAGER_H_
#define VIEW_MANAGER_H_

#include <Windows.h>

#define LEFT_TOP_CORNER_SCALING 1

class CViewManager
{
public:
    CViewManager() = default;
    ~CViewManager() = default;

    void setBaseSize(HWND hRenderTargetWindow, unsigned int width, unsigned int height);
    unsigned int getBaseWidth() const noexcept;
    unsigned int getBaseHeight() const noexcept;

    void setScale(float fScale);
    float getScale() const noexcept;
    void rescale(bool upscale);

    void addOffset(int iX, int iY);
    float getOffsetX() const noexcept;
    float getOffsetY() const noexcept;

    void resetScale(float scale = 1.f);
    float getDefaultScale() const noexcept;

    void onStyleChanged();
private:
    enum Constants { kBaseWidth = 1280, kBaseHeight = 720 };

    HWND m_hRenderTargetWnd = nullptr;

    unsigned int m_baseWidth = Constants::kBaseWidth;
    unsigned int m_baseHeight = Constants::kBaseHeight;
    float m_fDefaultScale = 1.f;

    float m_fScale = 1.f;
    float m_fOffsetX = 0;
    float m_fOffsetY = 0;

    void workOutDefaultScale();
    void adjustOffset();

    void resizeWindow();
    void requestRedraw() const noexcept;
};

#endif // !VIEW_MANAGER_H_
