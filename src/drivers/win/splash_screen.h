#pragma once

#include <vector>

class SplashScreen
{
public:

    ~SplashScreen()
    {
        Unload();
    }

    void LoadAndShow(uint64_t _show_for_ms);
    void Unload();

    uint8_t const * GetBytes() const
    {
        return image_data;
    }

    size_t GetByteCount() const
    {
        return width * height * components;
    }

    bool IsShowing() const;

    void Close()
    {
        Unload();
    }

private:

    bool closed = false;
    int width = 0;
    int height = 0;
    int components = 0;
    
    uint8_t * image_data = nullptr;
    uint64_t start_time_ms = 0;
    uint64_t show_for_ms = 0;
};