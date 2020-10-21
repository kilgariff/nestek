#pragma once

#include <string>

class StandaloneConfig
{
public:

    // Settings.
    std::string game_name = "FCEUX_Standalone";
    bool enable_separate_user_config = true;
    bool show_splash_screen = true;
    uint64_t splash_screen_timeout_ms = 2000;

    // Loading and saving.
    void LoadFromFile(std::string const file_path);
    void SaveToFile(std::string const file_path);
};

void LoadDefaultConfig(StandaloneConfig & config);
void LoadUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config);
void SaveUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config);
bool UserConfigExists(StandaloneConfig const & default_config);