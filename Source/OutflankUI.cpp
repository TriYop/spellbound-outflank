#include "OutflankUI.h"

START_NAMESPACE_DISTRHO

OutflankUI::OutflankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
{
}

void OutflankUI::onNanoDisplay()
{
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(DGL_NAMESPACE::Color(0x14, 0x14, 0x20));
    fill();
    closePath();
}

UI* createUI()
{
    return new OutflankUI();
}

END_NAMESPACE_DISTRHO
