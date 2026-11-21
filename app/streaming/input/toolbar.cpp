#include "streaming/session.h"
#include "input.h"

namespace {

const int k_ButtonWidth = 72;
const int k_ButtonHeight = 48;
const int k_Gap = 8;
const int k_Padding = 8;
const int k_IconStroke = 3;
const int k_RevealEdge = 2;
const int k_HideMargin = 48;

int toolbarWidth()
{
    return k_Padding * 2 + k_ButtonWidth * 3 + k_Gap * 2;
}

int toolbarHeight()
{
    return k_Padding * 2 + k_ButtonHeight;
}

void fill(SDL_Surface* surface, int x, int y, int w, int h, Uint32 color)
{
    SDL_Rect rect = { x, y, w, h };
    SDL_FillRect(surface, &rect, color);
}

void drawMinimizeIcon(SDL_Surface* surface, const SDL_Rect& button, Uint32 color)
{
    int w = 22;
    fill(surface, button.x + (button.w - w) / 2, button.y + button.h / 2 + 6, w, k_IconStroke, color);
}

void drawFullScreenIcon(SDL_Surface* surface, const SDL_Rect& button, Uint32 color)
{
    int size = 18;
    int x = button.x + (button.w - size) / 2 + 3;
    int y = button.y + (button.h - size) / 2 - 3;
    fill(surface, x, y, size, k_IconStroke, color);
    fill(surface, x, y + size - k_IconStroke, size, k_IconStroke, color);
    fill(surface, x, y, k_IconStroke, size, color);
    fill(surface, x + size - k_IconStroke, y, k_IconStroke, size, color);
    x -= 6;
    y += 6;
    fill(surface, x, y, size, k_IconStroke, color);
    fill(surface, x, y + size - k_IconStroke, size, k_IconStroke, color);
    fill(surface, x, y, k_IconStroke, size, color);
    fill(surface, x + size - k_IconStroke, y, k_IconStroke, size, color);
}

void drawCloseIcon(SDL_Surface* surface, const SDL_Rect& button, Uint32 color)
{
    int size = 20;
    int x = button.x + (button.w - size) / 2;
    int y = button.y + (button.h - size) / 2;
    for (int i = 0; i < size; i++) {
        fill(surface, x + i, y + i, k_IconStroke, k_IconStroke, color);
        fill(surface, x + size - 1 - i, y + i, k_IconStroke, k_IconStroke, color);
    }
}

SDL_Rect buttonRect(int index)
{
    SDL_Rect rect;
    rect.x = k_Padding + index * (k_ButtonWidth + k_Gap);
    rect.y = k_Padding;
    rect.w = k_ButtonWidth;
    rect.h = k_ButtonHeight;
    return rect;
}

SDL_Surface* renderToolbar(int hover)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, toolbarWidth(), toolbarHeight(), 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        return nullptr;
    }

    Uint32 background = SDL_MapRGBA(surface->format, 0x20, 0x20, 0x24, 0xE0);
    Uint32 button = SDL_MapRGBA(surface->format, 0x3A, 0x3A, 0x40, 0xFF);
    Uint32 buttonHover = SDL_MapRGBA(surface->format, 0x5A, 0x5A, 0x64, 0xFF);
    Uint32 closeHover = SDL_MapRGBA(surface->format, 0xC4, 0x2B, 0x1C, 0xFF);
    Uint32 icon = SDL_MapRGBA(surface->format, 0xF0, 0xF0, 0xF0, 0xFF);

    SDL_FillRect(surface, nullptr, background);

    for (int i = 0; i < 3; i++) {
        SDL_Rect rect = buttonRect(i);
        Uint32 color = button;
        if (i == hover) {
            color = (i == 2) ? closeHover : buttonHover;
        }
        SDL_FillRect(surface, &rect, color);
    }

    drawMinimizeIcon(surface, buttonRect(0), icon);
    drawFullScreenIcon(surface, buttonRect(1), icon);
    drawCloseIcon(surface, buttonRect(2), icon);

    return surface;
}

}

float SdlInputHandler::toolbarPixelScale()
{
    int windowWidth, windowHeight, pixelWidth, pixelHeight;
    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(m_Window, &pixelWidth, &pixelHeight);
#else
    pixelWidth = windowWidth;
    pixelHeight = windowHeight;
#endif
    if (windowWidth <= 0) {
        return 1.0f;
    }
    return (float)pixelWidth / windowWidth;
}

int SdlInputHandler::toolbarButtonAt(int x, int y)
{
    int windowWidth, windowHeight;
    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);

    float scale = toolbarPixelScale();
    int px = (int)(x * scale);
    int py = (int)(y * scale);
    int left = ((int)(windowWidth * scale) - toolbarWidth()) / 2;

    for (int i = 0; i < ToolbarButtonCount; i++) {
        SDL_Rect rect = buttonRect(i);
        rect.x += left;
        SDL_Point point = { px, py };
        if (SDL_PointInRect(&point, &rect)) {
            return i;
        }
    }
    return ToolbarNone;
}

void SdlInputHandler::setToolbarState(bool visible, int hover)
{
    if (visible == m_ToolbarVisible && hover == m_ToolbarHover) {
        return;
    }

    bool visibilityChanged = visible != m_ToolbarVisible;
    m_ToolbarVisible = visible;
    m_ToolbarHover = visible ? hover : ToolbarNone;
    if (!visible) {
        m_ToolbarPressed = ToolbarNone;
    }

    if (visibilityChanged) {
        SDL_ShowCursor(visible || !isCaptureActive() ? SDL_ENABLE : m_MouseCursorCapturedVisibilityState);
    }

    Session::get()->getOverlayManager().setOverlaySurface(Overlay::OverlayToolbar,
                                                          visible ? renderToolbar(m_ToolbarHover) : nullptr);
}

bool SdlInputHandler::handleToolbarMotion(int x, int y)
{
    bool fullScreen = (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!fullScreen || SDL_GetRelativeMouseMode()) {
        setToolbarState(false, ToolbarNone);
        return false;
    }

    int windowWidth, windowHeight;
    SDL_GetWindowSize(m_Window, &windowWidth, &windowHeight);
    float scale = toolbarPixelScale();
    int halfWidth = (int)(toolbarWidth() / scale / 2);
    int barHeight = (int)(toolbarHeight() / scale);
    bool nearBar = x >= windowWidth / 2 - halfWidth - k_HideMargin &&
                   x <= windowWidth / 2 + halfWidth + k_HideMargin &&
                   y <= barHeight + k_HideMargin;

    if (y <= k_RevealEdge || (m_ToolbarVisible && nearBar)) {
        setToolbarState(true, toolbarButtonAt(x, y));
    }
    else {
        setToolbarState(false, ToolbarNone);
    }

    return m_ToolbarVisible && y <= barHeight;
}

bool SdlInputHandler::handleToolbarButton(SDL_MouseButtonEvent* event)
{
    if (!m_ToolbarVisible || event->button != SDL_BUTTON_LEFT) {
        return false;
    }

    int button = toolbarButtonAt(event->x, event->y);

    if (event->state == SDL_PRESSED) {
        if (button == ToolbarNone) {
            return false;
        }
        m_ToolbarPressed = button;
        return true;
    }

    if (m_ToolbarPressed == ToolbarNone) {
        return false;
    }

    int pressed = m_ToolbarPressed;
    m_ToolbarPressed = ToolbarNone;

    if (button != pressed) {
        return true;
    }

    setToolbarState(false, ToolbarNone);

    switch (pressed) {
    case ToolbarMinimize:
        performSpecialKeyCombo(KeyComboToggleMinimize);
        break;
    case ToolbarFullScreen:
        performSpecialKeyCombo(KeyComboToggleFullScreen);
        break;
    case ToolbarClose:
        performSpecialKeyCombo(KeyComboQuit);
        break;
    }

    return true;
}
