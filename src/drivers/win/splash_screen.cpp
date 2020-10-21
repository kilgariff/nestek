#include "splash_screen.h"
#include "stb/stb_image.h"

#include <windows.h>

static uint64_t get_time()
{
    LARGE_INTEGER Counter;
    LARGE_INTEGER Frequency;
    LARGE_INTEGER Time;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&Counter);
    Time.QuadPart = Counter.QuadPart;
    Time.QuadPart *= 1000000;
    Time.QuadPart /= Frequency.QuadPart;
    Time.QuadPart /= 1000;

    return static_cast<uint64_t>(Time.QuadPart);
}

void SplashScreen::LoadAndShow(uint64_t _show_for_ms)
{
    this->show_for_ms = _show_for_ms;
    image_data = stbi_load("splash.png", &width, &height, &components, 4);
    start_time_ms = get_time();
}

bool SplashScreen::IsShowing() const
{
    if (image_data == nullptr)
    {
        return false;
    }

    if (get_time() - start_time_ms >= show_for_ms)
    {
        return false;
    }

    return true;
}

void SplashScreen::Unload()
{
    if (image_data != nullptr)
    {
        stbi_image_free(image_data);
    }
}