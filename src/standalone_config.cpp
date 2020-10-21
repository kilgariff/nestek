#include "standalone_config.h"

#include "windows.h"
#include "knownfolders.h"
#include "Shlobj.h"

#include "rapidjson/reader.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"

// TODO(ross): This is for get_path_to_exe(), which really should be away in a utility file somewhere.
#include "drivers/win/main.h"

#include <fstream>
#include <codecvt>
#include <locale>

using namespace rapidjson;
using namespace std;

//==================================================================================================
static std::string get_user_config_path(std::string const game_name)
{
    PWSTR pszPath = NULL;
    std::string path;

    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &pszPath) == S_OK)
    {
        std::wstring wide_str_path(pszPath);
        CoTaskMemFree(pszPath);

        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;

        path = converter.to_bytes(wide_str_path) + "\\" + game_name;
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

//==================================================================================================
void LoadUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config)
{
    config = default_config; // Use default config as base.
    config.LoadFromFile(get_user_config_path(default_config.game_name));
}

//==================================================================================================
void SaveUserConfig(StandaloneConfig const & default_config, StandaloneConfig & config)
{
    config.SaveToFile(get_user_config_path(default_config.game_name));
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