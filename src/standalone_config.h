#pragma once

#include <string>
#include <array>
#include <vector>
#include <limits>

enum NESButton
{
    ButtonA = 0,
    ButtonB,
    ButtonSelect,
    ButtonStart,
    DirectionUp,
    DirectionDown,
    DirectionLeft,
    DirectionRight,
    COUNT
};

struct ButtonMapping
{
    static uint32_t const sentinel = std::numeric_limits<uint32_t>::max();
    std::array<uint32_t, NESButton::COUNT> keyboard_devices;
    std::array<uint32_t, NESButton::COUNT> keyboard_keys;
    std::array<uint32_t, NESButton::COUNT> gamepad_buttons;
    std::array<uint32_t, NESButton::COUNT> gamepad_devices;
};

class StandaloneConfig
{
public:

    // Settings.
    std::string game_name = "FCEUX_Standalone";
    bool enable_separate_user_config = true;
    bool start_fullscreen = true;
    bool show_splash_screen = true;
    uint64_t splash_screen_timeout_ms = 2000;

    bool use_hq2x = false;
    bool disable_spritelimit = false;
    bool stretch_to_screen = false;

    std::vector<ButtonMapping> button_mappings;

    // Loading and saving.
    void LoadFromFile(std::string const file_path);
    void SaveToFile(std::string const file_path);
};

void LoadDefaultConfig(StandaloneConfig & config);
void LoadUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config);
void SaveUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config);
bool UserConfigExists(StandaloneConfig const & default_config);