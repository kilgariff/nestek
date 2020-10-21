#include "config_menu.h"
#include "driver.h"

// TODO(ross): This is windows-specific. If we're porting to other platforms we'll need a more general gamepad interface.
#include "drivers/win/main.h"
#include "drivers/win/input.h"

//==================================================================================================
// Top-level option strings.

constexpr size_t config_option_count =
	static_cast<size_t>(ConfigOption::COUNT);

static char const * config_option_str_array[config_option_count] = {
	"Back to game",
	"Remap buttons (player 1)",
	"Remap buttons (player 2)",
	"Delete save game",
	"Exit game"
};

static char const * config_option_to_string(ConfigOption const option)
{
	return config_option_str_array[static_cast<size_t>(option)];
}

//==================================================================================================
// Button remapping option strings.

constexpr size_t config_remap_option_count =
	static_cast<size_t>(ConfigRemapOption::COUNT);

static char const * config_remap_option_str_array[config_remap_option_count] = {
	"A",
	"B",
	"Select",
	"Start",
	"Up",
	"Down",
	"Left",
	"Right",
	"Reset Mapping",
	"Finished Mapping"
};

char const * config_remap_option_to_string(ConfigOption const option)
{
	return config_remap_option_str_array[static_cast<size_t>(option)];
}

//==================================================================================================
// Member functions.

bool ConfigMenu::IsShowing()
{
	return showing_config_menu;
}

bool ConfigMenu::IsAwaitingKey()
{
	return config_remap_awaiting_key;
}

void ConfigMenu::StopAwaitingKey()
{
	config_remap_awaiting_key = false;
}

void ConfigMenu::Show()
{
	state = ConfigMenuState::TopLevel;
	FCEUI_SetEmulationPaused(1);
	showing_config_menu = true;
}

void ConfigMenu::Hide()
{
	state = ConfigMenuState::TopLevel;
	FCEUI_SetEmulationPaused(0);
	FCEU_ResetMessages();
	showing_config_menu = false;
	current_config_option = ConfigOption::BackToGame;
	current_config_remap_option = ConfigRemapOption::ButtonA;
	StopAwaitingKey();
}

void ConfigMenu::BeginRemap(size_t player_idx)
{
	if (player_idx == 0)
	{
		state = ConfigMenuState::RemapPlayer1;
	}
	else
	{
		state = ConfigMenuState::RemapPlayer2;
	}
}

void ConfigMenu::NextOption()
{
	if (state == ConfigMenuState::TopLevel)
	{
		current_config_option =
			static_cast<ConfigOption>((static_cast<int>(current_config_option) + 1) % config_option_count);
	}
	else
	{
		current_config_remap_option =
			static_cast<ConfigRemapOption>((static_cast<int>(current_config_remap_option) + 1) % config_remap_option_count);
	}
}

void ConfigMenu::PreviousOption()
{
	if (state == ConfigMenuState::TopLevel)
	{
		if (current_config_option == ConfigOption::BackToGame)
		{
			current_config_option = static_cast<ConfigOption>(config_option_count - 1);
		}
		else
		{
			current_config_option =
				static_cast<ConfigOption>((static_cast<int>(current_config_option) - 1));
		}
	}
	else
	{
		if (current_config_remap_option == ConfigRemapOption::ButtonA)
		{
			current_config_remap_option = static_cast<ConfigRemapOption>(config_remap_option_count - 1);
		}
		else
		{
			current_config_remap_option =
				static_cast<ConfigRemapOption>((static_cast<int>(current_config_remap_option) - 1));
		}
	}
}

void ConfigMenu::ConfirmOption()
{
	if (state == ConfigMenuState::TopLevel)
	{
		switch (current_config_option)
		{
			case ConfigOption::BackToGame:
			{
				Hide();
				break;
			}

			case ConfigOption::RemapButtonsP1:
			{
				BeginRemap(0);
				break;
			}

			case ConfigOption::RemapButtonsP2:
			{
				// This shows the old win32 config dialog from FCEUX.
				// Keeping it here for reference.
				//ConfigInput(GetMainHWND());

				BeginRemap(1);
				break;
			}

			case ConfigOption::ExitGame:
			{
				state = ConfigMenuState::ExitEmulator;
				break;
			}
		}
	}
	else
	{
		switch (current_config_remap_option)
		{
			case ConfigRemapOption::FinishedRemapping:
			{
				state = ConfigMenuState::TopLevel;
				current_config_remap_option = ConfigRemapOption::ButtonA;
				StopAwaitingKey();
				break;
			}

			case ConfigRemapOption::ResetToDefault:
			{
				// TODO(ross): Reset button mapping to default.
				break;
			}

			default:
			{
				config_remap_awaiting_key = true;
				break;
			}
		}
	}
}

void ConfigMenu::GenerateOutput()
{
	if (showing_config_menu)
	{
		FCEU_ResetMessages();

		std::string config_menu = "\n\n";

		switch (state)
		{
		case ConfigMenuState::TopLevel:
		{
			config_menu += "MENU\n\n";
			for (size_t idx = 0; idx < config_option_count; ++idx)
			{
				config_menu += idx == static_cast<size_t>(current_config_option) ? "> " : "  ";
				config_menu += config_option_str_array[idx];
				config_menu += "\n\n";
			}
			break;
		}

		case ConfigMenuState::RemapPlayer1:
		case ConfigMenuState::RemapPlayer2:
		{
			size_t player_idx = 0;

			config_menu += "REMAP PLAYER";
			if (state == ConfigMenuState::RemapPlayer1)
			{
				config_menu += " 1";
			}
			else
			{
				config_menu += " 2";
				player_idx = 1;
			}

			config_menu += "\n\n";

			if (config_remap_awaiting_key == false)
			{
				for (size_t idx = 0; idx < config_remap_option_count; ++idx)
				{
					config_menu += idx == static_cast<size_t>(current_config_remap_option) ? "> " : "  ";
					config_menu += config_remap_option_str_array[idx];
					if (idx < static_cast<size_t>(ConfigRemapOption::ResetToDefault))
					{
						char * str = MakeButtString(&GetGamePadConfig(player_idx)[idx]);
						size_t const len = strlen(config_remap_option_str_array[idx]);
						for (size_t padc = len; padc < 14; ++padc)
						{
							config_menu += " ";
						}
						config_menu += str;
						free(str);
					}
					config_menu += "\n\n";
				}
			}
			else
			{
				size_t const current_idx = static_cast<size_t>(current_config_remap_option);
				config_menu += "Press the key or button for NES button ";
				config_menu += config_remap_option_str_array[current_idx];
				config_menu += "\n\n";
			}
			break;
		}
		}

		FCEU_DispMessage(config_menu.c_str(), 223);
	}
}

bool ConfigMenu::ShouldExitEmulator()
{
	return state == ConfigMenuState::ExitEmulator;
}

ConfigRemapOption ConfigMenu::GetCurrentRemapOption()
{
	return current_config_remap_option;
}