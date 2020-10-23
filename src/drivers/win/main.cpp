/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "common.h"

//little build hack for vs2015 (due to static libs linked in by lua/luaperks)
//could probably rebuild those libs, but this is easier
#if _MSC_VER >= 1900
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

// I like hacks.
#define uint8 __UNO492032
#include <winsock.h>
#include "ddraw.h"
#undef LPCWAVEFORMATEX
#include "dsound.h"
#include "dinput.h"
#include <direct.h>
#include <commctrl.h>
#include <shlobj.h>     // For directories configuration dialog.
#undef uint8

#include <fstream>

#include "../../version.h"
#include "../../types.h"
#include "../../fceu.h"
#include "../../state.h"
#include "../../debug.h"
#include "../../movie.h"

#include "archive.h"
#include "input.h"
#include "netplay.h"
#include "joystick.h"
#include "keyboard.h"
#include "ppuview.h"
#include "cheat.h"
#include "debug.h"
#include "ntview.h"
#include "throttle.h"
#include "palette.h" //For the SetPalette function
#include "main.h"
#include "args.h"
#include "config.h"
#include "sound.h"
#include "wave.h"
#include "video.h"
#include "utils/xstring.h"

#include "standalone_config.h"

#include <string>
#include <cstring>
#include <codecvt>
#include <locale>

//---------------------------
//mbg merge 6/29/06 - new aboutbox

#if defined(MSVC)
 #ifdef _M_X64
   #define _MSVC_ARCH "x64"
 #else
   #define _MSVC_ARCH "x86"
 #endif
 #ifdef _DEBUG
  #define _MSVC_BUILD "debug"
 #else
  #define _MSVC_BUILD "release"
 #endif
 #define __COMPILER__STRING__ "msvc " _Py_STRINGIZE(_MSC_VER) " " _MSVC_ARCH " " _MSVC_BUILD
 #define _Py_STRINGIZE(X) _Py_STRINGIZE1((X))
 #define _Py_STRINGIZE1(X) _Py_STRINGIZE2 ## X
 #define _Py_STRINGIZE2(X) #X
 //re: http://72.14.203.104/search?q=cache:HG-okth5NGkJ:mail.python.org/pipermail/python-checkins/2002-November/030704.html+_msc_ver+compiler+version+string&hl=en&gl=us&ct=clnk&cd=5
#elif defined(__GNUC__)
 #ifdef _DEBUG
  #define _GCC_BUILD "debug"
 #else
  #define _GCC_BUILD "release"
 #endif
 #define __COMPILER__STRING__ "gcc " __VERSION__ " " _GCC_BUILD
#else
 #define __COMPILER__STRING__ "unknown"
#endif

// 64-bit build requires manifest to use common controls 6 (style adapts to windows version)
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")

// External functions
extern std::string cfgFile;		//Contains the filename of the config file used.
extern bool turbo;				//Is game in turbo mode?
void ResetVideo(void);
void ShowCursorAbs(int w);
void HideFWindow(int h);
void FixWXY(int pref, bool shift_held);
void SetMainWindowStuff(void);
int GetClientAbsRect(LPRECT lpRect);
void UpdateFCEUWindow(void);
void FCEUD_Update(uint8 *XBuf, int32 *Buffer, int Count);

// Internal variables
int frameSkipAmt = 18;
uint8 *xbsave = NULL;
int eoptions = EO_BGRUN | EO_FORCEISCALE | EO_BESTFIT | EO_BGCOLOR | EO_SQUAREPIXELS;

//global variables
int soundoptions = SO_SECONDARY | SO_GFOCUS;
int soundrate = 48000;
int soundbuftime = 50;
int soundquality = 1;

//Sound volume controls (range 0-150 by 10's)j-----
int soundvolume = 150;			//Master sound volume
int soundTrianglevol = 256;		//Sound channel Triangle - volume control
int soundSquare1vol = 256;		//Sound channel Square1 - volume control
int soundSquare2vol = 256;		//Sound channel Square2 - volume control
int soundNoisevol = 256;		//Sound channel Noise - volume control
int soundPCMvol = 256;			//Sound channel PCM - volume control
//-------------------------------------------------

int KillFCEUXonFrame = 0; //TODO: clean up, this is used in fceux, move it over there?

double winsizemulx = 2.0, winsizemuly = 2.0;
double tvAspectX = TV_ASPECT_DEFAULT_X, tvAspectY = TV_ASPECT_DEFAULT_Y;
int genie = 0;
int pal_emulation = 0;
int pal_setting_specified = 0;
int dendy = 0;
int dendy_setting_specified = 0;
bool swapDuty = 0; // some Famicom and NES clones had duty cycle bits swapped
int ntsccol = 0, ntsctint, ntschue;
std::string BaseDirectory;
int PauseAfterLoad;
unsigned int skippy = 0;  //Frame skip
int frameSkipCounter = 0; //Counter for managing frame skip
// Contains the names of the overridden standard directories
// in the order roms, nonvol, states, fdsrom, snaps, cheats, movies, memwatch, basic bot, macro, input presets, lua scripts, avi, base
char *directory_names[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int edit_id[14] = { EDIT_ROM, EDIT_BATTERY, EDIT_STATE, EDIT_FDSBIOS, EDIT_SCREENSHOT, EDIT_CHEAT, EDIT_MOVIE, EDIT_MEMWATCH, EDIT_BOT, EDIT_MACRO, EDIT_PRESET, EDIT_LUA, EDIT_AVI, EDIT_ROOT };
int browse_btn_id[14] = {BUTTON_ROM, BUTTON_BATTERY, BUTTON_STATE, BUTTON_FDSBIOS, BUTTON_SCREENSHOT, BUTTON_CHEAT, BUTTON_MOVIE, BUTTON_MEMWATCH, BUTTON_BOT, BUTTON_MACRO, BUTTON_PRESET, BUTTON_LUA, BUTTON_AVI, BUTTON_ROOT };
std::string cfgFile = "fceux.cfg";
//Handle of the main window.
HWND hAppWnd = 0;

uint32 goptions = GOO_DISABLESS;

// Some timing-related variables (now ignored).
int maxconbskip = 32;          //Maximum consecutive blit skips.
int ffbskip = 32;              //Blit skips per blit when FF-ing

HINSTANCE fceu_hInstance;
HACCEL fceu_hAccel;

HRESULT ddrval;

static char TempArray[2048];

static int exiting = 0;
static volatile int moocow = 0;

int windowedfailed = 0;
int fullscreen = 0;	//Windows files only, variable that keeps track of fullscreen status

static volatile int _userpause = 0; //mbg merge 7/18/06 changed tasbuild was using this only in a couple of places

extern int autoHoldKey, autoHoldClearKey;
extern int frame_display, input_display;

int soundo = 1;

int srendlinen = 8;
int erendlinen = 231;
int srendlinep = 0;
int erendlinep = 239;

//mbg 6/30/06 - indicates that the main loop should close the game as soon as it can
bool closeGame = false;

// Counts the number of frames that have not been displayed.
// Used for the bot, to skip frames (makes things faster).
int BotFramesSkipped = 0;

// Instantiated FCEUX stuff:
bool SingleInstanceOnly=false; // Enable/disable option
bool DoInstantiatedExit=false;
HWND DoInstantiatedExitWindow;

SplashScreen splash_screen;
StandaloneConfig default_config;
StandaloneConfig user_config;
StandaloneConfig * active_config = &default_config;

void SaveGamepadConfig()
{
    // Only save out the gamepad config if separate user config is enabled.
    if (active_config->enable_separate_user_config == false)
    {
        return;
    }

    active_config->button_mappings.clear();

    for (size_t player_idx = 0; player_idx < 2; ++player_idx)
    {
        ButtonMapping player_button_mapping;

        for (size_t nes_button = NESButton::ButtonA; nes_button < NESButton::COUNT; ++nes_button)
        {
            ButtConfig * bc = &GetGamePadConfig(player_idx)[nes_button];

            bool const has_keyboard = bc->NumC >= 1 && bc->ButtType[0] == BUTTC_KEYBOARD;

            player_button_mapping.keyboard_devices[nes_button] = has_keyboard ? bc->DeviceNum[0] : ButtonMapping::sentinel;
            player_button_mapping.keyboard_keys[nes_button] = has_keyboard ? bc->ButtonNum[0] : ButtonMapping::sentinel;

            bool const has_gamepad = bc->NumC > 1 && bc->ButtType[1] == BUTTC_JOYSTICK;

            player_button_mapping.gamepad_devices[nes_button] = has_gamepad ? bc->DeviceNum[1] : ButtonMapping::sentinel;
            player_button_mapping.gamepad_buttons[nes_button] = has_gamepad ? bc->ButtonNum[1] : ButtonMapping::sentinel;
        }

        active_config->button_mappings.emplace_back(std::move(player_button_mapping));
    }

    SaveUserConfig(default_config, user_config);
}

void ToggleFullscreenConfig()
{
    active_config->start_fullscreen = !active_config->start_fullscreen;
    SaveUserConfig(default_config, user_config);
    SetVideoMode(active_config->start_fullscreen);
}

void SetupGamepadConfig()
{
    for (size_t player_idx = 0; player_idx < active_config->button_mappings.size(); ++player_idx)
    {
        auto const & player_button_mapping = active_config->button_mappings[player_idx];

        for (size_t nes_button = NESButton::ButtonA; nes_button < NESButton::COUNT; ++nes_button)
        {
            uint32_t const keyboard_device = player_button_mapping.keyboard_devices[nes_button];
            uint32_t const keyboard_key = player_button_mapping.keyboard_keys[nes_button];

            uint32_t const gamepad_device = player_button_mapping.gamepad_devices[nes_button];
            uint32_t const gamepad_button = player_button_mapping.gamepad_buttons[nes_button];
            
            bool const has_keyboard = keyboard_device != ButtonMapping::sentinel;
            bool const has_gamepad = gamepad_device != ButtonMapping::sentinel;

            ButtConfig * bc = GetGamePadConfig(player_idx) + nes_button;

            bc->NumC = 0;
            bc->ButtType[0] = 0;
            bc->DeviceNum[0] = 0;
            bc->ButtonNum[0] = 0;

            if (has_keyboard)
            {
                bc->NumC = 1;
                bc->ButtType[0] = BUTTC_KEYBOARD;
                bc->DeviceNum[0] = keyboard_device;
                bc->ButtonNum[0] = keyboard_key;
            }

            if (has_gamepad)
            {
                bc->NumC = 2;
                bc->ButtType[1] = BUTTC_JOYSTICK;
                bc->DeviceNum[1] = gamepad_device;
                bc->ButtonNum[1] = gamepad_button;
            }
        }
    }
}

void LoadDefaultGamepadConfig(size_t player_idx)
{
    if (default_config.button_mappings.size() > player_idx)
    {
        if (active_config->button_mappings.size() <= player_idx)
        {
            active_config->button_mappings.resize(player_idx + 1);
        }

        active_config->button_mappings[player_idx] = default_config.button_mappings[player_idx];
    }

    SetupGamepadConfig();
    SaveGamepadConfig();
}

// Internal functions
void SetDirs()
{
	int x;

	static int jlist[14]= {
		FCEUIOD_ROMS,
		FCEUIOD_NV,
		FCEUIOD_STATES,
		FCEUIOD_FDSROM,
		FCEUIOD_SNAPS,
		FCEUIOD_CHEATS,
		FCEUIOD_MOVIES,
		FCEUIOD_MEMW,
		FCEUIOD_BBOT,
		FCEUIOD_MACRO,
		FCEUIOD_INPUT,
		FCEUIOD_LUA,
		FCEUIOD_AVI,
		FCEUIOD__COUNT};

//	FCEUI_SetSnapName((eoptions & EO_SNAPNAME)!=0);

	for(x=0; x < sizeof(jlist) / sizeof(*jlist); x++)
	{
		FCEUI_SetDirOverride(jlist[x], directory_names[x]);
	}

	if(directory_names[13])
	{
		FCEUI_SetBaseDirectory(directory_names[13]);
	}
	else
	{
		FCEUI_SetBaseDirectory(BaseDirectory);
	}
}

/// Creates a directory.
/// @param dirname Name of the directory to create.
void DirectoryCreator(const char* dirname)
{
	CreateDirectory(dirname, 0);
}

/// Removes a directory.
/// @param dirname Name of the directory to remove.
void DirectoryRemover(const char* dirname)
{
	RemoveDirectory(dirname);
}

/// Used to walk over the default directories array.
/// @param callback Callback function that's called for every default directory name.
void DefaultDirectoryWalker(void (*callback)(const char*))
{
	unsigned int curr_dir;

	for(curr_dir = 0; curr_dir < NUMBER_OF_DEFAULT_DIRECTORIES; curr_dir++)
	{
		if(!directory_names[curr_dir])
		{
			sprintf(
				TempArray,
				"%s\\%s",
				directory_names[NUMBER_OF_DEFAULT_DIRECTORIES] ? directory_names[NUMBER_OF_DEFAULT_DIRECTORIES] : BaseDirectory.c_str(),
				default_directory_names[curr_dir]
			);

			callback(TempArray);
		}
	}
}

/// Remove empty, unused directories.
void RemoveDirs()
{
	DefaultDirectoryWalker(DirectoryRemover);
}

///Creates the default directories.
void CreateDirs()
{
	DefaultDirectoryWalker(DirectoryCreator);
}

//Fills the BaseDirectory string
//TODO: Potential buffer overflow caused by limited size of BaseDirectory?
void GetBaseDirectory(void)
{
	char temp[2048];
	GetModuleFileName(0, temp, 2048);
	BaseDirectory = temp;

	size_t truncate_at = BaseDirectory.find_last_of("\\/");
	if(truncate_at != std::string::npos)
		BaseDirectory = BaseDirectory.substr(0,truncate_at);
}

int BlockingCheck()
{
	MSG msg;
	moocow = 1;

	while(PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
	{
		if(GetMessage(&msg, 0, 0, 0) > 0)
		{
			//other accelerator capable dialogs could be added here
			int handled = 0;

			// Sound Config
			extern HWND uug;
			if(!handled && uug && IsChild(uug, msg.hwnd))
				handled = IsDialogMessage(uug, &msg);

			// Palette Config
			extern HWND hWndPal;
			if(!handled && hWndPal && IsChild(hWndPal, msg.hwnd))
				handled = IsDialogMessage(hWndPal, &msg);

			// PPU Viewer
			extern HWND hPPUView;
			if(!handled && hPPUView && IsChild(hPPUView, msg.hwnd))
				handled = IsDialogMessage(hPPUView, &msg);

			// Nametable Viewer
			extern HWND hNTView;
			if(!handled && hNTView && IsChild(hNTView, msg.hwnd))
				handled = IsDialogMessage(hNTView, &msg);

			// Logs
			extern HWND logwin;
			if(!handled && logwin && IsChild(logwin, msg.hwnd))
				handled = IsDialogMessage(logwin, &msg);

			// Header Editor
			extern HWND hHeadEditor;
			if (!handled && hHeadEditor && IsChild(hHeadEditor, msg.hwnd))
				handled = IsDialogMessage(hHeadEditor, &msg);

			// Netplay (Though it's quite dummy and nearly forgotten)
			extern HWND netwin;
			if (!handled && netwin && IsChild(netwin, msg.hwnd))
				handled = IsDialogMessage(netwin, &msg);

			/* //adelikat - Currently no accel keys are used in the main window.  Uncomment this block to activate them.
			if(!handled)
				if(msg.hwnd == hAppWnd)
				{
					handled = TranslateAccelerator(hAppWnd,fceu_hAccel,&msg);
					if(handled)
					{
						int zzz=9;
					}
				}
			*/

			if(!handled)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	moocow = 0;

	return exiting ? 0 : 1;
}

void UpdateRendBounds()
{
	FCEUI_SetRenderedLines(srendlinen, erendlinen, srendlinep, erendlinep);
}

/// Shows an error message in a message box.
///@param errormsg Text of the error message.
void FCEUD_PrintError(const char *errormsg)
{
	AddLogText(errormsg, 1);

	if (fullscreen && (eoptions & EO_HIDEMOUSE))
		ShowCursorAbs(1);

	MessageBox(0, errormsg, FCEU_NAME" Error", MB_ICONERROR | MB_OK | MB_SETFOREGROUND | MB_TOPMOST);

	if (fullscreen && (eoptions & EO_HIDEMOUSE))
		ShowCursorAbs(0);
}

///Generates a compiler identification string.
/// @return Compiler identification string
const char *FCEUD_GetCompilerString()
{
	return 	__COMPILER__STRING__;
}

//Displays the about box
void ShowAboutBox()
{
	MessageBox(hAppWnd, FCEUI_GetAboutString(), FCEU_NAME, MB_OK);
}

//Exits FCE Ultra
void DoFCEUExit()
{
	if(exiting)    //Eh, oops.  I'll need to try to fix this later.
		return;

	exiting = 1;
	closeGame = true;//mbg 6/30/06 - for housekeeping purposes we need to exit after the emulation cycle finishes
	// remember the ROM name
	extern char LoadedRomFName[2048];
	if (GameInfo)
		strcpy(romNameWhenClosingEmulator, LoadedRomFName);
	else
		romNameWhenClosingEmulator[0] = 0;
}

void FCEUD_OnCloseGame()
{
}

//Changes the thread priority of the main thread.
void DoPriority()
{
	if(eoptions & EO_HIGHPRIO)
	{
		if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
		{
			AddLogText("Error setting thread priority to THREAD_PRIORITY_HIGHEST.", 1);
		}
	}
	else
	{
		if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL))
		{
			AddLogText("Error setting thread priority to THREAD_PRIORITY_NORMAL.", 1);
		}
	}
}

int DriverInitialize()
{
	if(soundo)
		soundo = InitSound();

	SetVideoMode(0);

	// NOTE(ross): This forces fullscreen, but messes up debugging. Only do this for the final build.
	SetVideoMode(active_config->start_fullscreen);
	InitInputStuff();             /* Initialize DInput interfaces. */

	return 1;
}

static void DriverKill(void)
{
	// Save config file
	//sprintf(TempArray, "%s/fceux.cfg", BaseDirectory.c_str());
	//sprintf(TempArray, "%s/%s", BaseDirectory.c_str(),cfgFile.c_str());
	//SaveConfig(TempArray);

	DestroyInput();

	ResetVideo();

	if(soundo)
	{
		TrashSoundNow();
	}

	CloseWave();

	ByebyeWindow();
}

void do_exit()
{
	DriverKill();
	timeEndPeriod(1);
	FCEUI_Kill();
}

//Puts the default directory names into the elements of the directory_names array that aren't already defined.
//adelikat: commenting out this function, we don't need this.  This turns the idea of directory overrides to directory assignment
/*
void initDirectories()
{
	for (unsigned int i = 0; i < NUMBER_OF_DEFAULT_DIRECTORIES; i++)
	{
		if (directory_names[i] == 0)
		{
			sprintf(
				TempArray,
				"%s\\%s",
				directory_names[i] ? directory_names[i] : BaseDirectory.c_str(),
				default_directory_names[i]
			);

			directory_names[i] = (char*)malloc(strlen(TempArray) + 1);
			strcpy(directory_names[i], TempArray);
		}
	}

	if (directory_names[NUMBER_OF_DIRECTORIES - 1] == 0)
	{
		directory_names[NUMBER_OF_DIRECTORIES - 1] = (char*)malloc(BaseDirectory.size() + 1);
		strcpy(directory_names[NUMBER_OF_DIRECTORIES - 1], BaseDirectory.c_str());
	}
}
*/

std::string get_path_to_exe()
{
    std::string path_to_exe;

    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring wide_str_path(path);
    std::wstring::size_type pos = wide_str_path.find_last_of(L"\\/");
    wide_str_path = wide_str_path.substr(0, pos);

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    path_to_exe = converter.to_bytes(wide_str_path);
    
    return path_to_exe;
}

static BOOL CALLBACK EnumCallbackFCEUXInstantiated(HWND hWnd, LPARAM lParam)
{
	//LPSTR lpClassName = '\0';
	std::string TempString;
	char buf[512];

	GetClassName(hWnd, buf, 511);
	//Console.WriteLine(lpClassName.ToString());

	TempString = buf;

	if (TempString != "FCEUXWindowClass")
		return true;

	//zero 17-sep-2013 - removed window caption test which wasnt really making a lot of sense to me and was broken in any event
	if (hWnd != hAppWnd)
	{
		DoInstantiatedExit = true;
		DoInstantiatedExitWindow = hWnd;
	}

	//printf("[%03i] Found '%s'\n", ++WinCount, buf);
	return true;
}

#include "x6502.h"
int main(int argc,char *argv[])
{
	{
#ifdef MULTITHREAD_STDLOCALE_WORKAROUND
		// Note: there's a known threading bug regarding std::locale with MSVC according to
		// http://connect.microsoft.com/VisualStudio/feedback/details/492128/std-locale-constructor-modifies-global-locale-via-setlocale
		int iPreviousFlag = ::_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
#endif
		using std::locale;
		locale::global(locale(locale::classic(), "", locale::collate | locale::ctype));

#ifdef MULTITHREAD_STDLOCALE_WORKAROUND
		if (iPreviousFlag > 0 )
			::_configthreadlocale(iPreviousFlag);
#endif
	}

	SetThreadAffinityMask(GetCurrentThread(),1);

    // Load default and user standalone configuration files.
    LoadDefaultConfig(default_config);
    if (default_config.enable_separate_user_config)
    {
        LoadUserConfig(default_config, user_config);
        SaveUserConfig(default_config, user_config);
        active_config = &user_config;
    }

	initArchiveSystem();

	if(timeBeginPeriod(1) != TIMERR_NOERROR)
	{
		AddLogText("Error setting timer granularity to 1ms.", DO_ADD_NEWLINE);
	}

	InitCommonControls();

    if (active_config->show_splash_screen)
    {
        splash_screen.LoadAndShow(user_config.splash_screen_timeout_ms);
    }

	if(!FCEUI_Initialize())
	{
		do_exit();
		return 1;
	}

	fceu_hInstance = GetModuleHandle(0);
	fceu_hAccel = LoadAccelerators(fceu_hInstance,MAKEINTRESOURCE(IDR_ACCELERATOR1));

	// Get the base directory
	GetBaseDirectory();

	std::string const rom_file_path = get_path_to_exe() + "\\game.nes";

	fullscreen = 0;

	if(PlayInput)
		PlayInputFile = fopen(PlayInput, "rb");
	if(DumpInput)
		DumpInputFile = fopen(DumpInput, "wb");

	extern int disableBatteryLoading;
	if(PlayInput || DumpInput)
		disableBatteryLoading = 1;

	int saved_pal_setting = !!pal_emulation;

	//Bleh, need to find a better place for this.
	{
        FCEUI_SetGameGenie(genie!=0);

        fullscreen = !!fullscreen;
        soundo = !!soundo;
        frame_display = !!frame_display;
        allowUDLR = !!allowUDLR;
        EnableBackgroundInput = !!EnableBackgroundInput;
		dendy = !!dendy;

		KeyboardSetBackgroundAccess(EnableBackgroundInput!=0);
		JoystickSetBackgroundAccess(EnableBackgroundInput!=0);

        FCEUI_SetSoundVolume(soundvolume);
		FCEUI_SetSoundQuality(soundquality);
		FCEUI_SetTriangleVolume(soundTrianglevol);
		FCEUI_SetSquare1Volume(soundSquare1vol);
		FCEUI_SetSquare2Volume(soundSquare2vol);
		FCEUI_SetNoiseVolume(soundNoisevol);
		FCEUI_SetPCMVolume(soundPCMvol);
	}

	//Since a game doesn't have to be loaded before the GUI can be used, make
	//sure the temporary input type variables are set.
	ParseGIInput(NULL);

	// Initialize default directories
	//CreateDirs();
	SetDirs();

	DoVideoConfigFix();
	DoTimingConfigFix();

	//restore the last user-set palette (cpalette and cpalette_count are preserved in the config file)
	if(eoptions & EO_CPALETTE)
	{
		FCEUI_SetUserPalette(cpalette,cpalette_count);
	}

	CreateMainWindow();

	if(!InitDInput())
	{
		do_exit();
		return 1;
	}

	if(!DriverInitialize())
	{
		do_exit();
		return 1;
	}

	InitSpeedThrottle();

	//if (rom_file)
	//{
		ALoad(rom_file_path.c_str());
	//} else
	//{
	//	if (AutoResumePlay && romNameWhenClosingEmulator && romNameWhenClosingEmulator[0])
	//		ALoad(romNameWhenClosingEmulator, 0, true);
	//	if (eoptions & EO_FOAFTERSTART)
	//		LoadNewGamey(hAppWnd, 0);
	//}

	if (PAL && pal_setting_specified && !dendy_setting_specified)
		dendy = 0;

	if (PAL && !dendy)
        FCEUI_SetRegion(1, pal_setting_specified);
	else if (dendy)
        FCEUI_SetRegion(2, dendy_setting_specified);
	else
        FCEUI_SetRegion(0, pal_setting_specified || dendy_setting_specified);

	if(PaletteToLoad)
	{
		SetPalette(PaletteToLoad);
		free(PaletteToLoad);
		PaletteToLoad = NULL;
	}

	if(GameInfo && StateToLoad)
	{
		FCEUI_LoadState(StateToLoad);
		free(StateToLoad);
		StateToLoad = NULL;
	}

	//Initiates AVI capture mode, will set up proper settings, and close FCUEX once capturing is finished
	if(AVICapture && AviToLoad)	//Must be used in conjunction with AviToLoad
	{
		//We want to disable flags that will pause the emulator
		PauseAfterLoad = 0;
		KillFCEUXonFrame = AVICapture;
	}

	if(AviToLoad)
	{
		FCEUI_AviBegin(AviToLoad);
		free(AviToLoad);
		AviToLoad = NULL;
	}

    SetupGamepadConfig();

	if (PauseAfterLoad) FCEUI_ToggleEmulationPause();
	SetAutoFirePattern(AFon, AFoff);
	UpdateCheckedMenuItems();
doloopy:
	UpdateFCEUWindow();
	if(GameInfo)
	{
		while(GameInfo)
		{
	        uint8 *gfx=0; ///contains framebuffer
			int32 *sound=0; ///contains sound data buffer
			int32 ssize=0; ///contains sound samples count

			if (turbo)
			{
				if (!frameSkipCounter)
				{
					frameSkipCounter = frameSkipAmt;
					skippy = 0;
				}
				else
				{
					frameSkipCounter--;
					if (muteTurbo) skippy = 2;	//If mute turbo is on, we want to bypass sound too, so set it to 2
						else skippy = 1;				//Else set it to 1 to just frameskip
				}

			}
			else skippy = 0;

            FCEUI_Emulate(&gfx, &sound, &ssize, skippy); //emulate a single frame
            FCEUD_Update(gfx, sound, ssize); //update displays and debug tools

            UpdateConfigMenu();

			//mbg 6/30/06 - close game if we were commanded to by calls nested in FCEUI_Emulate()
			if (closeGame)
			{
				FCEUI_CloseGame();
				GameInfo = NULL;
			}

		}
		//xbsave = NULL;
		RedrawWindow(hAppWnd,0,0,RDW_ERASE|RDW_INVALIDATE);
	}
  else
    UpdateRawInputAndHotkeys();
	Sleep(50);
	if(!exiting)
		goto doloopy;

	DriverKill();
	timeEndPeriod(1);
	FCEUI_Kill();

	return(0);
}

void FCEUX_LoadMovieExtras(const char * fname) {
}

//mbg merge 7/19/06 - the function that contains the code that used to just be UpdateFCEUWindow() and FCEUD_UpdateInput()
void _updateWindow()
{
	UpdateFCEUWindow();
	PPUViewDoBlit();
	NTViewDoBlit(0);
	//UpdateTasEditor();	//AnS: moved to FCEUD_Update
}

void win_debuggerLoopStep()
{
	FCEUD_UpdateInput();
	_updateWindow();

	//question:
	//should this go here, or in the loop?

	// HACK: break when Frame Advance is pressed
	extern bool frameAdvanceRequested;
	extern int frameAdvance_Delay_count, frameAdvance_Delay;
	if (frameAdvanceRequested)
	{
		if (frameAdvance_Delay_count == 0 || frameAdvance_Delay_count >= frameAdvance_Delay)
			FCEUI_SetEmulationPaused(EMULATIONPAUSED_FA);
		if (frameAdvance_Delay_count < frameAdvance_Delay)
			frameAdvance_Delay_count++;
	}
}

void win_debuggerLoop()
{
	//delay until something causes us to unpause.
	//either a hotkey or a debugger command
	while(FCEUI_EmulationPaused() && !FCEUI_EmulationFrameStepped())
	{
		Sleep(50);
		win_debuggerLoopStep();
	}
}

// Update the game and gamewindow with a new frame
void FCEUD_Update(uint8 *XBuf, int32 *Buffer, int Count)
{
    win_SoundSetScale(fps_scale); //If turboing and mute turbo is true, bypass this

    //write all the sound we generated.
    if (soundo && Buffer && Count && !(muteTurbo && turbo))
    {
        win_SoundWriteData(Buffer, Count); //If turboing and mute turbo is true, bypass this
    }

    if (splash_screen.IsShowing())
    {
        BlitImage(splash_screen.GetBytes(), splash_screen.GetByteCount());
    }
    else
    {
        splash_screen.Unload();

        //blit the framebuffer
        if (XBuf)
            FCEUD_BlitScreen(XBuf);
    }

    //update debugging displays
    _updateWindow();

    extern bool JustFrameAdvanced;

    //MBG TODO - think about this logic
    //throttle

    bool throttle = true;
    if ((eoptions&EO_NOTHROTTLE))
    {
        if (!soundo) throttle = false;
    }

    if (throttle)  //if throttling is enabled..
        if (!turbo) //and turbo is disabled..
            if (!FCEUI_EmulationPaused()
                || JustFrameAdvanced
                )
                //then throttle
                while (SpeedThrottle())
                {
                    FCEUD_UpdateInput();
                    _updateWindow();
                }


    //sleep just to be polite
    if (!JustFrameAdvanced && FCEUI_EmulationPaused())
    {
        Sleep(50);
    }

    //while(EmulationPaused==1 && inDebugger)
    //{
    //	Sleep(50);
    //	BlockingCheck();
    //	FCEUD_UpdateInput(); //should this update the CONTROLS??? or only the hotkeys etc?
    //}

    ////so, we're not paused anymore.

    ////something of a hack, but straightforward:
    ////if we were paused, but not in the debugger, then unpause ourselves and step.
    ////this is so that the cpu won't cut off execution due to being paused, but the debugger _will_
    ////cut off execution as soon as it makes it into the main cpu cycle loop
    //if(FCEUI_EmulationPaused() && !inDebugger) {
    //	FCEUI_ToggleEmulationPause();
    //	FCEUI_Debugger().step = 1;
    //	FCEUD_DebugBreakpoint();
    //}

    //make sure to update the input once per frame
    FCEUD_UpdateInput();
}

static void FCEUD_MakePathDirs(const char *fname)
{
	char path[MAX_PATH];
	const char* div = fname;

	do
	{
		const char* fptr = strchr(div, '\\');

		if(!fptr)
		{
			fptr = strchr(div, '/');
		}

		if(!fptr)
		{
			break;
		}

		int off = fptr - fname;
		strncpy(path, fname, off);
		path[off] = '\0';
		mkdir(path);

		div = fptr + 1;

		while(div[0] == '\\' || div[0] == '/')
		{
			div++;
		}

	} while(1);
}

EMUFILE_FILE* FCEUD_UTF8_fstream(const char *n, const char *m)
{
	if(strchr(m, 'w') || strchr(m, '+'))
	{
		FCEUD_MakePathDirs(n);
	}

	EMUFILE_FILE *fs = new EMUFILE_FILE(n,m);
	if(!fs->is_open()) {
		delete fs;
		return 0;
	} else return fs;
}

FILE *FCEUD_UTF8fopen(const char *n, const char *m)
{
	if(strchr(m, 'w') || strchr(m, '+'))
	{
		FCEUD_MakePathDirs(n);
	}

	return(fopen(n, m));
}

int status_icon = 1;

int FCEUD_ShowStatusIcon(void)
{
	return status_icon;
}

void FCEUD_ToggleStatusIcon(void)
{
	status_icon = !status_icon;
	UpdateCheckedMenuItems();
}

char *GetRomName(bool force)
{
	//The purpose of this function is to format the ROM name stored in LoadedRomFName
	//And return a char array with just the name with path or extension
	//The purpose of this function is to populate a save as dialog with the ROM name as a default filename
	extern char LoadedRomFName[2048];	//Contains full path of ROM
	std::string Rom;					//Will contain the formatted path
	if(GameInfo || force)						//If ROM is loaded
		{
		char drv[PATH_MAX], dir[PATH_MAX], name[PATH_MAX], ext[PATH_MAX];
		splitpath(LoadedRomFName,drv,dir,name,ext);	//Extract components of the ROM path
		Rom = name;						//Pull out the Name only
		}
	else
		Rom = "";
	char*mystring = (char*)malloc(2048*sizeof(char));
	strcpy(mystring, Rom.c_str());		//Convert string to char*

	return mystring;
}

char *GetRomPath(bool force)
{
	//The purpose of this function is to format the ROM name stored in LoadedRomFName
	//And return a char array with just the name with path or extension
	//The purpose of this function is to populate a save as dialog with the ROM name as a default filename
	extern char LoadedRomFName[2048];	//Contains full path of ROM
	std::string Rom;					//Will contain the formatted path
	if(GameInfo || force)						//If ROM is loaded
		{
		char drv[PATH_MAX], dir[PATH_MAX], name[PATH_MAX], ext[PATH_MAX];
		splitpath(LoadedRomFName,drv,dir,name,ext);	//Extract components of the ROM path
		Rom = drv;						//Pull out the Path only
		Rom.append(dir);
		}
	else
		Rom = "";
	char*mystring = (char*)malloc(2048*sizeof(char));
	strcpy(mystring, Rom.c_str());		//Convert string to char*

	return mystring;
}
