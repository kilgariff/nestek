#include "standalone_config.h"

#include "rapidjson/reader.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"

// TODO(ross): This is for get_path_to_exe(), which really should be away in a utility file somewhere.
#include "drivers/win/main.h"

#include "windows.h"
#include "knownfolders.h"
#include "Shlobj.h"
#undef GetObject

#include <fstream>
#include <codecvt>
#include <locale>

using namespace rapidjson;
using namespace std;

//==================================================================================================
static std::string get_user_config_path(std::string const game_name)
{
    LPWSTR pszPath = NULL;
    std::string path;

    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &pszPath) == S_OK)
    {
        std::wstring wide_str_path(pszPath);
        CoTaskMemFree(pszPath);

        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;

        path = converter.to_bytes(wide_str_path);
        path += "\\" + game_name;
        CreateDirectory(path.c_str(), NULL);
        path += "\\user_config.json";
    }

    return path;
}

//==================================================================================================
void StandaloneConfig::LoadFromFile(std::string const file_path)
{
    if (file_path.empty())
    {
        return;
    }

    ifstream ifs(file_path);
    IStreamWrapper isw(ifs);
    Document d;
    d.ParseStream(isw);

    if (d.IsObject())
    {
        game_name = d["game_name"].GetString();
        enable_separate_user_config = d["enable_separate_user_config"].GetBool();
        show_splash_screen = d["show_splash_screen"].GetBool();
        splash_screen_timeout_ms = d["splash_screen_timeout_ms"].GetUint64();

        if (d.HasMember("button_mappings"))
        {
            auto const & button_mappings_array = d["button_mappings"];

            if (button_mappings_array.IsArray())
            {
                button_mappings.clear();

                for (auto const & json_mapping : button_mappings_array.GetArray())
                {
                    auto const & mapping_obj = json_mapping.GetObject();

                    ButtonMapping mapping;

                    using obj_t = rapidjson::GenericObject<true, rapidjson::Value::ValueType>;
                    std::array<obj_t, NESButton::COUNT> enum_to_obj = {
                        mapping_obj["a"].GetObject(),
                        mapping_obj["b"].GetObject(),
                        mapping_obj["select"].GetObject(),
                        mapping_obj["start"].GetObject(),
                        mapping_obj["up"].GetObject(),
                        mapping_obj["down"].GetObject(),
                        mapping_obj["left"].GetObject(),
                        mapping_obj["right"].GetObject()
                    };

                    for (size_t nes_button = NESButton::ButtonA; nes_button < NESButton::COUNT; ++nes_button)
                    {
                        auto const & json_button = enum_to_obj[nes_button];
                        mapping.keyboard_devices[nes_button] = json_button["keyboard_device"].GetUint64();
                        mapping.keyboard_keys[nes_button] = json_button["keyboard_keycode"].GetUint64();
                        mapping.gamepad_devices[nes_button] = json_button["gamepad_device"].GetUint64();
                        mapping.gamepad_buttons[nes_button] = json_button["gamepad_button"].GetUint64();
                    }

                    button_mappings.emplace_back(std::move(mapping));
                }
            }
        }
    }
}

//==================================================================================================
void StandaloneConfig::SaveToFile(std::string const file_path)
{
    if (file_path.empty())
    {
        return;
    }

    ofstream stream(file_path);
    if (stream)
    {
        OStreamWrapper osw(stream);
        PrettyWriter<rapidjson::OStreamWrapper> writer(osw);

        Document d;
        d.SetObject();
    
        d.AddMember("game_name", StringRef(game_name.c_str()), d.GetAllocator());
        d.AddMember("enable_separate_user_config", enable_separate_user_config, d.GetAllocator());
        d.AddMember("show_splash_screen", show_splash_screen, d.GetAllocator());
        d.AddMember("splash_screen_timeout_ms", splash_screen_timeout_ms, d.GetAllocator());

        // Gamepad config.
        std::array<std::string, NESButton::COUNT> const enum_to_key = {
            "a",
            "b",
            "select",
            "start",
            "up",
            "down",
            "left",
            "right"
        };

        {
            Value json_button_mappings(kArrayType);

            for (auto const & player_button_mapping : button_mappings)
            {
                Value json_player_button_mapping(kObjectType);

                for (size_t nes_button = NESButton::ButtonA; nes_button < NESButton::COUNT; ++nes_button)
                {
                    Value json_button_mapping(kObjectType);

                    json_button_mapping.AddMember("keyboard_device", player_button_mapping.keyboard_devices[nes_button], d.GetAllocator());
                    json_button_mapping.AddMember("keyboard_keycode", player_button_mapping.keyboard_keys[nes_button], d.GetAllocator());

                    json_button_mapping.AddMember("gamepad_device", player_button_mapping.gamepad_devices[nes_button], d.GetAllocator());
                    json_button_mapping.AddMember("gamepad_button", player_button_mapping.gamepad_buttons[nes_button], d.GetAllocator());

                    std::string const & key = enum_to_key[nes_button];
                    json_player_button_mapping.AddMember(StringRef(key.c_str()), json_button_mapping, d.GetAllocator());
                }

                json_button_mappings.PushBack(json_player_button_mapping, d.GetAllocator());
            }

            d.AddMember("button_mappings", json_button_mappings, d.GetAllocator());
        }

        d.Accept(writer);
    }
}

//==================================================================================================
void LoadDefaultConfig(StandaloneConfig & config)
{
    std::string const path_to_exe = get_path_to_exe();

    config = {};
    config.LoadFromFile(path_to_exe + "\\standalone_config.json");
}

std::string user_config_path;

//==================================================================================================
void LoadUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config)
{
    if (user_config_path.empty())
    {
        user_config_path = get_user_config_path(default_config.game_name);
    }

    config = default_config; // Use default config as base.
    config.LoadFromFile(user_config_path);
}

//==================================================================================================
void SaveUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config)
{
    if (user_config_path.empty())
    {
        user_config_path = get_user_config_path(default_config.game_name);
    }

    config.SaveToFile(user_config_path);
}

//==================================================================================================
bool UserConfigExists(StandaloneConfig const & default_config)
{
    ifstream file(get_user_config_path(default_config.game_name));

    if (file)
    {
        return true;
    }
    else
    {
        return false;
    }
}