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
*f
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

//http://www.nubaria.com/en/blog/?p=289

// File description: Everything relevant for the main window should go here. This
//                   does not include functions relevant for dialog windows.

#include "../../input.h"
#include "../../state.h"
#include "../../cheat.h" //adelikat: For FCEU_LoadGameCheats()
#include "../../version.h"
#include "../../video.h" //adelikat: For ScreenshotAs Get/Set functions
#include "window.h"
#include "main.h"
#include "state.h"

#include "sound.h"
#include "wave.h"
#include "input.h"
#include "video.h"
#include "input.h"
#include "fceu.h"

#include "cheat.h"
#include "ppuview.h"
#include "debug.h"
#include "ntview.h"
#include "header_editor.h"
#include "throttle.h"
#include "keyboard.h"
#include "joystick.h"
#include "oldmovie.h"
#include "movie.h"
#include "guiconfig.h"
#include "timing.h"
#include "palette.h"
#include "directories.h"
#include "gui.h"
#include "help.h"
#include "movie.h"
#include "utils/xstring.h"
#include "file.h"
#include "config.h" //adelikat: For SaveConfigFile()
#include "config_menu.h"

#include "keyscan.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <cinttypes>
#include <algorithm>

#include "Win32InputBox.h"
extern int32 fps_scale_unpaused;

using namespace std;

//----Context Menu - Some dynamically added menu items
#define FCEUX_CONTEXT_UNHIDEMENU        60000
#define FCEUX_CONTEXT_LOADLASTLUA       60001
#define FCEUX_CONTEXT_CLOSELUAWINDOWS   60002
#define FCEUX_CONTEXT_TOGGLESUBTITLES   60003
#define FCEUX_CONTEXT_DUMPSUBTITLES     60004

//********************************************************************************
//Globals
//********************************************************************************

//Handles----------------------------------------------
static HMENU fceumenu = 0;	  //Main menu.
// HWND pwindow;				  //Client Area
HMENU recentmenu;				//Recent Menu
HMENU recentluamenu;			//Recent Lua Files Menu
HMENU recentmoviemenu;			//Recent Movie Files Menu
HMENU hfceuxcontext;		  //Handle to context menu
HMENU hfceuxcontextsub;		  //Handle to context sub menu
HWND MainhWnd;				  //Main FCEUX(Parent) window Handle.  Dialogs should use GetMainHWND() to get this

//Extern variables-------------------------------------
extern bool movieSubtitles;
extern FCEUGI *GameInfo;
extern int EnableAutosave;
extern bool frameAdvanceLagSkip;
extern bool turbo;
extern bool movie_readonly;
extern bool AutoSS;			//flag for whether an auto-save has been made
extern int newppu;
extern INT_PTR CALLBACK ReplayMetadataDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);	//Metadata dialog
extern bool CheckFileExists(const char* filename);	//Receives a filename (fullpath) and checks to see if that file exists
extern bool oldInputDisplay;
extern int RAMInitOption;
extern int movieRecordMode;

//AutoFire-----------------------------------------------
void ShowNetplayConsole(void); //mbg merge 7/17/06 YECH had to add
void MapInput(void);
void SetAutoFirePattern(int onframes, int offframes);
void SetAutoFireOffset(int offset);
static int CheckedAutoFirePattern = MENU_AUTOFIRE_PATTERN_1;
static int CheckedAutoFireOffset = MENU_AUTOFIRE_OFFSET_1;
int GetCheckedAutoFirePattern();
int GetCheckedAutoFireOffset();

//Internal variables-------------------------------------
char *md5_asciistr(uint8 digest[16]);
static int winwidth, winheight;
static volatile int nofocus = 0;
static int tog = 0;					//Toggle for Hide Menu
static bool loggingSound = false;
static LONG WindowXC=1<<30,WindowYC;
int MainWindow_wndx, MainWindow_wndy;
static uint32 mousex,mousey,mouseb;
static int vchanged = 0;
int menuYoffset = 0;
bool wasPausedByCheats = false;		//For unpausing the emulator if paused by the cheats dialog
bool rightClickEnabled = false;		//If set to false, the right click context menu will be disabled.
bool fullscreenByDoubleclick = false;
uint8 BWorldData[1 + 13 + 1];

//Config Menu-------------------------------------
ConfigMenu config_menu;

//Function Prototypes
void ChangeMenuItemText(int menuitem, string text);			//Alters a menu item name
void ChangeContextMenuItemText(int menuitem, string text, HMENU menu);	//Alters a context menu item name

//Recent Menu Strings ------------------------------------
char *recent_files[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
const unsigned int MENU_FIRST_RECENT_FILE = 600;
const unsigned int MAX_NUMBER_OF_RECENT_FILES = sizeof(recent_files)/sizeof(*recent_files);

//Recent Lua Menu ----------------------------------------
char *recent_lua[] = { 0, 0, 0, 0, 0 };
const unsigned int LUA_FIRST_RECENT_FILE = 50000;
const unsigned int MAX_NUMBER_OF_LUA_RECENT_FILES = sizeof(recent_lua)/sizeof(*recent_lua);

//Recent Movie Menu -------------------------------------
char *recent_movie[] = { 0, 0, 0, 0, 0 };
const unsigned int MOVIE_FIRST_RECENT_FILE = 51000;
const unsigned int MAX_NUMBER_OF_MOVIE_RECENT_FILES = sizeof(recent_movie)/sizeof(*recent_movie);

//Exported variables ------------------------------------

int EnableBackgroundInput = 0;
int ismaximized = 0;
WNDPROC DefaultEditCtrlProc;

//Help Menu subtopics
string moviehelp = "MovieRecording";		 //Movie Recording
string gettingstartedhelp = "Gettingstarted";//Getting Started

//********************************************************************************
void CloseConfigMenu()
{
	config_menu.Hide();

	// Stop timer that's listening for gamepad input.
	KillTimer(GetMainHWND(), 1);
}

void UpdateConfigMenu()
{
	config_menu.GenerateOutput();

	if (config_menu.ShouldExitEmulator())
	{
		DoFCEUExit();
	}
}

void SetMainWindowText()
{
	SetWindowText(hAppWnd, active_config->game_name.c_str());
}

bool HasRecentFiles()
{
	for (int x = 0; x < MAX_NUMBER_OF_RECENT_FILES; x++)
	{
		if (recent_files[x])
			return true;
	}
	return false;
}

HWND GetMainHWND()
{
	return MainhWnd;
}

int GetCheckedAutoFirePattern()
{
//adelikat: sorry, I'm sure there is an easier way to accomplish this.
//This function allows the proper check to be displayed on the auto-fire pattern offset at start up when another autofire pattern is saved in config
	if (AFon == 1 && AFoff == 2) return MENU_AUTOFIRE_PATTERN_2;
	if (AFon == 1 && AFoff == 3) return MENU_AUTOFIRE_PATTERN_3;
	if (AFon == 1 && AFoff == 4) return MENU_AUTOFIRE_PATTERN_4;
	if (AFon == 1 && AFoff == 5) return MENU_AUTOFIRE_PATTERN_5;
	if (AFon == 2 && AFoff == 1) return MENU_AUTOFIRE_PATTERN_6;
	if (AFon == 2 && AFoff == 2) return MENU_AUTOFIRE_PATTERN_7;
	if (AFon == 2 && AFoff == 3) return MENU_AUTOFIRE_PATTERN_8;
	if (AFon == 2 && AFoff == 4) return MENU_AUTOFIRE_PATTERN_9;
	if (AFon == 3 && AFoff == 1) return MENU_AUTOFIRE_PATTERN_10;
	if (AFon == 3 && AFoff == 2) return MENU_AUTOFIRE_PATTERN_11;
	if (AFon == 3 && AFoff == 3) return MENU_AUTOFIRE_PATTERN_12;
	if (AFon == 4 && AFoff == 1) return MENU_AUTOFIRE_PATTERN_13;
	if (AFon == 4 && AFoff == 2) return MENU_AUTOFIRE_PATTERN_14;
	if (AFon == 5 && AFoff == 1) return MENU_AUTOFIRE_PATTERN_15;

	return MENU_AUTOFIRE_PATTERN_1;
}

int GetCheckedAutoFireOffset()
{
	if (AutoFireOffset == 1) return MENU_AUTOFIRE_OFFSET_2;
	if (AutoFireOffset == 2) return MENU_AUTOFIRE_OFFSET_3;
	if (AutoFireOffset == 3) return MENU_AUTOFIRE_OFFSET_4;
	if (AutoFireOffset == 4) return MENU_AUTOFIRE_OFFSET_5;
	if (AutoFireOffset == 5) return MENU_AUTOFIRE_OFFSET_6;
	return MENU_AUTOFIRE_OFFSET_1;
}

// Internal functions



static void ConvertFCM(HWND hwndOwner)
{
	std::string initdir = FCEU_GetPath(FCEUMKF_MOVIE);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwndOwner;
	ofn.lpstrFilter = "FCEU <2.0 Movie Files (*.fcm)\0*.fcm\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = new char[640*1024]; //640K should be enough for anyone
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = 640*1024;
	ofn.lpstrInitialDir = initdir.c_str();
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	ofn.lpstrDefExt = "fcm";
	ofn.lpstrTitle = "Select old movie(s) for conversion";

	if(GetOpenFileName(&ofn))
	{
		std::vector<std::string> todo;

		//build a list of movies to convert. this might be one, or it might be many, depending on what the user selected
		if(ofn.nFileExtension==0)
		{
			//multiselect
			std::string dir = ofn.lpstrFile;
			char* cp = ofn.lpstrFile + dir.size() + 1;
			while(*cp) 
			{
				std::string fname = cp;
				todo.push_back(dir + "/" + fname);
				cp += fname.size() + 1;
			}
		}
		else
		{
			todo.push_back(ofn.lpstrFile);
		}

		SetCursor(LoadCursor(0,IDC_WAIT));

		//convert each movie
		int okcount = 0;
		for(uint32 i=0;i<todo.size();i++)
		{
			std::string infname = todo[i];
			
			//produce output filename
			std::string outname;
			size_t dot = infname.find_last_of(".");
			if(dot == std::string::npos)
				outname = infname + ".fm2";
			else
				outname = infname.substr(0,dot) + ".fm2";

			MovieData md;
			EFCM_CONVERTRESULT result = convert_fcm(md, infname);
			if(result==FCM_CONVERTRESULT_SUCCESS)
			{
				okcount++;
				EMUFILE_FILE* outf = FCEUD_UTF8_fstream(outname, "wb");
				md.dump(outf,false);
				delete outf;
			} else {
				std::string msg = "Failure converting " + infname + "\r\n\r\n" + EFCM_CONVERTRESULT_message(result);
				MessageBox(hwndOwner,msg.c_str(),"Failure converting fcm", 0);
			}
		}

		std::string okmsg = "Converted " + stditoa(okcount) + " movie(s). There were " + stditoa(todo.size()-okcount) + " failure(s).";
		MessageBox(hwndOwner,okmsg.c_str(),"FCM Conversion results", 0);
	}

	delete[] ofn.lpstrFile;
}

void CalcWindowSize(RECT *al)
{
	int xres = 256;
	if(fullscreen && vmodes[0].special==3)
		xres=301;
	if(!fullscreen && winspecial==3)
		xres=301;

	double screen_width = VNSWID_NU(xres);
	double screen_height = FSettings.TotalScanlines();
	if (eoptions & EO_TVASPECT)
		screen_width = ceil(screen_height * (screen_width / xres) * (tvAspectX / tvAspectY));

	al->left = 0;
	al->top = 0;
	al->right = ceil(screen_width * winsizemulx);
	al->bottom = menuYoffset + ceil(screen_height * winsizemuly);

	AdjustWindowRectEx(al,
		GetWindowLong(hAppWnd, GWL_STYLE),
		GetMenu(hAppWnd) != NULL,
		GetWindowLong(hAppWnd, GWL_EXSTYLE)
		);

	al->right -= al->left;
	al->left = 0;
	al->bottom -= al->top;
	al->top=0;
}

/// Updates the menu items that should only be enabled if a game is loaded.
/// @param enable Flag that indicates whether the menus should be enabled (1) or disabled (0). 
void updateGameDependentMenus()
{
	// they are quite simple, enabled only when game is loaded
	const int menu_ids[]= {
		MENU_CLOSE_FILE,
		ID_FILE_SCREENSHOT,
		ID_FILE_SAVESCREENSHOTAS,
		MENU_RESET,
		MENU_POWER,
		MENU_INSERT_COIN,
		MENU_SWITCH_DISK,
		MENU_EJECT_DISK,
		MENU_RECORD_AVI,
		MENU_INPUT_BARCODE,
		MENU_STOP_AVI,
		MENU_RECORD_WAV,
		MENU_STOP_WAV,
		MENU_HIDE_MENU,
		//MENU_DEBUGGER,
		MENU_PPUVIEWER,
		MENU_NAMETABLEVIEWER,
		MENU_HEXEDITOR,
		MENU_TRACELOGGER,
		MENU_CDLOGGER,
		MENU_GAMEGENIEDECODER,
		MENU_CHEATS,
		ID_RAM_SEARCH,
		ID_RAM_WATCH,
		ID_TOOLS_TEXTHOOKER
	};

	bool enable = GameInfo != 0;
	for (unsigned int i = 0; i < sizeof(menu_ids) / sizeof(*menu_ids); i++)
		EnableMenuItem(fceumenu, menu_ids[i], MF_BYCOMMAND | enable ? MF_ENABLED : MF_GRAYED | MF_DISABLED);

	// Special treatment for the iNES head editor, only when no game is loaded or an NES game is loaded
	extern iNES_HEADER head;
	enable = GameInfo == 0 || !strncmp((const char*)&head, "NES\x1A", 4);
	EnableMenuItem(fceumenu, MENU_INESHEADEREDITOR, MF_BYCOMMAND | enable ? MF_ENABLED : MF_GRAYED | MF_DISABLED);
}

//Updates menu items which need to be checked or unchecked.
void UpdateCheckedMenuItems()
{
}

void UpdateContextMenuItems(HMENU context, int whichContext)
{

}


//adelikat: This removes a recent menu item from any recent menu, and shrinks the list accordingly
//The update recent menu function will need to be run after this
void RemoveRecentItem(unsigned int which, char**bufferArray, const unsigned int MAX)
{
	//which = array item to remove
	//buffer = char array of the recent menu
	//MAX = max number of array items

	//Just in case
	if (which >= MAX)
		return;
	
	//Remove the item
	if(bufferArray[which])
	{
		free(bufferArray[which]);
	}

	//If the item is not the last one in the list, shift the remaining ones up
	if (which < (MAX-1))
	{
		//Move the remaining items up
		for(unsigned int x = which+1; x < MAX; x++)
		{
			bufferArray[x-1] = bufferArray[x];	//Shift each remaining item up by 1
		}
	}
	
	bufferArray[MAX-1] = 0;	//Clear out the last item since it is empty now no matter what
}

/// Updates recent files / recent directories menu
/// @param menu Menu handle of the main window's menu
/// @param strs Strings to add to the menu
/// @param mitem Menu ID of the recent files / directory menu
/// @param baseid Menu ID of the first subitem
void UpdateRMenu(HMENU menu, char **strs, unsigned int mitem, unsigned int baseid)
{
	// UpdateRMenu(recentmenu, recent_files, MENU_RECENT_FILES, MENU_FIRST_RECENT_FILE);

	MENUITEMINFO moo;
	int x;

	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);
	moo.hSubMenu = menu;
	moo.fState = strs[0] ? MFS_ENABLED : MFS_GRAYED;

	SetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);

	// Remove all recent files submenus
	for(x = 0; x < MAX_NUMBER_OF_RECENT_FILES; x++)
	{
		RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
	}

	// Recreate the menus
	for(x = MAX_NUMBER_OF_RECENT_FILES - 1; x >= 0; x--)
	{  
		// Skip empty strings
		if(!strs[x])
			continue;

		std::string tmp = strs[x];
		std::string archiveName, fileName, fileToOpen;
		FCEU_SplitArchiveFilename(tmp,archiveName,fileName,fileToOpen);
		if(archiveName != "")
			tmp = archiveName + " <" + fileName + ">";

		//clamp this string to 128 chars
		if(tmp.size()>128)
			tmp = tmp.substr(0,128);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;

		//// Fill in the menu text.
		//if(strlen(strs[x]) < 128)
		//{
		//	sprintf(tmp, "&%d. %s", ( x + 1 ) % 10, strs[x]);
		//}
		//else
		//{
		//	sprintf(tmp, "&%d. %s", ( x + 1 ) % 10, strs[x] + strlen( strs[x] ) - 127);
		//}

		// Insert the menu item
		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = (LPSTR)tmp.c_str();
		InsertMenuItem(menu, 0, 1, &moo);
	}

	DrawMenuBar(hAppWnd);
}

/// Helper function to populate the recent directories and recent files arrays.
/// @param addString String to add to the array.
/// @param bufferArray Array where the string will be added.
/// @param arrayLen Length of the bufferArray.
/// @param menu Menu handle of the main menu.
/// @param menuItem
/// @param baseID
void UpdateRecentArray(const char* addString, char** bufferArray, unsigned int arrayLen, HMENU menu, unsigned int menuItem, unsigned int baseId)
{
	// Try to find out if the filename is already in the recent files list.
	for(unsigned int x = 0; x < arrayLen; x++)
	{
		if(bufferArray[x])
		{
			if(!strcmp(bufferArray[x], addString))    // Item is already in list.
			{
				// If the filename is in the file list don't add it again.
				// Move it up in the list instead.

				int y;
				char *tmp;

				// Save pointer.
				tmp = bufferArray[x];

				for(y = x; y; y--)
				{
					// Move items down.
					bufferArray[y] = bufferArray[y - 1];
				}

				// Put item on top.
				bufferArray[0] = tmp;

				// Update the recent files menu
				UpdateRMenu(menu, bufferArray, menuItem, baseId);

				return;
			}
		}
	}

	// The filename wasn't found in the list. That means we need to add it.

	// If there's no space left in the recent files list, get rid of the last
	// item in the list.
	if(bufferArray[arrayLen - 1])
	{
		free(bufferArray[arrayLen - 1]);
	}

	// Move the other items down.
	for(unsigned int x = arrayLen - 1; x; x--)
	{
		bufferArray[x] = bufferArray[x - 1];
	}

	// Add the new item.
	bufferArray[0] = (char*)malloc(strlen(addString) + 1); //mbg merge 7/17/06 added cast
	strcpy(bufferArray[0], addString);

	// Update the recent files menu
	UpdateRMenu(menu, bufferArray, menuItem, baseId);
}

/// Add a filename to the recent files list.
/// @param filename Name of the file to add.
void AddRecentFile(const char *filename)
{
	UpdateRecentArray(filename, recent_files, MAX_NUMBER_OF_RECENT_FILES, recentmenu, MENU_RECENT_FILES, MENU_FIRST_RECENT_FILE);
}

void LoadRecentRom(int slot)
{
	char*& fname = recent_files[slot];
	if(fname)
	{
		if (!ALoad(fname))
		{
			int result = MessageBox(hAppWnd, "Remove from list?", "Could Not Open Recent File", MB_YESNO);
			if (result == IDYES)
			{
				RemoveRecentItem(slot, recent_files, MAX_NUMBER_OF_RECENT_FILES);
				UpdateRMenu(recentmenu, recent_files, MENU_RECENT_FILES, MENU_FIRST_RECENT_FILE);
			}
		}
	}
}

void UpdateLuaRMenu(HMENU menu, char **strs, unsigned int mitem, unsigned int baseid)
{
	MENUITEMINFO moo;
	int x;

	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);
	moo.hSubMenu = menu;
	moo.fState = strs[0] ? MFS_ENABLED : MFS_GRAYED;

	SetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);

	// Remove all recent files submenus
	for(x = 0; x < MAX_NUMBER_OF_LUA_RECENT_FILES; x++)
	{
		RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
	}

	// Recreate the menus
	for(x = MAX_NUMBER_OF_LUA_RECENT_FILES - 1; x >= 0; x--)
	{  
		// Skip empty strings
		if(!strs[x])
			continue;

		string tmp = strs[x];
		
		//clamp this string to 128 chars
		if(tmp.size()>128)
			tmp = tmp.substr(0,128);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;
		
		// Insert the menu item
		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = (LPSTR)tmp.c_str();
		InsertMenuItem(menu, 0, 1, &moo);
	}

	DrawMenuBar(hAppWnd);
}

void UpdateRecentLuaArray(const char* addString, char** bufferArray, unsigned int arrayLen, HMENU menu, unsigned int menuItem, unsigned int baseId)
{
	// Try to find out if the filename is already in the recent files list.
	for(unsigned int x = 0; x < arrayLen; x++)
	{
		if(bufferArray[x])
		{
			if(!strcmp(bufferArray[x], addString))    // Item is already in list.
			{
				// If the filename is in the file list don't add it again.
				// Move it up in the list instead.

				int y;
				char *tmp;

				// Save pointer.
				tmp = bufferArray[x];

				for(y = x; y; y--)
				{
					// Move items down.
					bufferArray[y] = bufferArray[y - 1];
				}

				// Put item on top.
				bufferArray[0] = tmp;

				// Update the recent files menu
				UpdateLuaRMenu(menu, bufferArray, menuItem, baseId);

				return;
			}
		}
	}

	// The filename wasn't found in the list. That means we need to add it.

	// If there's no space left in the recent files list, get rid of the last
	// item in the list.
	if(bufferArray[arrayLen - 1])
	{
		free(bufferArray[arrayLen - 1]);
	}

	// Move the other items down.
	for(unsigned int x = arrayLen - 1; x; x--)
	{
		bufferArray[x] = bufferArray[x - 1];
	}

	// Add the new item.
	bufferArray[0] = (char*)malloc(strlen(addString) + 1); //mbg merge 7/17/06 added cast
	strcpy(bufferArray[0], addString);

	// Update the recent files menu
	UpdateLuaRMenu(menu, bufferArray, menuItem, baseId);

}

void AddRecentLuaFile(const char *filename)
{
	UpdateRecentLuaArray(filename, recent_lua, MAX_NUMBER_OF_LUA_RECENT_FILES, recentluamenu, MENU_LUA_RECENT, LUA_FIRST_RECENT_FILE); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateMovieRMenu(HMENU menu, char **strs, unsigned int mitem, unsigned int baseid)
{
	MENUITEMINFO moo;
	int x;

	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_SUBMENU | MIIM_STATE;

	GetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);
	moo.hSubMenu = menu;
	moo.fState = strs[0] ? MFS_ENABLED : MFS_GRAYED;

	SetMenuItemInfo(GetSubMenu(fceumenu, 0), mitem, FALSE, &moo);

	// Remove all recent files submenus
	for(x = 0; x < MAX_NUMBER_OF_MOVIE_RECENT_FILES; x++)
	{
		RemoveMenu(menu, baseid + x, MF_BYCOMMAND);
	}

	// Recreate the menus
	for(x = MAX_NUMBER_OF_MOVIE_RECENT_FILES - 1; x >= 0; x--)
	{  
		// Skip empty strings
		if(!strs[x])
			continue;

		string tmp = strs[x];
		
		//clamp this string to 128 chars
		if(tmp.size()>128)
			tmp = tmp.substr(0,128);

		moo.cbSize = sizeof(moo);
		moo.fMask = MIIM_DATA | MIIM_ID | MIIM_TYPE;
		
		// Insert the menu item
		moo.cch = tmp.size();
		moo.fType = 0;
		moo.wID = baseid + x;
		moo.dwTypeData = (LPSTR)tmp.c_str();
		InsertMenuItem(menu, 0, 1, &moo);
	}

	DrawMenuBar(hAppWnd);
}

void UpdateRecentMovieArray(const char* addString, char** bufferArray, unsigned int arrayLen, HMENU menu, unsigned int menuItem, unsigned int baseId)
{
	// Try to find out if the filename is already in the recent files list.
	for(unsigned int x = 0; x < arrayLen; x++)
	{
		if(bufferArray[x])
		{
			if(!strcmp(bufferArray[x], addString))    // Item is already in list.
			{
				// If the filename is in the file list don't add it again.
				// Move it up in the list instead.

				int y;
				char *tmp;

				// Save pointer.
				tmp = bufferArray[x];

				for(y = x; y; y--)
				{
					// Move items down.
					bufferArray[y] = bufferArray[y - 1];
				}

				// Put item on top.
				bufferArray[0] = tmp;

				// Update the recent files menu
				UpdateMovieRMenu(menu, bufferArray, menuItem, baseId);

				return;
			}
		}
	}

	// The filename wasn't found in the list. That means we need to add it.

	// If there's no space left in the recent files list, get rid of the last
	// item in the list.
	if(bufferArray[arrayLen - 1])
	{
		free(bufferArray[arrayLen - 1]);
	}

	// Move the other items down.
	for(unsigned int x = arrayLen - 1; x; x--)
	{
		bufferArray[x] = bufferArray[x - 1];
	}

	// Add the new item.
	bufferArray[0] = (char*)malloc(strlen(addString) + 1); //mbg merge 7/17/06 added cast
	strcpy(bufferArray[0], addString);

	// Update the recent files menu
	UpdateMovieRMenu(menu, bufferArray, menuItem, baseId);

}

void AddRecentMovieFile(const char *filename)
{
	UpdateRecentMovieArray(filename, recent_movie, MAX_NUMBER_OF_MOVIE_RECENT_FILES, recentmoviemenu, MENU_MOVIE_RECENT, MOVIE_FIRST_RECENT_FILE); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Hides the main menu.
//@param hide_menu Flag to turn the main menu on (0) or off (1) 
void HideMenu(unsigned int hide_menu)
{
	if(hide_menu)
	{
		SetMenu(hAppWnd, 0);
	}
	else
	{
		//SetMenu(hAppWnd, fceumenu);
	}
}

void HideFWindow(int h)
{
	LONG desa;

	if (h)  /* Full-screen. */
	{
		RECT bo;
		GetWindowRect(hAppWnd, &bo);
		WindowXC = bo.left;
		WindowYC = bo.top;

		SetMenu(hAppWnd, 0);
		desa=WS_POPUP | WS_CLIPSIBLINGS;  
	} else
	{
		desa = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS;
		HideMenu(tog);

		/* Stupid DirectDraw bug(I think?) requires this.  Need to investigate it. */
		SetWindowPos(hAppWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE);
	}

	SetWindowLongPtr(hAppWnd, GWL_STYLE, desa | ( GetWindowLong(hAppWnd, GWL_STYLE) & WS_VISIBLE ));
	SetWindowPos(hAppWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
}

//Toggles the display status of the main menu.
void ToggleHideMenu(void)
{ 
	//if(!fullscreen && !nofocus && (GameInfo || tog))
	//{
	//	tog ^= 1;
	//	HideMenu(tog);
	//	SetMainWindowStuff();
	//}
}

//Toggles the display status of the main menu.
//TODO: We could get rid of this one.
void FCEUD_HideMenuToggle(void)
{
	//ToggleHideMenu();
}

void CloseGame()
{
	if (GameInfo)
	{
		FCEUI_CloseGame();
		updateGameDependentMenus();
		SetMainWindowText();
	}
}

bool ALoad(const char *nameo, char* innerFilename, bool silent)
{
	HideMenu(1);

	int oldPaused = EmulationPaused;

	// loading is not started yet, so the game can continue;
	// FCEUI_LoadGameVirtual() already had an FCEUI_CloseGame() call after loading success;
	// if (GameInfo) FCEUI_CloseGame();

	if (FCEUI_LoadGameVirtual(nameo, !(pal_setting_specified || dendy_setting_specified), silent))
	{
		pal_emulation = FCEUI_GetCurrentVidSystem(0, 0);

		UpdateCheckedMenuItems();

		PushCurrentVideoSettings();

		std::string recentFileName = nameo;
		if(GameInfo->archiveFilename && GameInfo->archiveCount>1)
			recentFileName = (std::string)GameInfo->archiveFilename + "|" + GameInfo->filename;
		else
			recentFileName = nameo;
		
		AddRecentFile(recentFileName.c_str());

		RefreshThrottleFPS();

		if(eoptions & EO_HIDEMENU && !tog)
		{
			ToggleHideMenu();
		}

		if(eoptions & EO_FSAFTERLOAD)
		{
			SetFSVideoMode();
		}
	}
	else
	{
		return false;
	}
	
	SetMainWindowText();
	ParseGIInput(GameInfo);

	updateGameDependentMenus();
	EmulationPaused = oldPaused;
	return true;
}

/// Shows an Open File dialog and opens the ROM if the user selects a ROM file.
/// @param hParent Handle of the main window
/// @param initialdir Directory that's pre-selected in the Open File dialog.
void LoadNewGamey(HWND hParent, const char *initialdir)
{
	//const char filter[] = "All usable files (*.nes,*.nsf,*.fds,*.unf,*.zip,*.rar,*.7z,*.gz)\0*.nes;*.nsf;*.fds;*.unf;*.zip;*.rar;*.7z;*.gz\0All non-compressed usable files (*.nes,*.nsf,*.fds,*.unf)\0*.nes;*.nsf;*.fds;*.unf\0All Files (*.*)\0*.*\0\0";
	//char nameo[2048];

	//// Create the Open File dialog
	//OPENFILENAME ofn;
	//memset(&ofn,0,sizeof(ofn));

	//ofn.lStructSize=sizeof(ofn);
	//ofn.hInstance=fceu_hInstance;
	//ofn.lpstrTitle=FCEU_NAME" Open File...";
	//ofn.lpstrFilter=filter;
	//nameo[0]=0;
	//ofn.hwndOwner=hParent;
	//ofn.lpstrFile=nameo;
	//ofn.nMaxFile=256;
	//ofn.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY; //OFN_EXPLORER|OFN_ENABLETEMPLATE|OFN_ENABLEHOOK;
	//string stdinitdir = FCEU_GetPath(FCEUMKF_ROMS);
	//
	//if (initialdir)					//adelikat: If a directory is specified in the function parameter, it should take priority
	//	ofn.lpstrInitialDir = initialdir;
	//else							//adelikat: Else just use the override, if no override it will default to 0 - last directory used.
	//	ofn.lpstrInitialDir = stdinitdir.c_str();
	//
	//// Show the Open File dialog
	//if(GetOpenFileName(&ofn))
	//{
	//	ALoad(nameo);
	//}
}

void GetMouseData(uint32 (&md)[3])
{
	extern RECT bestfitRect;

	int xres = 256;
	if(fullscreen && vmodes[0].special==3)
		xres=301;
	if(!fullscreen && winspecial==3)
		xres=301;

	double screen_width = VNSWID_NU(xres);
	double screen_height = FSettings.TotalScanlines();

	if (eoptions & EO_BESTFIT && (bestfitRect.top || bestfitRect.left))
	{
		if ((int)mousex <= bestfitRect.left)
		{
			md[0] = 0;
		} else if ((int)mousex >= bestfitRect.right)
		{
			md[0] = screen_width - 1;
		} else
		{
			md[0] = screen_width * (mousex - bestfitRect.left) / (bestfitRect.right - bestfitRect.left);
		}
		if ((int)mousey <= bestfitRect.top)
		{
			md[1] = 0;
		} else if ((int)mousey >= bestfitRect.bottom)
		{
			md[1] = screen_height - 1;
		} else
		{
			md[1] = screen_height * (mousey - bestfitRect.top) / (bestfitRect.bottom - bestfitRect.top);
		}
	} else
	{
		RECT client_rect;
		GetClientRect(hAppWnd, &client_rect);
		if ((int)mousex <= client_rect.left)
		{
			md[0] = 0;
		} else if ((int)mousex >= client_rect.right)
		{
			md[0] = screen_width - 1;
		} else
		{
			md[0] = screen_width * (mousex - client_rect.left) / (client_rect.right - client_rect.left);
		}
		if ((int)mousey <= client_rect.top)
		{
			md[1] = 0;
		} else if ((int)mousey >= client_rect.bottom)
		{
			md[1] = screen_height - 1;
		} else
		{
			md[1] = screen_height * (mousey - client_rect.top) / (client_rect.bottom - client_rect.top);
		}
	}
	md[0] += VNSCLIP;
	md[1] += FSettings.FirstSLine;
	md[2] = ((mouseb & MK_LBUTTON) ? 1 : 0) | (( mouseb & MK_RBUTTON ) ? 2 : 0);
}

void GetMouseRelative(int32 (&md)[3])
{
	static int cx = -1;
	static int cy = -1;

	int dx = 0;
	int dy = 0;

	bool constrain = (fullscreen != 0) && (nofocus == 0);

	if (constrain || cx < 0 || cy < 0)
	{
		RECT window;
		GetWindowRect(hAppWnd, &window);
		cx = (window.left + window.right) / 2;
		cy = (window.top + window.bottom) / 2;
	}

	POINT cursor;
	if (GetCursorPos(&cursor))
	{
		dx = cursor.x - cx;
		dy = cursor.y - cy;

		if (constrain)
		{
			SetCursorPos(cx,cy);
		}
		else
		{
			cx = cursor.x;
			cy = cursor.y;
		}
	}
	
	md[0] = dx;
	md[1] = dy;
	md[2] = ((mouseb & MK_LBUTTON) ? 1 : 0) | (( mouseb & MK_RBUTTON ) ? 2 : 0);
}

void DumpSubtitles(HWND hWnd)
{ 
	const char filter[]="Subtitles files (*.srt)\0*.srt\0All Files (*.*)\0*.*\0\0";
	char nameo[2048];

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrTitle="Save Subtitles as...";
	ofn.lpstrFilter = filter;
	strcpy(nameo, mass_replace(GetRomName(), "|", ".").c_str());
	ofn.lpstrFile = nameo;
	ofn.nMaxFile = 256;
	ofn.Flags = OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "srt";

	if (GetSaveFileName(&ofn))
	{
		FILE *srtfile;
		srtfile = fopen(nameo, "w");
		if (srtfile) 
		{
			extern std::vector<int> subtitleFrames;
			extern std::vector<std::string> subtitleMessages;
			float fps = (currMovieData.palFlag == 0 ? 60.0988 : 50.0069); // NTSC vs PAL
			float subduration = 3; // seconds for the subtitles to be displayed

			for (unsigned int i = 0; i < subtitleFrames.size(); i++)
			{
				fprintf(srtfile, "%u\n", i+1); // starts with 1, not 0
				double seconds, ms, endseconds, endms;
				seconds = subtitleFrames[i]/fps;
				if (i+1 < subtitleFrames.size()) // there's another subtitle coming after this one
				{
					if (subtitleFrames[i+1]-subtitleFrames[i] < subduration*fps) // avoid two subtitles at the same time
					{
						endseconds = (subtitleFrames[i+1]-1)/fps; // frame x: subtitle1; frame x+1 subtitle2
					} else {
						endseconds = seconds+subduration;
					}
				} else {
					endseconds = seconds+subduration;
				}
				
				ms = modf(seconds, &seconds);
				endms = modf(endseconds, &endseconds);
				// this is just beyond ugly, don't show it to your kids
				fprintf(srtfile,
				"%02.0f:%02d:%02d,%03d --> %02.0f:%02d:%02d,%03d\n", // hh:mm:ss,ms --> hh:mm:ss,ms
				floor(seconds/3600),	(int)floor(seconds/60   ) % 60, (int)floor(seconds)	% 60, (int)(ms*1000),
				floor(endseconds/3600), (int)floor(endseconds/60) % 60, (int)floor(endseconds) % 60, (int)(endms*1000));
				fprintf(srtfile, "%s\n\n", subtitleMessages[i].c_str()); // new line for every subtitle
			}
			fclose(srtfile);
		}
	}

	return;

}


//Message loop of the main window
LRESULT FAR PASCAL AppWndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	MainhWnd = hWnd;
	int whichContext = 0;
	RECT file_rect;
	RECT help_rect;
	int x = 0;
	
	PCOPYDATASTRUCT pcData;

	switch(msg)
	{
	case WM_COPYDATA:

		pcData = (PCOPYDATASTRUCT) lParam;

	    switch ( pcData->dwData )
		{ 
	    case 1: //cData.dwData = 1; (can use for other types as well)
			if (!ALoad((LPSTR) ( (DATA *) (pcData->lpData) )-> strFilePath))
				MessageBox(hWnd,"File from second instance failed to open", "Failed to open file", MB_OK);
			break;
		}
		break;

	case WM_LBUTTONDBLCLK:
	{
		/*if (fullscreenByDoubleclick)
		{
			extern void ToggleFullscreen();
			ToggleFullscreen();
			return 0;
		} else
		{
			mouseb=wParam;
			goto proco;
		}*/
		break;
	}

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
		mouseb=wParam;
		goto proco;

	case WM_MOUSEWHEEL:
	{
		//// send the message to TAS Editor
		//if (taseditorWindow.hwndTASEditor)
		//	SendMessage(taseditorWindow.hwndTASEditor, msg, wParam, lParam);
		return 0;
	}

	case WM_MBUTTONDOWN:
	{
		//if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		//	playback.handleMiddleButtonClick();
		return 0;
	}

	case WM_RBUTTONUP:
	{
		//// If TAS Editor is engaged, rightclick shouldn't popup menus, because right button is used with wheel input for TAS Editor's own purposes
		//if (!FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		//{
		//	if (rightClickEnabled)
		//	{
		//		//If There is a movie loaded
		//		int whichContext = GetCurrentContextIndex();

		//		hfceuxcontext = LoadMenu(fceu_hInstance,"FCEUCONTEXTMENUS");
		//		hfceuxcontextsub = GetSubMenu(hfceuxcontext, whichContext);
		//		UpdateContextMenuItems(hfceuxcontextsub, whichContext);
		//		UpdateMenuHotkeys(whichContext);
		//		pt.x = LOWORD(lParam);		//Get mouse x in terms of client area
		//		pt.y = HIWORD(lParam);		//Get mouse y in terms of client area
		//		ClientToScreen(hAppWnd, (LPPOINT) &pt);	//Convert client area x,y to screen x,y
		//		TrackPopupMenu(hfceuxcontextsub,TPM_RIGHTBUTTON,(pt.x),(pt.y),0,hWnd,0);	//Create menu
		//	} else
		//	{
		//		mouseb=wParam;
		//	}
		//}
		//goto proco;
	}

	case WM_MOVE: 
	{
		if (!IsIconic(hWnd)) {
		RECT wrect;
		GetWindowRect(hWnd,&wrect);
		MainWindow_wndx = wrect.left;
		MainWindow_wndy = wrect.top;

		#ifdef WIN32
		WindowBoundsCheckNoResize(MainWindow_wndx,MainWindow_wndy,wrect.right);
		#endif
		}
		goto proco;
	}

	case WM_MOUSEMOVE:
	{
		mousex=LOWORD(lParam);
		mousey=HIWORD(lParam);
		goto proco;
	}

	case WM_ERASEBKGND:
		if(xbsave)
			return(0);
		else
			goto proco;

	case WM_PAINT:
		if (xbsave)
		{
			InvalidateRect(hWnd, NULL, false);		// AnS: HACK in order to force the entire client area to be redrawn; this fixes the bug with DIRECTDRAWCLIPPER sometimes calculating wrong region
			PAINTSTRUCT ps;
			BeginPaint(hWnd,&ps);
			FCEUD_BlitScreen(xbsave);
			EndPaint(hWnd,&ps);
			return(0);
		}
		goto proco;

	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			nofocus = 1;
		else if (!fullscreen && !changerecursive && !windowedfailed)
		{
			switch(wParam)
			{
			case SIZE_MAXIMIZED: 
				ismaximized = 1;
				changerecursive = 1;
				SetMainWindowStuff();
				changerecursive = 0;
				break;
			case SIZE_RESTORED: 
				ismaximized = 0;
				changerecursive = 1;
				SetMainWindowStuff();
				changerecursive = 0;
				break;
			}
		}
		break;
	case WM_SIZING:
		{
			RECT *wrect=(RECT *)lParam;
			RECT srect;

			int h=wrect->bottom-wrect->top;
			int w=wrect->right-wrect->left;
			int how = 0;

			if(wParam == WMSZ_BOTTOM || wParam == WMSZ_TOP)
				how = 2;
			else if(wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT)
				how = 1;
			else if(wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT
				|| wParam == WMSZ_TOPRIGHT || wParam == WMSZ_TOPLEFT)
				how = 3;
			if(how & 1)
				winsizemulx*= (double)w/winwidth;
			if(how & 2)
				winsizemuly*= (double)h/winheight;

			bool shift_held = (GetAsyncKeyState(VK_SHIFT) < 0);
			if(how & 1)
				FixWXY(0, shift_held);
			else
				FixWXY(1, shift_held);

			CalcWindowSize(&srect);
			winwidth=srect.right;
			winheight=srect.bottom;
			wrect->right = wrect->left + srect.right;
			wrect->bottom = wrect->top + srect.bottom;
		
			GetMenuItemRect(hWnd, fceumenu, 0, &file_rect);
			GetMenuItemRect(hWnd, fceumenu, 5, &help_rect);
			menuYoffset = help_rect.top-file_rect.top;
		}
		//sizchange=1;
		//break;
		goto proco;
	case WM_DISPLAYCHANGE:
		if(!fullscreen && !changerecursive)
			vchanged=1;
		goto proco;
	case WM_SYSCOMMAND:
		if(GameInfo && wParam == SC_SCREENSAVE && (goptions & GOO_DISABLESS))
			return(0);

		//adelikat: If we are going to disable screensave, we should disable monitor off as well
		if(GameInfo && wParam == SC_MONITORPOWER && (goptions & GOO_DISABLESS))
			return(0);

		if(wParam==SC_KEYMENU)
		{
			if(GameInfo && ((InputType[2]==SIFC_FKB) || (InputType[2]==SIFC_SUBORKB) || (InputType[2]==SIFC_PEC586KB)) && cidisabled)
				break;
			if(lParam == VK_RETURN || fullscreen || tog) break;
		}
		goto proco;
	case WM_SYSKEYDOWN:
		if(GameInfo && ((InputType[2]==SIFC_FKB) || (InputType[2]==SIFC_SUBORKB) || (InputType[2]==SIFC_PEC586KB)) && cidisabled)
			break; // Hopefully this won't break DInput...

		if(fullscreen || tog)
		{
			if(wParam==VK_MENU)
				break;
		}

		if(wParam==VK_F10)
			break;			// 11.12.08 CH4 Disable F10 as System Key dammit
/*
		if(wParam == VK_RETURN)
		{
			if(!(lParam&(1<<30)))
			{
				UpdateCheckedMenuItems();
				changerecursive=1;
				if(!SetVideoMode(fullscreen^1))
					SetVideoMode(fullscreen);
				changerecursive=0;
			}
			break;
		}
adelikat: Outsourced this to a remappable hotkey
*/
		goto proco;

	case WM_TIMER:
	{
		uint8 devicenum;
		uint16 buttonnum;
		GUID guid;

		static bool just_pressed = true;

		if (DoJoyWaitTest(&guid, &devicenum, &buttonnum))
		{
			if (just_pressed)
			{
				if (config_menu.IsAwaitingKey())
				{
					if (config_menu.GetState() == ConfigMenuState::RemapPlayer1 ||
						config_menu.GetState() == ConfigMenuState::RemapPlayer2)
					{
						size_t const player_idx =
							config_menu.GetState() == ConfigMenuState::RemapPlayer1 ? 0 : 1;

						ButtConfig * bc = &GetGamePadConfig(player_idx)[size_t(config_menu.GetCurrentRemapOption())];

						bc->NumC = 2;
						bc->ButtType[1] = BUTTC_JOYSTICK;
						bc->DeviceNum[1] = devicenum;
						bc->ButtonNum[1] = buttonnum;
						bc->DeviceInstance[1] = guid;

						config_menu.StopAwaitingKey();
					}
				}
				else
				{
					// Joystick up or direction up.
					if (buttonnum == 49153 || buttonnum == 8192)
					{
						config_menu.PreviousOption();
					}

					// Joystick down or direction down.
					else if (buttonnum == 32769 || buttonnum == 8194)
					{
						config_menu.NextOption();
					}

					// Button A.
					else if (buttonnum == 0)
					{
						config_menu.ConfirmOption();
					}

					// Button B.
					else if (buttonnum == 1)
					{
						config_menu.Hide();
					}
				}
			}

			just_pressed = false;
		}
		else
		{
			just_pressed = true;
		}

		break;
	}

	case WM_KEYUP:
	{
        // No input while the splash screen is showing.
        if (splash_screen.IsShowing() == false)
        {
		    if (config_menu.IsShowing() && config_menu.IsAwaitingKey())
		    {
			    config_menu.StopAwaitingKey();

			    if (config_menu.GetState() == ConfigMenuState::RemapPlayer1 ||
				    config_menu.GetState() == ConfigMenuState::RemapPlayer2)
			    {
				    LPARAM key_code = (((lParam >> 16) & 0x7F) | ((lParam >> 17) & 0x80)) & 255;

				    size_t const player_idx =
					    config_menu.GetState() == ConfigMenuState::RemapPlayer1 ? 0 : 1;

				    ButtConfig * bc = &GetGamePadConfig(player_idx)[size_t(config_menu.GetCurrentRemapOption())];
				    bc->NumC = std::max<uint32>(1, bc->NumC);
				    bc->ButtType[0] = BUTTC_KEYBOARD;
				    bc->DeviceNum[0] = 0;
				    bc->ButtonNum[0] = key_code;
			    }
		    }
		    else if (wParam == VK_ESCAPE)
		    {
			    if (config_menu.IsShowing())
			    {
				    config_menu.Hide();
			    }
			    else
			    {
				    config_menu.Show();

				    // Start timer to listen for gamepad input.
				    SetTimer(GetMainHWND(), 1, 25, 0); // Fire timer 1 every 25 ms (generates WM_TIMER message).
			    }
		    }
		    else if (config_menu.IsShowing())
		    {
			    if (wParam == VK_RETURN)
			    {
				    config_menu.ConfirmOption();
			    }
			    else if (wParam == VK_DOWN)
			    {
				    config_menu.NextOption();
			    }
			    else if (wParam == VK_UP)
			    {
				    config_menu.PreviousOption();
			    }
		    }
        }

		goto proco;
	}

	case WM_KEYDOWN:
	{
		if (config_menu.IsShowing() == false && GameInfo)
		{
			//Only disable command keys if a game is loaded(and the other conditions are right, of course).
			if ((InputType[2] == SIFC_FKB) || (InputType[2] == SIFC_SUBORKB) || (InputType[2] == SIFC_PEC586KB))
			{
				if (wParam == VK_SCROLL)
				{
					cidisabled ^= 1;
					FCEUI_DispMessage("%s Keyboard %sabled.", 0, InputType[2] == SIFC_FKB ? "Family" : (InputType[2] == SIFC_SUBORKB ? "Subor" : "PEC586"), cidisabled ? "en" : "dis");
				}
				if (cidisabled)
					break; // Hopefully this won't break DInput...
			}
		}

		goto proco;
	}

	case WM_CLOSE:
	case WM_DESTROY:
	case WM_QUIT:
		DoFCEUExit();
		break;
	case WM_SETFOCUS:
		nofocus = 0;
		if (wasPausedByCheats)
		{
			EmulationPaused = 0;
			wasPausedByCheats = false;
		}
		break;
	case WM_ACTIVATEAPP:
		if((BOOL)wParam)
			nofocus = 0;
		else
			nofocus = 1;
		goto proco;
	case WM_ENTERMENULOOP:
		UpdateCheckedMenuItems();
		EnableMenuItem(fceumenu,MENU_RESET,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_RESET)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_POWER,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_POWER)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_EJECT_DISK,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_EJECT_DISK)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_SWITCH_DISK,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_SWITCH_DISK)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_INSERT_COIN,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_INSERT_COIN)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_INPUT_BARCODE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_INPUT_BARCODE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_TASEDITOR,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_TASEDITOR)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_CLOSE_FILE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_CLOSEGAME) && GameInfo ?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_RECENT_FILES,MF_BYCOMMAND | ((FCEU_IsValidUI(FCEUI_OPENGAME) && HasRecentFiles()) ?MF_ENABLED:MF_GRAYED)); //adelikat - added && recent_files, otherwise this line prevents recent from ever being gray when TAS Editor is not engaged
		EnableMenuItem(fceumenu,MENU_OPEN_FILE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_OPENGAME)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_RECORD_MOVIE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_RECORDMOVIE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_REPLAY_MOVIE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_PLAYMOVIE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_MOVIE_RECENT,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_PLAYMOVIE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_STOP_MOVIE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_STOPMOVIE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,ID_FILE_PLAYMOVIEFROMBEGINNING,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_PLAYFROMBEGINNING)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu, ID_FILE_TOGGLE_RECORDING_MOVIE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_TOGGLERECORDINGMOVIE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu, ID_FILE_INSERT_1_FRAME, MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_INSERT1FRAME) ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(fceumenu, ID_FILE_DELETE_1_FRAME, MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_DELETE1FRAME) ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(fceumenu, ID_FILE_TRUNCATE_MOVIE, MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_TRUNCATEMOVIE) ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_SAVESTATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_QUICKSAVE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_LOADSTATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_QUICKLOAD)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_SAVE_STATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_SAVESTATE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_LOAD_STATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_LOADSTATE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_NEXTSAVESTATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_NEXTSAVESTATE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_PREVIOUSSAVESTATE,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_PREVIOUSSAVESTATE)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_VIEWSAVESLOTS,MF_BYCOMMAND | (FCEU_IsValidUI(FCEUI_VIEWSLOTS)?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_STOP_AVI,MF_BYCOMMAND | (FCEUI_AviIsRecording()?MF_ENABLED:MF_GRAYED));
		EnableMenuItem(fceumenu,MENU_STOP_WAV,MF_BYCOMMAND | (loggingSound?MF_ENABLED:MF_GRAYED));
		// PAL and PPU should not be changed while a movie is recorded/played
		if (FCEUMOV_Mode(MOVIEMODE_INACTIVE))
		{
			EnableMenuItem(fceumenu, MENU_PAL, MF_ENABLED);
			EnableMenuItem(fceumenu, ID_NEWPPU, MF_ENABLED);
			EnableMenuItem(fceumenu, ID_OLDPPU, MF_ENABLED);
		} else
		{
			EnableMenuItem(fceumenu, MENU_PAL, MF_GRAYED);
			EnableMenuItem(fceumenu, ID_NEWPPU, MF_GRAYED);
			EnableMenuItem(fceumenu, ID_OLDPPU, MF_GRAYED);
		}
		CheckMenuRadioItem(fceumenu, ID_NEWPPU, ID_OLDPPU, newppu ? ID_NEWPPU : ID_OLDPPU, MF_BYCOMMAND);
		// when TASEditor is engaged, some settings should not be changeable
		if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		{
			EnableMenuItem(fceumenu, MENU_ENABLE_AUTOSAVE, MF_GRAYED);
			EnableMenuItem(fceumenu, ID_ENABLE_BACKUPSAVESTATES, MF_GRAYED);
			EnableMenuItem(fceumenu, ID_ENABLE_COMPRESSSAVESTATES, MF_GRAYED);
		} else
		{
			EnableMenuItem(fceumenu, MENU_ENABLE_AUTOSAVE, MF_ENABLED);
			EnableMenuItem(fceumenu, ID_ENABLE_BACKUPSAVESTATES, MF_ENABLED);
			EnableMenuItem(fceumenu, ID_ENABLE_COMPRESSSAVESTATES, MF_ENABLED);
		}

	default:
proco:
		return DefWindowProc(hWnd,msg,wParam,lParam);
	}
	return 0;
}

void FixWXY(int pref, bool shift_held)
{
	if (eoptions & EO_FORCEASPECT)
	{
		if (pref == 0)
			winsizemuly = winsizemulx;
		else
			winsizemulx = winsizemuly;
	}
	if (winsizemulx < 0.1)
		winsizemulx = 0.1;
	if (winsizemuly < 0.1)
		winsizemuly = 0.1;

	// round to integer values
	if (((eoptions & EO_FORCEISCALE) && !shift_held) || (!(eoptions & EO_FORCEISCALE) && shift_held))
	{
		int x,y;

		x = winsizemulx * 2;
		y = winsizemuly * 2;

		x = (x>>1) + (x&1);
		y = (y>>1) + (y&1);

		if(!x) x=1;
		if(!y) y=1;

		winsizemulx = x;
		winsizemuly = y;    
	}

	/*
	// is this really necessary?
	if (winsizemulx > 100)
		winsizemulx = 100;
	if (winsizemuly > 100)
		winsizemuly = 100;
	*/
}

void UpdateFCEUWindow(void)
{
	if(vchanged && !fullscreen && !changerecursive && !nofocus)
	{
		SetVideoMode(0);
		vchanged = 0;
	}

	BlockingCheck();

	if(!(eoptions & EO_BGRUN))
	{
		while(nofocus)
		{

			Sleep(75);
			BlockingCheck();
		}
	}
}


//Destroys the main window
void ByebyeWindow()
{
	SetMenu(hAppWnd, 0);
	DestroyMenu(fceumenu);
	DestroyWindow(hAppWnd);
}

/// reates the main window.
/// @return Flag that indicates failure (0) or success (1)
int CreateMainWindow()
{
	WNDCLASSEX winclass;
	RECT tmp;

	// Create and register the window class

	memset(&winclass, 0, sizeof(winclass));
	winclass.cbSize = sizeof(WNDCLASSEX);
	winclass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;	// AnS: removed CS_SAVEBITS
	winclass.lpfnWndProc = AppWndProc;
	winclass.cbClsExtra = 0;
	winclass.cbWndExtra = 0;
	winclass.hInstance = fceu_hInstance;
	winclass.hIcon = 0; // LoadIcon(fceu_hInstance, "ICON_1");
	winclass.hIconSm = 0; // LoadIcon(fceu_hInstance, "ICON_1");
	winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	winclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); //mbg merge 7/17/06 added cast
	winclass.lpszClassName = "FCEUXWindowClass";

	if(!RegisterClassEx(&winclass))
	{
		return FALSE;
	}

	AdjustWindowRectEx(&tmp, WS_OVERLAPPEDWINDOW, 1, 0);

	//fceumenu = LoadMenu(fceu_hInstance,"FCEUMENU");
	//UpdateMenuHotkeys(FCEUMENU_MAIN);
	recentmenu = CreateMenu();
	recentluamenu = CreateMenu();
	recentmoviemenu = CreateMenu();

	// Update recent files menu
	UpdateRMenu(recentmenu, recent_files, MENU_RECENT_FILES, MENU_FIRST_RECENT_FILE);
	UpdateLuaRMenu(recentluamenu, recent_lua, MENU_LUA_RECENT, LUA_FIRST_RECENT_FILE);
	UpdateMovieRMenu(recentmoviemenu, recent_movie, MENU_MOVIE_RECENT, MOVIE_FIRST_RECENT_FILE);

	updateGameDependentMenus();
	if (MainWindow_wndx==-32000) MainWindow_wndx=0; //Just in case
	if (MainWindow_wndy==-32000) MainWindow_wndy=0;

	hAppWnd = CreateWindowEx(
		0,
		"FCEUXWindowClass",
		FCEU_NAME_AND_VERSION,
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS,  /* Style */
		MainWindow_wndx,
		MainWindow_wndy,
		256,
		FSettings.TotalScanlines(),  /* X,Y ; Width, Height */
		NULL,
		fceumenu,
		fceu_hInstance,
		NULL);

	//DragAcceptFiles(hAppWnd, 1);

	SetMainWindowStuff();

	CenterWindowOnScreen(hAppWnd);

	return 1;
}

void SetMainWindowStuff()
{
	RECT tmp;

	GetWindowRect(hAppWnd, &tmp);

	if (ismaximized)
	{
		winwidth = tmp.right - tmp.left;
		winheight = tmp.bottom - tmp.top;

		ShowWindow(hAppWnd, SW_SHOWMAXIMIZED);
	}
	else
	{
		RECT srect;

		if(WindowXC != ( 1 << 30 ))
		{
			/* Subtracting and adding for if(eoptions&EO_USERFORCE) below. */
			tmp.bottom -= tmp.top;
			tmp.bottom += WindowYC;

			tmp.right -= tmp.left;
			tmp.right += WindowXC;


			tmp.left = WindowXC;
			tmp.top = WindowYC;
			WindowXC = 1 << 30;
		}

		CalcWindowSize(&srect);

		SetWindowPos(
			hAppWnd,
			HWND_TOP,
			tmp.left,
			tmp.top,
			srect.right,
			srect.bottom,
			SWP_SHOWWINDOW
			);

		winwidth = srect.right;
		winheight = srect.bottom;

		ShowWindow(hAppWnd, SW_SHOWNORMAL);
	}

	if (eoptions & EO_BESTFIT && !windowedfailed)
	{
		RECT client_recr;
		GetClientRect(hAppWnd, &client_recr);
		recalculateBestFitRect(client_recr.right - client_recr.left, client_recr.bottom - client_recr.top);
	}
}

/// @return Flag that indicates failure (0) or success (1).
int GetClientAbsRect(LPRECT lpRect)
{
	POINT point;
	point.x = point.y = 0;

	if(!ClientToScreen(hAppWnd, &point))
	{
		return 0;
	}

	lpRect->top = point.y;
	lpRect->left = point.x;

	//if(ismaximized)
	//{
		RECT al;
		GetClientRect(hAppWnd, &al);
		lpRect->right = point.x + al.right;
		lpRect->bottom = point.y + al.bottom;
	//}
	//else
	//{
	//	lpRect->right = point.x + VNSWID * winsizemulx;
	//	lpRect->bottom = point.y + FSettings.TotalScanlines() * winsizemuly;
	//}
	return 1;
}

//Shows an Open File menu and starts recording an AVI
void FCEUD_AviRecordTo(void)
{
	OPENFILENAME ofn;
	char szChoice[MAX_PATH];

	string tempFilename, aviFilename;
		
	std::string aviDirectory = FCEU_GetPath(21);  //21 = FCEUMKF_AVI
	if (aviDirectory.find_last_of("\\") != (aviDirectory.size()-1))
		aviDirectory.append("\\");			//if directory override has no \ then add one

	//if we are playing a movie, construct the filename from the current movie.
	if(FCEUMOV_Mode(MOVIEMODE_PLAY|MOVIEMODE_RECORD)) //adelikat: TOOD: Think about this.  I think MOVIEMODE_FINISHED shouldn't not be included here.  Am I wrong?
	{
		tempFilename = GetMfn();	//get movie filename
		tempFilename.erase(0,1);	//remove dot
	}
	//else construct it from the ROM name.
	else
		tempFilename = mass_replace(GetRomName(), "|", ".").c_str();
	
	aviFilename = aviDirectory + tempFilename;	//concate avi directory and movie filename
				
	strcpy(szChoice, aviFilename.c_str());
	char* dot = strrchr(szChoice,'.');

	if (dot) *dot='\0';
	strcat(szChoice, ".avi");

	// avi record file browser
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hAppWnd;
	ofn.lpstrFilter = "AVI Files (*.avi)\0*.avi\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = szChoice;
	ofn.lpstrDefExt = "avi";
	ofn.lpstrTitle = "Save AVI as";

	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
		FCEUI_AviBegin(szChoice);
}

//Stop AVI recording
void FCEUD_AviStop(void)
{
	FCEUI_AviEnd();
}

void FCEUD_CmdOpen(void)
{
	LoadNewGamey(hAppWnd, 0);
}

bool FCEUD_PauseAfterPlayback()
{
	return false;
}

void ChangeContextMenuItemText(int menuitem, string text, HMENU menu)
{
	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_TYPE;
	moo.cch = NULL;
	GetMenuItemInfo(menu, menuitem, FALSE, &moo);
	moo.dwTypeData = (LPSTR)text.c_str();
	SetMenuItemInfo(menu, menuitem, FALSE, &moo);
}

void ChangeMenuItemText(int menuitem, string text)
{
	MENUITEMINFO moo;
	moo.cbSize = sizeof(moo);
	moo.fMask = MIIM_TYPE;
	moo.cch = NULL;
	GetMenuItemInfo(fceumenu, menuitem, FALSE, &moo);
	moo.dwTypeData = (LPSTR)text.c_str();
	SetMenuItemInfo(fceumenu, menuitem, FALSE, &moo);
}

string HOTKEYMENUINDEX::getQualifiedMenuText(FCEUMENU_INDEX menu_index) {
	HMENU menu;
	GetHMENU(menu_index, menu);
	int length = GetMenuString(menu, menu_id, 0, 0, flags) + 1;
	char* buffer = new char[length];
	GetMenuString(menu, menu_id, buffer, length, flags);
	if (char* pTab = strrchr(buffer, '\t'))
		*pTab = '\0';
	std::string menustr = HOTKEYMENUINDEX::getQualifiedMenuText(buffer, cmd_id);
	delete[] buffer;
	return menustr;
}

string HOTKEYMENUINDEX::getQualifiedMenuText(char* text, int emu_cmd_id) {
	string menustr;
	return menustr;
}

struct HOTKEYMENUINDEX hotkeyMenuIndexes[] = {
	// "&Open..."
	{ MENU_OPEN_FILE,EMUCMD_OPENROM },
	// "Open ROM"
	{ FCEU_CONTEXT_OPENROM,EMUCMD_OPENROM,MENU_BELONG(CONTEXT_OFF) },
	// "&Close"
	{ MENU_CLOSE_FILE,EMUCMD_CLOSEROM },
	// "Close ROM"
	{ FCEU_CONTEXT_CLOSEROM,EMUCMD_CLOSEROM,MENU_BELONG(CONTEXT_GAME) },
	// "&Load State"
	{ MENU_LOADSTATE,EMUCMD_LOAD_STATE },
	// "&Save State"
	{ MENU_SAVESTATE,EMUCMD_SAVE_STATE },
	// "Load State &From..."
	{ MENU_LOAD_STATE,EMUCMD_LOAD_STATE_FROM },
	// "Save State &As..."
	{ MENU_SAVE_STATE,EMUCMD_SAVE_STATE_AS },
	// "&Next save slot"
	{ MENU_NEXTSAVESTATE,EMUCMD_SAVE_SLOT_NEXT },
	// "&Previous save slot"
	{ MENU_PREVIOUSSAVESTATE,EMUCMD_SAVE_SLOT_PREV },
	// "&View save slots"
	{ MENU_VIEWSAVESLOTS,EMUCMD_MISC_SHOWSTATES },
	// "Record Movie..."
	{ FCEUX_CONTEXT_RECORDMOVIE,EMUCMD_MOVIE_RECORD_TO, MENU_BELONG(CONTEXT_GAME) },
	// "&Record Movie..."
	{ MENU_RECORD_MOVIE,EMUCMD_MOVIE_RECORD_TO },
	// "Play Movie..."
	{ FCEUX_CONTEXT_REPLAYMOVIE,EMUCMD_MOVIE_REPLAY_FROM,MENU_BELONG(CONTEXT_GAME) },
	// "&Play Movie..."
	{ MENU_REPLAY_MOVIE,EMUCMD_MOVIE_REPLAY_FROM },
	// "&Stop Movie"
	{ MENU_STOP_MOVIE, EMUCMD_MOVIE_STOP },
	// "Stop Movie Replay" (Playing)
	// "Stop Movie Recording" (Recording)
	{ FCEU_CONTEXT_STOPMOVIE,EMUCMD_MOVIE_STOP,BELONG_CONTEXT_MOVIE_ALL },
	// "Play Movie from Beginning"
	{ FCEU_CONTEXT_PLAYMOVIEFROMBEGINNING,EMUCMD_MOVIE_PLAY_FROM_BEGINNING,BELONG_CONTEXT_MOVIE_ALL },
	// "Play from &Beginning"
	{ ID_FILE_PLAYMOVIEFROMBEGINNING,EMUCMD_MOVIE_PLAY_FROM_BEGINNING },
	// "&Read-only"
	{ ID_FILE_MOVIE_TOGGLEREAD,EMUCMD_MOVIE_READONLY_TOGGLE },
	// "Toggle to read+write" (Readonly)
	// "Toggle to Read-only" (Readwrite)
	{ FCEUX_CONTEXT_READONLYTOGGLE,EMUCMD_MOVIE_READONLY_TOGGLE,BELONG_CONTEXT_MOVIE_ALL },
	// "&Screenshot"
	{ ID_FILE_SCREENSHOT,EMUCMD_SCREENSHOT },
	// "Screenshot"
	{ FCEUX_CONTEXT_SCREENSHOT,EMUCMD_SCREENSHOT, MENU_BELONG(CONTEXT_GAME) },
	// "&Record AVI..."
	{ MENU_RECORD_AVI,EMUCMD_AVI_RECORD_AS },
	//"&Stop AVI"
	{ MENU_STOP_AVI,EMUCMD_AVI_STOP },
	// "&Reset"
	{ MENU_RESET,EMUCMD_RESET },
	// "&Power"
	{ MENU_POWER,EMUCMD_POWER },
	// "&Eject/Insert Disk"
	{ MENU_EJECT_DISK,EMUCMD_FDS_EJECT_INSERT },
	// "&Switch Disk Side"
	{ MENU_SWITCH_DISK,EMUCMD_FDS_SIDE_SELECT },
	// "&Insert Coin"
	{ MENU_INSERT_COIN,EMUCMD_VSUNI_COIN },
	// "Speed &Up"
	{ ID_NES_SPEEDUP,EMUCMD_SPEED_FASTER },
	// "Slow &Down"
	{ ID_NES_SLOWDOWN,EMUCMD_SPEED_SLOWER },
	// "&Slowest Speed"
	{ ID_NES_SLOWESTSPEED,EMUCMD_SPEED_SLOWEST },
	// "&Normal Speed"
	{ ID_NES_NORMALSPEED,EMUCMD_SPEED_NORMAL },
	// "&Turbo"
	{ ID_NES_TURBO,EMUCMD_SPEED_TURBO_TOGGLE },
	// "&Pause"
	{ ID_NES_PAUSE,EMUCMD_PAUSE },
	// "&Hide Menu"
	{ MENU_HIDE_MENU,EMUCMD_HIDE_MENU_TOGGLE },
	// "Unhide Menu"
	{ FCEUX_CONTEXT_UNHIDEMENU,EMUCMD_HIDE_MENU_TOGGLE,BELONG_CONTEXT_RUNNING },
	// "&Frame Adv. - Skip Lag"
	{ MENU_DISPLAY_FA_LAGSKIP,EMUCMD_FRAMEADV_SKIPLAG },
	// "&Lag Counter"
	{ MENU_DISPLAY_LAGCOUNTER,EMUCMD_MISC_DISPLAY_LAGCOUNTER_TOGGLE },
	// "&Frame Counter"
	{ ID_DISPLAY_FRAMECOUNTER,EMUCMD_MOVIE_FRAME_DISPLAY_TOGGLE },
	// "&Movie status icon"
	{ ID_DISPLAY_MOVIESTATUSICON,EMUCMD_MOVIE_ICON_DISPLAY_TOGGLE },
	// "FPS"
	{ ID_DISPLAY_FPS,EMUCMD_FPS_DISPLAY_TOGGLE },
	// "Graphics: &BG"
	{ MENU_DISPLAY_BG,EMUCMD_MISC_DISPLAY_BG_TOGGLE },
	// "Graphics: &OBJ"
	{ MENU_DISPLAY_OBJ,EMUCMD_MISC_DISPLAY_OBJ_TOGGLE },
	// "&Cheats..."
	{ MENU_CHEATS,EMUCMD_TOOL_OPENCHEATS },
	// "&Memory Watch..."
	{ MENU_MEMORY_WATCH,EMUCMD_TOOL_OPENMEMORYWATCH },
	// "&Debugger..."
	{ MENU_DEBUGGER,EMUCMD_TOOL_OPENDEBUGGER },
	// "&PPU Viewer..."
	{ MENU_PPUVIEWER,EMUCMD_TOOL_OPENPPU },
	// "&Name Table Viewer..."
	{ MENU_NAMETABLEVIEWER,EMUCMD_TOOL_OPENNTVIEW },
	// "&Hex Editor..."
	{ MENU_HEXEDITOR,EMUCMD_TOOL_OPENHEX },
	// "&Trace Logger..."
	{ MENU_TRACELOGGER,EMUCMD_TOOL_OPENTRACELOGGER },
	// "&Code/Data Logger..."
	{ MENU_CDLOGGER,EMUCMD_TOOL_OPENCDLOGGER },
	// "Undo savestate"
	{ FCEUX_CONTEXT_UNDOSAVESTATE,EMUCMD_MISC_UNDOREDOSAVESTATE,BELONG_CONTEXT_RUNNING },
	// "Rewind to last auto-save"
	{ FCEUX_CONTEXT_REWINDTOLASTAUTO,EMUCMD_MISC_AUTOSAVE,BELONG_CONTEXT_RUNNING },
	// "Toggle to Recording" (Playing)
	// "Toggle to Playing" (Recording)
	{ FCEUX_CONTEXT_TOGGLE_RECORDING,EMUCMD_MOVIE_TOGGLE_RECORDING,BELONG_CONTEXT_MOVIE_ALL },
	// "&Toggle Recording/Playing"
	{ ID_FILE_TOGGLE_RECORDING_MOVIE,EMUCMD_MOVIE_TOGGLE_RECORDING },
	// "&Truncate at Current Frame"
	{ ID_FILE_TRUNCATE_MOVIE,EMUCMD_MOVIE_TRUNCATE },
	// "Truncate at Current Frame"
	{ FCEUX_CONTEXT_TRUNCATE_MOVIE,EMUCMD_MOVIE_TRUNCATE,BELONG_CONTEXT_MOVIE_ALL },
	// "&Insert 1 Frame"
	{ ID_FILE_INSERT_1_FRAME,EMUCMD_MOVIE_INSERT_1_FRAME },
	// "Insert 1 Frame"
	{ FCEUX_CONTEXT_INSERT_1_FRAME,EMUCMD_MOVIE_INSERT_1_FRAME,BELONG_CONTEXT_MOVIE_ALL },
	// "&Delete 1 Frame"
	{ ID_FILE_DELETE_1_FRAME,EMUCMD_MOVIE_DELETE_1_FRAME },
	// "Delete 1 Frame"
	{ FCEUX_CONTEXT_DELETE_1_FRAME,EMUCMD_MOVIE_DELETE_1_FRAME,BELONG_CONTEXT_MOVIE_ALL },
	// "&Next Record Mode"
	{ ID_FILE_NEXTRECORDMODE,EMUCMD_MOVIE_NEXT_RECORD_MODE },
	// "&Truncate
	{ ID_FILE_RECORDMODE_TRUNCATE,EMUCMD_MOVIE_RECORD_MODE_TRUNCATE },
	// "&Truncate"
	{ FCEUX_CONTEXT_RECORDMODE_TRUNCATE,EMUCMD_MOVIE_RECORD_MODE_TRUNCATE,BELONG_CONTEXT_MOVIE_ALL },
	// "&Overwrite[W]"
	{ ID_FILE_RECORDMODE_OVERWRITE,EMUCMD_MOVIE_RECORD_MODE_OVERWRITE },
	// "&Overwrite[W]"
	{ FCEUX_CONTEXT_RECORDMODE_OVERWRITE,EMUCMD_MOVIE_RECORD_MODE_OVERWRITE,BELONG_CONTEXT_MOVIE_ALL },
	// "&Insert[I]"
	{ ID_FILE_RECORDMODE_INSERT,EMUCMD_MOVIE_RECORD_MODE_INSERT },
	// "&Insert[I]"
	{ FCEUX_CONTEXT_RECORDMODE_INSERT,EMUCMD_MOVIE_RECORD_MODE_INSERT,BELONG_CONTEXT_MOVIE_ALL },
	// This is a special one for submenus of "Display Input", its matching hotkey is variable on the fly to the next mode
	{ 0, EMUCMD_MOVIE_INPUT_DISPLAY_TOGGLE, MENU_BELONG(MAIN_INPUTDISPLAY) | MENU_BELONG(MAIN)}
};

void HOTKEYMENUINDEX::updateMenuText(FCEUMENU_INDEX index) {
	if (IsMenuBelongsTo(index, hmenu_ident))
	{
		HMENU hmenu;
		GetHMENU(index, hmenu);
		ModifyMenu(hmenu, menu_id, flags | GetMenuState(hmenu, menu_id, flags), menu_id, getQualifiedMenuText(index).c_str());
	}
}

// Push the window away from the main FCEUX window
POINT CalcSubWindowPos(HWND hDlg, POINT* conf)
{
	POINT pt; // dialog position
	RECT wR, dR; // Window rect, dialog rect


	// Try to calc the default position, it doesn't overlap the main window and ensure it's in the screen;
	GetWindowRect(hAppWnd, &wR);
	GetWindowRect(hDlg, &dR);

	pt.x = wR.left;
	pt.y = wR.top;

	LONG wW = wR.right - wR.left; // window width
	LONG dW = dR.right - dR.left; // dialog width

	if (pt.x + wW + dW < GetSystemMetrics(SM_CXSCREEN))
		pt.x += wW; // if there is enough place for the dialog on the right, put the dialog there
	else if (pt.x - dW > 0)
		pt.x -= dW; // otherwise, we check if we can put it on the left

	// If the dialog has a configured window position, override the default position
	if (conf)
	{
		LONG wH = wR.bottom - wR.top;
		// It is overrided only when the configured position is not completely off screen
		if (conf->x > -wW * 2 || conf->x < wW * 2 + GetSystemMetrics(SM_CXSCREEN))
			pt.x = conf->x;
		if (conf->y > -wH * 2 || conf->y < wH * 2 + GetSystemMetrics(SM_CYSCREEN))
			pt.y = conf->y;
	}

	// finally set the window position
	SetWindowPos(hDlg, NULL, pt.x, pt.y, NULL, NULL, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

	// return the calculated point, maybe the caller can use it for further.
	return pt;
}

LRESULT APIENTRY FilterEditCtrlProc(HWND hwnd, UINT msg, WPARAM wP, LPARAM lP)
{
	bool through = true;
	INT_PTR result = 0;

	switch (msg)
	{
		case WM_PASTE:
		{

			bool (*IsLetterLegal)(char) = GetIsLetterLegal(GetDlgCtrlID(hwnd));

			if (IsLetterLegal)
			{
				if (OpenClipboard(hwnd))
				{
					HANDLE handle = GetClipboardData(CF_TEXT);
					if (handle)
					{

						// get the original clipboard string
						char* clipStr = (char*)GlobalLock(handle);

						// check if the text in clipboard has illegal characters
						int len = strlen(clipStr);
						for (int i = 0; i < len; ++i)
						{
							if (!IsLetterLegal(clipStr[i]))
							{
								through = false;
								// Show Edit control tip, just like the control with ES_NUMBER do
								ShowLetterIllegalBalloonTip(hwnd, IsLetterLegal);
								break;
							}
						}
						GlobalUnlock(handle);
						CloseClipboard();
					}
				}
			}
		}
		break;
		case WM_CHAR:
		{
			bool(*IsLetterLegal)(char) = GetIsLetterLegal(GetDlgCtrlID(hwnd));
			through = IsInputLegal(IsLetterLegal, wP);
			if (!through)
				ShowLetterIllegalBalloonTip(hwnd, IsLetterLegal);
		}
	}

	return through ? CallWindowProc(DefaultEditCtrlProc, hwnd, msg, wP, lP) : result;
}

// return a letter legal checking function for the specified control with the given id
bool inline (*GetIsLetterLegal(UINT id))(char letter)
{
	switch (id)
	{

		// Game genie text in Cheat and Game Genie Encoder/Decoder
		case IDC_CHEAT_GAME_GENIE_TEXT:
		case IDC_GAME_GENIE_CODE:
			return IsLetterLegalGG;
		
		// Addresses in Debugger
		case IDC_DEBUGGER_VAL_PCSEEK:
		case IDC_DEBUGGER_VAL_PC:
		case IDC_DEBUGGER_VAL_A:
		case IDC_DEBUGGER_VAL_X:
		case IDC_DEBUGGER_VAL_Y:
		case IDC_DEBUGGER_BOOKMARK:

		// Debugger -> Add breakpoint
		case IDC_ADDBP_ADDR_START: case IDC_ADDBP_ADDR_END:

		// Address, Value, Compare, Known Value, Note equal, Greater than and Less than in Cheat
		case IDC_CHEAT_ADDR: case IDC_CHEAT_VAL: case IDC_CHEAT_COM:
		case IDC_CHEAT_VAL_KNOWN: case IDC_CHEAT_VAL_NE_BY:
		case IDC_CHEAT_VAL_GT_BY: case IDC_CHEAT_VAL_LT_BY:

		// Address, Value and Compare in Game Genie Encoder/Decoder
		case IDC_GAME_GENIE_ADDR: case IDC_GAME_GENIE_VAL: case IDC_GAME_GENIE_COMP:

		// Address controls in Memory watch
		case MW_ADDR00: case MW_ADDR01: case MW_ADDR02: case MW_ADDR03:
		case MW_ADDR04: case MW_ADDR05: case MW_ADDR06: case MW_ADDR07:
		case MW_ADDR08: case MW_ADDR09: case MW_ADDR10: case MW_ADDR11:
		case MW_ADDR12: case MW_ADDR13: case MW_ADDR14: case MW_ADDR15:
		case MW_ADDR16: case MW_ADDR17: case MW_ADDR18: case MW_ADDR19:
		case MW_ADDR20: case MW_ADDR21: case MW_ADDR22: case MW_ADDR23:
		case IDC_EDIT_COMPAREADDRESS:

			return IsLetterLegalHex;

		// Specific Address in RAM Search
		// RAM Watch / RAM Search / Cheat -> Add watch (current only in adding watch operation)
		case IDC_EDIT_COMPAREADDRESSES:
			return IsLetterLegalHexList;

		// Size multiplier and TV Aspect in Video Config
		case IDC_WINSIZE_MUL_X: case IDC_WINSIZE_MUL_Y:
		case IDC_TVASPECT_X: case IDC_TVASPECT_Y:
			return IsLetterLegalFloat;

		// Cheat code in Cheat
		case IDC_CHEAT_TEXT:
			return IsLetterLegalCheat;

		// PRG ROM, PRG RAM, PRG NVRAM, CHR ROM, CHR RAM and CHR NVRAM in iNES Header Editor
		case IDC_PRGROM_EDIT: case IDC_PRGRAM_EDIT: case IDC_PRGNVRAM_EDIT:
		case IDC_CHRROM_EDIT: case IDC_CHRRAM_EDIT: case IDC_CHRNVRAM_EDIT:
			return IsLetterLegalSize;

		// Specific value, Different by and Modulo in RAM search
		case IDC_EDIT_COMPAREVALUE:
		case IDC_EDIT_DIFFBY:
		case IDC_EDIT_MODBY:
		{
		}
	}
	return NULL;
}

void ShowLetterIllegalBalloonTip(HWND hwnd, bool(*IsLetterLegal)(char letter))
{
	wchar_t* title = L"Unacceptable Character";
	wchar_t* msg = GetLetterIllegalErrMsg(IsLetterLegal);

	EDITBALLOONTIP tip;
	tip.cbStruct = sizeof(EDITBALLOONTIP);
	tip.pszText = msg;
	tip.pszTitle = title;
	tip.ttiIcon = TTI_ERROR;
	SendMessage(hwnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&tip);

	// make a sound
	MessageBeep(0xFFFFFFFF);
}

inline wchar_t* GetLetterIllegalErrMsg(bool(*IsLetterLegal)(char letter))
{
	if (IsLetterLegal == IsLetterLegalGG)
		return L"You can only type Game Genie characters:\nA P Z L G I T Y E O X U K S V N";
	if (IsLetterLegal == IsLetterLegalHex)
		return L"You can only type characters for hexadecimal number (0-9,A-F).";
	if (IsLetterLegal == IsLetterLegalHexList)
		return L"You can only type characters for hexademical number (0-9,A-F), each number is separated by a comma (,)";
	if (IsLetterLegal == IsLetterLegalCheat)
		return
		L"The cheat code comes into the following 2 formats:\n"
		"AAAA:VV freezes the value in Address $AAAA to $VV.\n"
		"AAAA?CC:VV changes the value in Address $AAAA to $VV only when it's $CC.\n"
		"All the characters are hexadecimal number (0-9,A-F).\n";
	if (IsLetterLegal == IsLetterLegalFloat)
		return L"You can only type decimal number (decimal point is acceptable).";
	if (IsLetterLegal == IsLetterLegalSize)
		return L"You can only type decimal number followed with B, KB or MB.";
	if (IsLetterLegal == IsLetterLegalDec)
		return L"You can only type decimal number (sign character is acceptable).";
	if (IsLetterLegal == IsLetterLegalDecHexMixed)
		return
		L"You can only type decimal or hexademical number\n"
		"(sign character is acceptable).\n\n"
		"When your number contains letter A-F,\n"
		"it is regarded as hexademical number,\n"
		"however, if you want to express a heademical number\n"
		"but all the digits are in 0-9,\n"
		"you must add a $ prefix to prevent ambiguous.\n"
		"eg. 10 is a decimal number,\n"
		"$10 means a hexademical number that is 16 in decimal.";
	if (IsLetterLegal == IsLetterLegalUnsignedDecHexMixed)
		return
		L"You can only type decimal or hexademical number.\n\n"
		"When your number contains letter A-F,\n"
		"it is regarded as hexademical number,\n"
		"however, if you want to express a heademical number\n"
		"but all the digits are in 0-9,\n"
		"you must add a $ prefix to prevent ambiguous.\n"
		"eg. 10 is a decimal number,\n"
		"$10 means a hexademical number that is 16 in decimal.";

	return L"Your input contains invalid characters.";
}

inline bool IsInputLegal(bool (*IsLetterLegal)(char letter), char letter)
{
	return !IsLetterLegal || letter == VK_BACK || GetKeyState(VK_CONTROL) & 0x8000 || IsLetterLegal(letter);
}

inline bool IsLetterLegalGG(char letter)
{
	return false;
}

inline bool IsLetterLegalHex(char letter)
{
	return letter >= '0' && letter <= '9' || letter >= 'A' && letter <= 'F' || letter >= 'a' && letter <= 'f';
}

inline bool IsLetterLegalHexList(char letter)
{
	return IsLetterLegalHex(letter) || letter == ',' || letter == ' ';
}

inline bool IsLetterLegalCheat(char letter)
{
	return letter >= '0' && letter <= ':' || letter >= 'A' && letter <= 'F' || letter >= 'a' && letter <= 'f' || letter == '?';
}

inline bool IsLetterLegalSize(char letter)
{
	return letter >= '0' && letter <= '9' || letter == 'm' || letter == 'M' || letter == 'k' || letter == 'K' || letter == 'b' || letter == 'B';
}

inline bool IsLetterLegalDec(char letter)
{
	return letter >= '0' && letter <= '9' || letter == '-' || letter == '+';
}

inline bool IsLetterLegalFloat(char letter)
{
	return letter >= '0' && letter <= '9' || letter == '.' || letter == '-' || letter == '+';
}

inline bool IsLetterLegalDecHexMixed(char letter)
{
	return letter >= '0' && letter <= '9' || letter >= 'A' && letter <= 'F' || letter >= 'a' && letter <= 'f' || letter == '$' || letter == '-' || letter == '+';
}

inline bool IsLetterLegalUnsignedDecHexMixed(char letter)
{
	return letter >= '0' && letter <= '9' || letter >= 'A' && letter <= 'F' || letter >= 'a' && letter <= 'f' || letter == '$';
}
