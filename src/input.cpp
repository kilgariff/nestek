/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 1998 BERO
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

#include "types.h"
#include "x6502.h"

#include "fceu.h"
#include "sound.h"
#include "netplay.h"
#include "movie.h"
#include "state.h"
#include "input/zapper.h"
#include "input.h"
#include "vsuni.h"
#include "fds.h"
#include "driver.h"

#ifdef WIN32
#include "drivers/win/main.h"
#include "drivers/win/ppuview.h"
#include "drivers/win/window.h"
#include "drivers/win/ntview.h"

#include <string>
#include <ostream>
#include <cstring>

extern bool mustRewindNow;
#endif // WIN32

//it is easier to declare these input drivers extern here than include a bunch of files
//-------------
extern INPUTC *FCEU_InitZapper(int w);
extern INPUTC *FCEU_InitPowerpadA(int w);
extern INPUTC *FCEU_InitPowerpadB(int w);
extern INPUTC *FCEU_InitArkanoid(int w);
extern INPUTC *FCEU_InitMouse(int w);
extern INPUTC *FCEU_InitSNESMouse(int w);
extern INPUTC *FCEU_InitVirtualBoy(int w);

extern INPUTCFC *FCEU_InitArkanoidFC(void);
extern INPUTCFC *FCEU_InitSpaceShadow(void);
extern INPUTCFC *FCEU_InitFKB(void);
extern INPUTCFC *FCEU_InitSuborKB(void);
extern INPUTCFC *FCEU_InitPEC586KB(void);
extern INPUTCFC *FCEU_InitHS(void);
extern INPUTCFC *FCEU_InitMahjong(void);
extern INPUTCFC *FCEU_InitQuizKing(void);
extern INPUTCFC *FCEU_InitFamilyTrainerA(void);
extern INPUTCFC *FCEU_InitFamilyTrainerB(void);
extern INPUTCFC *FCEU_InitOekaKids(void);
extern INPUTCFC *FCEU_InitTopRider(void);
extern INPUTCFC *FCEU_InitFamiNetSys(void);
extern INPUTCFC *FCEU_InitBarcodeWorld(void);
//---------------

//global lag variables
unsigned int lagCounter;
bool lagCounterDisplay;
char lagFlag;
extern bool frameAdvanceLagSkip;
extern bool movieSubtitles;
//-------------

static uint8 joy_readbit[2];
uint8 joy[4]={0,0,0,0}; //HACK - should be static but movie needs it
uint16 snesjoy[4]={0,0,0,0}; //HACK - should be static but movie needs it
static uint8 LastStrobe;
uint8 RawReg4016 = 0; // Joystick strobe (W)

bool replaceP2StartWithMicrophone = false;

//This function is a quick hack to get the NSF player to use emulated gamepad input.
uint8 FCEU_GetJoyJoy(void)
{
	return(joy[0]|joy[1]|joy[2]|joy[3]);
}

extern uint8 coinon;

//set to true if the fourscore is attached
static bool FSAttached = false;

JOYPORT joyports[2] = { JOYPORT(0), JOYPORT(1) };
FCPORT portFC;

FILE* DumpInputFile;
FILE* PlayInputFile;

static DECLFR(JPRead)
{
	lagFlag = 0;
	uint8 ret=0;
	static bool microphone = false;

	ret|=joyports[A&1].driver->Read(A&1);

	// Test if the port 2 start button is being pressed.
	// On a famicom, port 2 start shouldn't exist, so this removes it.
	// Games can't automatically be checked for NES/Famicom status,
	// so it's an all-encompassing change in the input config menu.
	if ((replaceP2StartWithMicrophone) && (A&1) && (joy_readbit[1] == 4)) {
	// Nullify Port 2 Start Button
	ret&=0xFE;
	}

	if(portFC.driver)
		ret = portFC.driver->Read(A&1,ret);

	// Not verified against hardware.
	if (replaceP2StartWithMicrophone) {
		if (joy[1]&8) {
			microphone = !microphone;
			if (microphone) {
				ret|=4;
			}
		} else {
			microphone = false;
		}
	}

	if(PlayInputFile)
		ret = fgetc(PlayInputFile);

	if(DumpInputFile)
		fputc(ret,DumpInputFile);

	ret|=X.DB&0xC0;

	return(ret);
}

static DECLFW(B4016)
{
	if(portFC.driver)
		portFC.driver->Write(V&7);

	for(int i=0;i<2;i++)
		joyports[i].driver->Write(V&1);

	if((LastStrobe&1) && (!(V&1)))
	{
		//old comment:
		//This strobe code is just for convenience.  If it were
		//with the code in input / *.c, it would more accurately represent
		//what's really going on.  But who wants accuracy? ;)
		//Seriously, though, this shouldn't be a problem.
		//new comment:

		//mbg 6/7/08 - I guess he means that the input drivers could track the strobing themselves
		//I dont see why it is unreasonable here.
		for(int i=0;i<2;i++)
			joyports[i].driver->Strobe(i);
		if(portFC.driver)
			portFC.driver->Strobe();
	}
	LastStrobe=V&0x1;
	RawReg4016 = V;
}

//a main joystick port driver representing the case where nothing is plugged in
static INPUTC DummyJPort={0};
//and an expansion port driver for the same ting
static INPUTCFC DummyPortFC={0};


//--------4 player driver for expansion port--------
static uint8 F4ReadBit[2];
static void StrobeFami4(void)
{
	F4ReadBit[0]=F4ReadBit[1]=0;
}

static uint8 ReadFami4(int w, uint8 ret)
{
	ret&=1;

	ret |= ((joy[2+w]>>(F4ReadBit[w]))&1)<<1;
	if(F4ReadBit[w]>=8) ret|=2;
	else F4ReadBit[w]++;

	return(ret);
}

static INPUTCFC FAMI4C={ReadFami4,0,StrobeFami4,0,0,0};
//------------------

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


static uint8 ReadGPVS(int w)
{
	uint8 ret=0;

	if(joy_readbit[w]>=8)
		ret=1;
	else
	{
		ret = ((joy[w]>>(joy_readbit[w]))&1);
		if(!fceuindbg)
			joy_readbit[w]++;
	}
	return ret;
}

static void UpdateGP(int w, void *data, int arg)
{
	if(w==0)	//adelikat, 3/14/09: Changing the joypads to inclusive OR the user's joypad + the Lua joypad, this way lua only takes over the buttons it explicity says to
	{			//FatRatKnight: Assume lua is always good. If it's doing nothing in particular using my logic, it'll pass-through the values anyway.
		joy[0] = *(uint32 *)joyports[0].ptr;;
		joy[2] = *(uint32 *)joyports[0].ptr >> 16;
	}
	else
	{
		joy[1] = *(uint32 *)joyports[1].ptr >> 8;
		joy[3] = *(uint32 *)joyports[1].ptr >> 24;
	}

}

static void LogGP(int w, MovieRecord* mr)
{
	if(w==0)
	{
		mr->joysticks[0] = joy[0];
		mr->joysticks[2] = joy[2];
	}
	else
	{
		mr->joysticks[1] = joy[1];
		mr->joysticks[3] = joy[3];
	}
}

static void LoadGP(int w, MovieRecord* mr)
{
	if(w==0)
	{
		joy[0] = mr->joysticks[0];
		if(FSAttached) joy[2] = mr->joysticks[2];
	}
	else
	{
		joy[1] = mr->joysticks[1];
		if(FSAttached) joy[3] = mr->joysticks[3];
	}
}


//basic joystick port driver
static uint8 ReadGP(int w)
{
	uint8 ret;

	if(joy_readbit[w]>=8)
		ret = ((joy[2+w]>>(joy_readbit[w]&7))&1);
	else
		ret = ((joy[w]>>(joy_readbit[w]))&1);
	if(joy_readbit[w]>=16) ret=0;
	if(!FSAttached)
	{
		if(joy_readbit[w]>=8) ret|=1;
	}
	else
	{
		if(joy_readbit[w]==19-w) ret|=1;
	}
	if(!fceuindbg)
		joy_readbit[w]++;
	return ret;
}

static void StrobeGP(int w)
{
	joy_readbit[w]=0;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

//SNES pad

static void UpdateSNES(int w, void *data, int arg)
{
	//LUA NOT SUPPORTED YET
	if(w==0)
	{
		snesjoy[0]= ((uint32 *)joyports[0].ptr)[0];
		snesjoy[2]= ((uint32 *)joyports[0].ptr)[2];
	}
	else
	{
		snesjoy[1] = ((uint32 *)joyports[0].ptr)[1];
		snesjoy[3] = ((uint32 *)joyports[0].ptr)[3];
	}

}

static void LogSNES(int w, MovieRecord* mr)
{
	//not supported for SNES pad right noe
}

static void LoadSNES(int w, MovieRecord* mr)
{
	//not supported for SNES pad right now
}


static uint8 ReadSNES(int w)
{
	//no fourscore support on snes (not clear how it would work)

	uint8 ret;

	if(joy_readbit[w]>=16)
		ret = 1;
	else
	{
		ret = ((snesjoy[w]>>(joy_readbit[w]))&1);
	}
	if(!fceuindbg)
		joy_readbit[w]++;
	return ret;
}

static void StrobeSNES(int w)
{
	joy_readbit[w]=0;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^



static INPUTC GPC={ReadGP,0,StrobeGP,UpdateGP,0,0,LogGP,LoadGP};
static INPUTC GPCVS={ReadGPVS,0,StrobeGP,UpdateGP,0,0,LogGP,LoadGP};
static INPUTC GPSNES={ReadSNES,0,StrobeSNES,UpdateSNES,0,0,LogSNES,LoadSNES};

void FCEU_DrawInput(uint8 *buf)
{
	for(int pad=0;pad<2;pad++)
		joyports[pad].driver->Draw(pad,buf,joyports[pad].attrib);
	if(portFC.driver)
		portFC.driver->Draw(buf,portFC.attrib);
}


void FCEU_UpdateInput(void)
{
	//tell all drivers to poll input and set up their logical states
	if(!FCEUMOV_Mode(MOVIEMODE_PLAY))
	{
		for(int port=0;port<2;port++){
			joyports[port].driver->Update(port,joyports[port].ptr,joyports[port].attrib);
		}
		portFC.driver->Update(portFC.ptr,portFC.attrib);
	}

	if(GameInfo->type==GIT_VSUNI)
		if(coinon) coinon--;

	if(FCEUnetplay)
		NetplayUpdate(joy);

	FCEUMOV_AddInputState();

	//TODO - should this apply to the movie data? should this be displayed in the input hud?
	if(GameInfo->type==GIT_VSUNI){
		FCEU_VSUniSwap(&joy[0],&joy[1]);
	}
}

static DECLFR(VSUNIRead0)
{
	lagFlag = 0;
	uint8 ret=0;

	ret|=(joyports[0].driver->Read(0))&1;

	ret|=(vsdip&3)<<3;
	if(coinon)
		ret|=0x4;
	return ret;
}

static DECLFR(VSUNIRead1)
{
	lagFlag = 0;
	uint8 ret=0;

	ret|=(joyports[1].driver->Read(1))&1;
	ret|=vsdip&0xFC;
	return ret;
}



//calls from the ppu;
//calls the SLHook for any driver that needs it
void InputScanlineHook(uint8 *bg, uint8 *spr, uint32 linets, int final)
{
	for(int port=0;port<2;port++)
		joyports[port].driver->SLHook(port,bg,spr,linets,final);
	portFC.driver->SLHook(bg,spr,linets,final);
}

#include <iostream>
//binds JPorts[pad] to the driver specified in JPType[pad]
static void SetInputStuff(int port)
{
	switch(joyports[port].type)
	{
	case SI_GAMEPAD:
		if(GameInfo->type==GIT_VSUNI){
			joyports[port].driver = &GPCVS;
		} else {
			joyports[port].driver= &GPC;
		}
		break;
	case SI_SNES:
		joyports[port].driver= &GPSNES;
		break;
	case SI_ARKANOID:
		joyports[port].driver=FCEU_InitArkanoid(port);
		break;
	case SI_ZAPPER:
		joyports[port].driver=FCEU_InitZapper(port);
		break;
	case SI_POWERPADA:
		joyports[port].driver=FCEU_InitPowerpadA(port);
		break;
	case SI_POWERPADB:
		joyports[port].driver=FCEU_InitPowerpadB(port);
		break;
	case SI_MOUSE:
		joyports[port].driver=FCEU_InitMouse(port);
		break;
	case SI_SNES_MOUSE:
		joyports[port].driver=FCEU_InitSNESMouse(port);
		break;
	case SI_VIRTUALBOY:
		joyports[port].driver=FCEU_InitVirtualBoy(port);
		break;
	case SI_NONE:
	case SI_UNSET:
		joyports[port].driver=&DummyJPort;
		break;
	}
}

static void SetInputStuffFC()
{
	switch(portFC.type)
	{
	case SIFC_NONE:
	case SIFC_UNSET:
		portFC.driver=&DummyPortFC;
		break;
	case SIFC_ARKANOID:
		portFC.driver=FCEU_InitArkanoidFC();
		break;
	case SIFC_SHADOW:
		portFC.driver=FCEU_InitSpaceShadow();
		break;
	case SIFC_OEKAKIDS:
		portFC.driver=FCEU_InitOekaKids();
		break;
	case SIFC_4PLAYER:
		portFC.driver=&FAMI4C;
		memset(&F4ReadBit,0,sizeof(F4ReadBit));
		break;
	case SIFC_FKB:
		portFC.driver=FCEU_InitFKB();
		break;
	case SIFC_SUBORKB:
		portFC.driver=FCEU_InitSuborKB();
		break;
	case SIFC_PEC586KB:
		portFC.driver=FCEU_InitPEC586KB();
		break;
	case SIFC_HYPERSHOT:
		portFC.driver=FCEU_InitHS();
		break;
	case SIFC_MAHJONG:
		portFC.driver=FCEU_InitMahjong();
		break;
	case SIFC_QUIZKING:
		portFC.driver=FCEU_InitQuizKing();
		break;
	case SIFC_FTRAINERA:
		portFC.driver=FCEU_InitFamilyTrainerA();
		break;
	case SIFC_FTRAINERB:
		portFC.driver=FCEU_InitFamilyTrainerB();
		break;
	case SIFC_BWORLD:
		portFC.driver=FCEU_InitBarcodeWorld();
		break;
	case SIFC_TOPRIDER:
		portFC.driver=FCEU_InitTopRider();
		break;
	case SIFC_FAMINETSYS:
		portFC.driver = FCEU_InitFamiNetSys();
		break;
	}
}

void FCEUI_SetInput(int port, ESI type, void *ptr, int attrib)
{
	joyports[port].attrib = attrib;
	joyports[port].type = type;
	joyports[port].ptr = ptr;
	SetInputStuff(port);
}

void FCEUI_SetInputFC(ESIFC type, void *ptr, int attrib)
{
	portFC.attrib = attrib;
	portFC.type = type;
	portFC.ptr = ptr;
	SetInputStuffFC();
}


//initializes the input system to power-on state
void InitializeInput(void)
{
	memset(joy_readbit,0,sizeof(joy_readbit));
	memset(joy,0,sizeof(joy));
	LastStrobe = 0;

	if(GameInfo->type==GIT_VSUNI)
	{
		SetReadHandler(0x4016,0x4016,VSUNIRead0);
		SetReadHandler(0x4017,0x4017,VSUNIRead1);
	}
	else
		SetReadHandler(0x4016,0x4017,JPRead);

	SetWriteHandler(0x4016,0x4016,B4016);

	//force the port drivers to be setup
	SetInputStuff(0);
	SetInputStuff(1);
	SetInputStuffFC();
}


bool FCEUI_GetInputFourscore()
{
	return FSAttached;
}
bool FCEUI_GetInputMicrophone()
{
	return replaceP2StartWithMicrophone;
}
void FCEUI_SetInputFourscore(bool attachFourscore)
{
	FSAttached = attachFourscore;
}

//mbg 6/18/08 HACK
extern ZAPPER ZD[2];
SFORMAT FCEUCTRL_STATEINFO[]={
	{ joy_readbit,	2, "JYRB"},
	{ joy,			4, "JOYS"},
	{ &LastStrobe,	1, "LSTS"},
	{ &ZD[0].bogo,	1, "ZBG0"},
	{ &ZD[1].bogo,	1, "ZBG1"},
	{ &lagFlag,		1, "LAGF"},
	{ &lagCounter,	4, "LAGC"},
	{ &currFrameCounter, 4, "FRAM"},
	{ 0 }
};

void FCEU_DoSimpleCommand(int cmd)
{
	switch(cmd)
	{
	case FCEUNPCMD_FDSINSERT: FCEU_FDSInsert();break;
	case FCEUNPCMD_FDSSELECT: FCEU_FDSSelect();break;
	case FCEUNPCMD_VSUNICOIN: FCEU_VSUniCoin(); break;
	case FCEUNPCMD_VSUNIDIP0:
	case FCEUNPCMD_VSUNIDIP0+1:
	case FCEUNPCMD_VSUNIDIP0+2:
	case FCEUNPCMD_VSUNIDIP0+3:
	case FCEUNPCMD_VSUNIDIP0+4:
	case FCEUNPCMD_VSUNIDIP0+5:
	case FCEUNPCMD_VSUNIDIP0+6:
	case FCEUNPCMD_VSUNIDIP0+7:	FCEU_VSUniToggleDIP(cmd - FCEUNPCMD_VSUNIDIP0);break;
	case FCEUNPCMD_POWER: PowerNES();break;
	case FCEUNPCMD_RESET: ResetNES();break;
	}
}

void FCEU_QSimpleCommand(int cmd)
{
	if(FCEUnetplay)
		FCEUNET_SendCommand(cmd, 0);
	else
	{
		if(!FCEUMOV_Mode(MOVIEMODE_TASEDITOR))		// TAS Editor will do the command himself
			FCEU_DoSimpleCommand(cmd);
		if(FCEUMOV_Mode(MOVIEMODE_RECORD|MOVIEMODE_TASEDITOR))
			FCEUMOV_AddCommand(cmd);
	}
}

void FCEUI_FDSSelect(void)
{
	if(!FCEU_IsValidUI(FCEUI_SWITCH_DISK))
		return;

	FCEU_DispMessage("Command: Switch disk side", 0);
	FCEU_QSimpleCommand(FCEUNPCMD_FDSSELECT);
}

void FCEUI_FDSInsert(void)
{
	if(!FCEU_IsValidUI(FCEUI_EJECT_DISK))
		return;

	FCEU_DispMessage("Command: Insert/Eject disk", 0);
	FCEU_QSimpleCommand(FCEUNPCMD_FDSINSERT);
}

void FCEUI_VSUniToggleDIP(int w)
{
	FCEU_QSimpleCommand(FCEUNPCMD_VSUNIDIP0 + w);
}

void FCEUI_VSUniCoin(void)
{
	if(!FCEU_IsValidUI(FCEUI_INSERT_COIN))
		return;

	FCEU_QSimpleCommand(FCEUNPCMD_VSUNICOIN);
}

//Resets the frame counter if movie inactive and rom is reset or power-cycle
void ResetFrameCounter()
{
extern EMOVIEMODE movieMode;
	if(movieMode == MOVIEMODE_INACTIVE)
		currFrameCounter = 0;
}

//Resets the NES
void FCEUI_ResetNES(void)
{
	if(!FCEU_IsValidUI(FCEUI_RESET))
		return;

	FCEU_DispMessage("Command: Soft reset", 0);
	FCEU_QSimpleCommand(FCEUNPCMD_RESET);
	ResetFrameCounter();
}

//Powers off the NES
void FCEUI_PowerNES(void)
{
	if(!FCEU_IsValidUI(FCEUI_POWER))
		return;

	FCEU_DispMessage("Command: Power switch", 0);
	FCEU_QSimpleCommand(FCEUNPCMD_POWER);
	ResetFrameCounter();
}

const char* FCEUI_CommandTypeNames[]=
{
	"Misc.",
	"Speed",
	"State",
	"Movie",
	"Sound",
	"AVI",
	"FDS",
	"VS Sys",
	"Tools",
	"TAS Editor",
};

//static void CommandUnImpl(void);
static void CommandToggleDip(void);
static void CommandStateLoad(void);
static void CommandStateSave(void);
static void CommandSelectSaveSlot(void);
static void CommandEmulationSpeed(void);
static void CommandSoundAdjust(void);
static void CommandUsePreset(void);
static void BackgroundDisplayToggle(void);
static void ObjectDisplayToggle(void);
static void ViewSlots(void);
static void LaunchTasEditor(void);
static void LaunchMemoryWatch(void);
static void LaunchCheats(void);
static void LaunchDebugger(void);
static void LaunchPPU(void);
static void LaunchNTView(void);
static void LaunchHex(void);
static void LaunchTraceLogger(void);
static void LaunchCodeDataLogger(void);
static void DebuggerStepInto(void);
static void FA_SkipLag(void);
static void OpenRom(void);
static void CloseRom(void);
void ReloadRom(void);
static void MovieSubtitleToggle(void);
static void UndoRedoSavestate(void);
static void FCEUI_DoExit(void);
void ToggleFullscreen();
static void TaseditorRewindOn(void);
static void TaseditorRewindOff(void);
static void TaseditorCommand(void);
extern void FCEUI_ToggleShowFPS();

struct EMUCMDTABLE FCEUI_CommandTable[]=
{
	{ EMUCMD_VSUNI_TOGGLE_DIP_0,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 0", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_1,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 1", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_2,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 2", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_3,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 3", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_4,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 4", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_5,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 5", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_6,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 6", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_7,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 7", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_8,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 8", 0 },
	{ EMUCMD_VSUNI_TOGGLE_DIP_9,			EMUCMDTYPE_VSUNI,	CommandToggleDip,				0, 0, "Toggle Dipswitch 9", 0 },
	{ EMUCMD_MISC_AUTOSAVE,					EMUCMDTYPE_MISC,	FCEUI_RewindToLastAutosave,		0, 0, "Load Last Auto-save", 0},
	{ EMUCMD_MISC_SHOWSTATES,				EMUCMDTYPE_MISC,	ViewSlots,						0, 0, "View save slots", 0 },
	{ EMUCMD_OPENROM,						EMUCMDTYPE_TOOL,	OpenRom,						0, 0, "Open ROM", 0},
	{ EMUCMD_CLOSEROM,						EMUCMDTYPE_TOOL,	CloseRom,						0, 0, "Close ROM", 0},
	{ EMUCMD_MISC_UNDOREDOSAVESTATE,		EMUCMDTYPE_MISC,	UndoRedoSavestate,				0, 0, "Undo/Redo Savestate", 0},
	{ EMUCMD_MISC_TOGGLEFULLSCREEN,			EMUCMDTYPE_MISC,	ToggleFullscreen,				0, 0, "Toggle Fullscreen",	0},
};

#define NUM_EMU_CMDS		(sizeof(FCEUI_CommandTable)/sizeof(FCEUI_CommandTable[0]))

static int execcmd, i;

void FCEUI_HandleEmuCommands(TestCommandState* testfn)
{
	bool taseditor = FCEUMOV_Mode(MOVIEMODE_TASEDITOR);
	for(i=0; i<NUM_EMU_CMDS; ++i)
	{
		int new_state;
		int old_state = FCEUI_CommandTable[i].state;
		execcmd = FCEUI_CommandTable[i].cmd;
		new_state = (*testfn)(execcmd);
		// in TAS Editor mode forbid commands without EMUCMDFLAG_TASEDITOR flag
		bool allow = true;

		if(allow)
		{
			if (new_state == 1 && old_state == 0 && FCEUI_CommandTable[i].fn_on)
				(*(FCEUI_CommandTable[i].fn_on))();
			else if (new_state == 0 && old_state == 1 && FCEUI_CommandTable[i].fn_off)
				(*(FCEUI_CommandTable[i].fn_off))();
		}
		FCEUI_CommandTable[i].state = new_state;
	}
}

// Function not currently used
//static void CommandUnImpl(void)
//{
//	FCEU_DispMessage("command '%s' unimplemented.",0, FCEUI_CommandTable[i].name);
//}

static void CommandToggleDip(void)
{
	if (GameInfo->type==GIT_VSUNI)
		FCEUI_VSUniToggleDIP(execcmd-EMUCMD_VSUNI_TOGGLE_DIP_0);
}

static void CommandEmulationSpeed(void)
{
	FCEUD_SetEmulationSpeed(EMUSPEED_SLOWEST+(execcmd-EMUCMD_SPEED_SLOWEST));
}

void FCEUI_SelectStateNext(int);

static void ViewSlots(void)
{
	FCEUI_SelectState(CurrentState, 1);
}

static void CommandSelectSaveSlot(void)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
	} else
	{
		if(execcmd <= EMUCMD_SAVE_SLOT_9)
			FCEUI_SelectState(execcmd - EMUCMD_SAVE_SLOT_0, 1);
		else if(execcmd == EMUCMD_SAVE_SLOT_NEXT)
			FCEUI_SelectStateNext(1);
		else if(execcmd == EMUCMD_SAVE_SLOT_PREV)
			FCEUI_SelectStateNext(-1);
	}
}

static void CommandStateSave(void)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
	} else
	{
		//	FCEU_PrintError("execcmd=%d, EMUCMD_SAVE_STATE_SLOT_0=%d, EMUCMD_SAVE_STATE_SLOT_9=%d", execcmd,EMUCMD_SAVE_STATE_SLOT_0,EMUCMD_SAVE_STATE_SLOT_9);
		if(execcmd >= EMUCMD_SAVE_STATE_SLOT_0 && execcmd <= EMUCMD_SAVE_STATE_SLOT_9)
		{
			int oldslot=FCEUI_SelectState(execcmd-EMUCMD_SAVE_STATE_SLOT_0, 0);
			FCEUI_SaveState(0);
			FCEUI_SelectState(oldslot, 0);
		}
		else
			FCEUI_SaveState(0);
	}
}

static void CommandStateLoad(void)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
	} else
	{
		if(execcmd >= EMUCMD_LOAD_STATE_SLOT_0 && execcmd <= EMUCMD_LOAD_STATE_SLOT_9)
		{
			int oldslot=FCEUI_SelectState(execcmd-EMUCMD_LOAD_STATE_SLOT_0, 0);
			FCEUI_LoadState(0);
			FCEUI_SelectState(oldslot, 0);
		}
		else
			FCEUI_LoadState(0);
	}
}

static void CommandSoundAdjust(void)
{
	int n=0;
	switch(execcmd)
	{
	case EMUCMD_SOUND_VOLUME_UP:		n=1;  break;
	case EMUCMD_SOUND_VOLUME_DOWN:		n=-1;  break;
	case EMUCMD_SOUND_VOLUME_NORMAL:	n=0;  break;
	}

	FCEUD_SoundVolumeAdjust(n);
}


static void CommandUsePreset(void)
{
	FCEUI_UseInputPreset(execcmd-EMUCMD_MISC_USE_INPUT_PRESET_1);
}

static void BackgroundDisplayToggle(void)
{
	bool spr, bg;
	FCEUI_GetRenderPlanes(spr,bg);
	bg = !bg;
	FCEUI_SetRenderPlanes(spr,bg);
}

static void ObjectDisplayToggle(void)
{
	bool spr, bg;
	FCEUI_GetRenderPlanes(spr,bg);
	spr = !spr;
	FCEUI_SetRenderPlanes(spr,bg);
}

void LagCounterReset()
{
	lagCounter = 0;
}

void LagCounterToggle(void)
{
	lagCounterDisplay ^= 1;
}

static void LaunchTasEditor(void)
{
#ifdef WIN32
	extern bool enterTASEditor();
	enterTASEditor();
#endif
}

static void LaunchMemoryWatch(void)
{
}

static void LaunchDebugger(void)
{
}

static void LaunchNTView(void)
{
#ifdef WIN32
	DoNTView();
#endif
}

static void LaunchPPU(void)
{
#ifdef WIN32
	DoPPUView();
#endif
}

static void LaunchHex(void)
{
}

static void LaunchTraceLogger(void)
{
}

static void LaunchCodeDataLogger(void)
{
}

static void LaunchCheats(void)
{
}

static void DebuggerStepInto()
{
#ifdef WIN32
	if (GameInfo)
	{
		extern void DoDebuggerStepInto();
		DoDebuggerStepInto();
	}
#endif
}

static void FA_SkipLag(void)
{
	frameAdvanceLagSkip ^= 1;
}

static void OpenRom(void)
{
#ifdef WIN32
	extern HWND hAppWnd;
	LoadNewGamey(hAppWnd, 0);
#endif
}

static void CloseRom(void)
{
#ifdef WIN32
	CloseGame();
#endif
}

void ReloadRom(void)
{
#ifdef WIN32
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{

	} else
	{
		// load most recent ROM
		extern void LoadRecentRom(int slot);
		LoadRecentRom(0);
	}
#endif
}

static void MovieSubtitleToggle(void)
{
	movieSubtitles ^= 1;
	if (movieSubtitles)	FCEU_DispMessage("Movie subtitles on",0);
	else FCEU_DispMessage("Movie subtitles off",0);
}

static void UndoRedoSavestate(void)
{
	// FIXME this will always evaluate to true, should this be
	// if (*lastSavestateMade...) to check if it holds a string or just
	// a '\0'?
	if (lastSavestateMade && (undoSS || redoSS))
		SwapSaveState();
}

static void FCEUI_DoExit(void)
{
#ifdef WIN32
	DoFCEUExit();
#endif
}

void ToggleFullscreen()
{
#ifdef WIN32
	extern int SetVideoMode(int fs);		//adelikat: Yeah, I know, hacky
	extern void UpdateCheckedMenuItems();

	UpdateCheckedMenuItems();
	changerecursive=1;

	int oldmode = fullscreen;
	if(!SetVideoMode(oldmode ^ 1))
		SetVideoMode(oldmode);
	changerecursive=0;
#endif
}
