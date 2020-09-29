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

#include "config.h"
#include "common.h"
#include "main.h"
#include "window.h"
#include "video.h"
#include "fceu.h"
#include "file.h"

#include "../../state.h"	//adelikat: For bool backupSavestates

extern CFGSTRUCT NetplayConfig[];
extern CFGSTRUCT InputConfig[];
extern CFGSTRUCT HotkeyConfig[];
extern int autoHoldKey, autoHoldClearKey;
extern int frameAdvance_Delay;
extern int EnableAutosave, AutosaveQty, AutosaveFrequency;
extern int AFon, AFoff, AutoFireOffset;
extern int DesynchAutoFire;
extern bool lagCounterDisplay;
extern bool frameAdvanceLagSkip;
extern int ClipSidesOffset;
extern bool movieSubtitles;
extern bool subtitlesOnAVI;
extern bool autoMovieBackup;
extern bool bindSavestate;
extern int PPUViewRefresh;
extern int NTViewRefresh;
extern uint8 gNoBGFillColor;
extern bool rightClickEnabled;
extern bool fullscreenByDoubleclick;
extern int CurrentState;
extern int globalCheatDisabled;
extern int disableAutoLSCheats;
extern bool enableHUDrecording;
extern bool disableMovieMessages;
extern bool replaceP2StartWithMicrophone;
extern bool SingleInstanceOnly;
extern bool Show_FPS;
extern int movieRecordMode;
extern bool oldInputDisplay;
extern bool fullSaveStateLoads;
extern int frameSkipAmt;
extern int32 fps_scale_frameadvance;
extern int palnotch;
extern int palsaturation;
extern int palsharpness;
extern int palcontrast;
extern int palbrightness;
extern bool paldeemphswap;
extern int RAMInitOption;
extern int RAMInitSeed;

// Hacky fix for taseditor_config.last_author and rom_name_when_closing_emulator
char* taseditorConfigLastAuthorName;
char* ResumeROM;

//window positions and sizes:
extern int DbgSizeX,DbgSizeY;
extern int NTViewPosX,NTViewPosY;
extern int PPUViewPosX, PPUViewPosY;
extern bool PPUView_maskUnusedGraphics;
extern bool PPUView_invertTheMask;
extern int PPUView_sprite16Mode;
extern int MainWindow_wndx, MainWindow_wndy;
extern int MemWatch_wndx, MemWatch_wndy;
extern int Monitor_wndx, Monitor_wndy;
extern int logging_options;
extern int Tracer_wndWidth, Tracer_wndHeight;
extern int CDLogger_wndx, CDLogger_wndy;
extern bool autosaveCDL;
extern bool autoloadCDL;
extern int GGConv_wndx, GGConv_wndy;
extern int MLogPosX,MLogPosY;

extern int HexRowHeightBorder;
extern int HexBackColorR;
extern int HexBackColorG;
extern int HexBackColorB;
extern int HexForeColorR;
extern int HexForeColorG;
extern int HexForeColorB;
extern int HexFreezeColorR;
extern int HexFreezeColorG;
extern int HexFreezeColorB;
extern int RomFreezeColorR;
extern int RomFreezeColorG;
extern int RomFreezeColorB;
extern int HexBoundColorR;
extern int HexBoundColorG;
extern int HexBoundColorB;

//adelikat:  Hacky fix for Ram Watch recent menu
char* ramWatchRecent[] = {0, 0, 0, 0, 0};

//Structure that contains configuration information
static CFGSTRUCT fceuconfig[] =
{
	ACS(recent_files[0]),
	ACS(recent_files[1]),
	ACS(recent_files[2]),
	ACS(recent_files[3]),
	ACS(recent_files[4]),
	ACS(recent_files[5]),
	ACS(recent_files[6]),
	ACS(recent_files[7]),
	ACS(recent_files[8]),
	ACS(recent_files[9]),

	ACS(recent_lua[0]),
	ACS(recent_lua[1]),
	ACS(recent_lua[2]),
	ACS(recent_lua[3]),
	ACS(recent_lua[4]),

	ACS(recent_movie[0]),
	ACS(recent_movie[1]),
	ACS(recent_movie[2]),
	ACS(recent_movie[3]),
	ACS(recent_movie[4]),

	ACS(ramWatchRecent[0]),
	ACS(ramWatchRecent[1]),
	ACS(ramWatchRecent[2]),
	ACS(ramWatchRecent[3]),
	ACS(ramWatchRecent[4]),

	AC(AutoResumePlay),
	ACS(ResumeROM),

	AC(gNoBGFillColor),
	AC(ntsccol_enable),AC(ntsctint),AC(ntschue),
	AC(force_grayscale),
	AC(dendy),
	AC(RAMInitOption),
	AC(RAMInitSeed),
	AC(postrenderscanlines),
	AC(vblankscanlines),
	AC(overclock_enabled),
	AC(skip_7bit_overclocking),
	AC(palnotch),
	AC(palsaturation),
	AC(palsharpness),
	AC(palcontrast),
	AC(palbrightness),
	AC(paldeemphswap),

	NAC("palyo",pal_emulation),
	NAC("genie",genie),
	NAC("fs",fullscreen),
	NAC("vgamode",vmod),
	NAC("sound",soundo),
	NAC("sicon",status_icon),

	AC(newppu),

	NACS("odroms",directory_names[0]),
	NACS("odnonvol",directory_names[1]),
	NACS("odstates",directory_names[2]),
	NACS("odfdsrom",directory_names[3]),
	NACS("odsnaps",directory_names[4]),
	NACS("odcheats",directory_names[5]),
	NACS("odmovies",directory_names[6]),
	NACS("odmemwatch",directory_names[7]),
	NACS("odmacro",directory_names[9]),
	NACS("odinput",directory_names[10]),
	NACS("odlua",directory_names[11]),
	NACS("odavi",directory_names[12]),
	NACS("odbase",directory_names[13]),

	AC(winspecial),
	AC(NTSCwinspecial),
	AC(winsizemulx),
	AC(winsizemuly),
	AC(tvAspectX),
	AC(tvAspectY),

	AC(soundrate),
	AC(soundbuftime),
	AC(soundoptions),
	AC(soundquality),
	AC(soundvolume),
	AC(soundTrianglevol),
	AC(soundSquare1vol),
	AC(soundSquare2vol),
	AC(soundNoisevol),
	AC(soundPCMvol),
	AC(muteTurbo),
	AC(swapDuty),

	AC(goptions),
	NAC("eoptions",eoptions),
	NACA("cpalette",cpalette),
	NAC("cpalette_count",cpalette_count),

	NACA("InputType",InputType),

	NAC("vmcx",vmodes[0].x),
	NAC("vmcy",vmodes[0].y),
	NAC("vmcb",vmodes[0].bpp),
	NAC("vmcf",vmodes[0].flags),
	NAC("vmcxs",vmodes[0].xscale),
	NAC("vmcys",vmodes[0].yscale),
	NAC("vmspecial",vmodes[0].special),

	NAC("srendline",srendlinen),
	NAC("erendline",erendlinen),
	NAC("srendlinep",srendlinep),
	NAC("erendlinep",erendlinep),

	AC(directDrawModeWindowed),
	AC(directDrawModeFullscreen),
	AC(winsync),
	NAC("988fssync",fssync),

	AC(ismaximized),
	AC(maxconbskip),
	AC(ffbskip),

	ADDCFGSTRUCT(NetplayConfig),
	ADDCFGSTRUCT(InputConfig),
	ADDCFGSTRUCT(HotkeyConfig),

	AC(autoHoldKey),
	AC(autoHoldClearKey),
	AC(frame_display),
	AC(rerecord_display),
	AC(input_display),
	AC(EnableBackgroundInput),
	AC(frameAdvance_Delay),
	AC(EnableAutosave),
	AC(AutosaveQty),
	AC(AutosaveFrequency),
	AC(frameAdvanceLagSkip),
	AC(allowUDLR),
	AC(fullSaveStateLoads),
	AC(frameSkipAmt),
	AC(fps_scale_frameadvance),

	//window positions
	AC(NTViewPosX),
	AC(NTViewPosY),
	AC(PPUViewPosX),
	AC(PPUViewPosY),
	AC(PPUView_maskUnusedGraphics),
	AC(PPUView_invertTheMask),
	AC(PPUView_sprite16Mode),
	AC(MainWindow_wndx),
	AC(MainWindow_wndy),
	AC(MLogPosX),
	AC(MLogPosY),
	AC(AFon),
	AC(AFoff),
	AC(AutoFireOffset),
	AC(DesynchAutoFire),
	ACS(taseditorConfigLastAuthorName),
	AC(lagCounterDisplay),
	AC(oldInputDisplay),
	AC(movieSubtitles),
	AC(subtitlesOnAVI),
	AC(bindSavestate),
	AC(autoMovieBackup),
	AC(ClipSidesOffset),
	AC(PPUViewRefresh),
	AC(NTViewRefresh),
	AC(rightClickEnabled),
	AC(fullscreenByDoubleclick),
	AC(CurrentState),
	//ACS(memwLastfile[2048]),
	AC(backupSavestates),
	AC(compressSavestates),
	AC(disableAutoLSCheats),
	AC(globalCheatDisabled),
	AC(enableHUDrecording),
	AC(disableMovieMessages),
	AC(replaceP2StartWithMicrophone),
	AC(SingleInstanceOnly),
	AC(Show_FPS),
	AC(movieRecordMode),

	ENDCFGSTRUCT
};

void SaveConfig(const char *filename)
{
	////adelikat: Hacky fix for Ram Watch recent menu
	//for (int x = 0; x < 5; x++)
	//{
	//	ramWatchRecent[x] = rw_recent_files[x];
	//}
	//// Hacky fix for taseditor_config.last_author and rom_name_when_closing_emulator
	//taseditorConfigLastAuthorName = taseditorConfig.lastAuthorName;
	//ResumeROM = romNameWhenClosingEmulator;
	////-----------------------------------

	//SaveFCEUConfig(filename,fceuconfig);
}

void LoadConfig(const char *filename)
{
	FCEUI_GetNTSCTH(&ntsctint, &ntschue);

	LoadFCEUConfig(filename, fceuconfig);

	FCEUI_SetNTSCTH(ntsccol_enable, ntsctint, ntschue);

	if (ResumeROM)
		strcpy(romNameWhenClosingEmulator, ResumeROM);
	else
		romNameWhenClosingEmulator[0] = 0;

	//-----------------------------------
}

