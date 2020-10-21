#pragma once

#include <cstdint>

enum class ConfigOption : uint8_t
{
    BackToGame,
    RemapButtonsP1,
    RemapButtonsP2,
    DeleteSave,
    ExitGame,
    COUNT
};

enum class ConfigRemapOption : uint8_t
{
    ButtonA,
    ButtonB,
    ButtonSelect,
    ButtonStart,
    DirectionUp,
    DirectionDown,
    DirectionLeft,
    DirectionRight,
    ResetToDefault,
    FinishedRemapping,
    COUNT
};

enum class ConfigMenuState
{
    TopLevel,
    RemapPlayer1,
    RemapPlayer2,
    ExitEmulator
};

class ConfigMenu
{
private:

    // General.
    bool showing_config_menu = false;
    ConfigMenuState state;

    // Top-level.
    ConfigOption current_config_option = ConfigOption::BackToGame;

    // Remapping.
    ConfigRemapOption current_config_remap_option = ConfigRemapOption::ButtonA;
    bool config_remap_awaiting_key = false;

public:

    ConfigOption GetCurrentConfigOption();
    ConfigRemapOption GetCurrentRemapOption();

    ConfigMenuState GetState()
    {
        return state;
    }

    void Show();
    void Hide();

    void NextOption();
    void PreviousOption();
    void ConfirmOption();

    bool IsShowing();
    bool IsAwaitingKey();
    void StopAwaitingKey();

    void BeginRemap(size_t player_idx);

    void GenerateOutput();

    bool ShouldExitEmulator();
};