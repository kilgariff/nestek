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


#include "types.h"
#include "file.h"
#include "utils/endian.h"
#include "netplay.h"
#include "fceu.h"
#include "state.h"
#include "cheat.h"
#include "input.h"
#include "driver.h"
#include "utils/memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h> //mbg merge 7/17/06 removed

int FCEUnetplay=0;

static uint8 netjoy[4]; // Controller cache.
static int numlocal;
static int netdivisor;
static int netdcount;

//NetError should only be called after a FCEUD_*Data function returned 0, in the function
//that called FCEUD_*Data, to prevent it from being called twice.

static void NetError(void)
{
	FCEU_DispMessage("Network error/connection lost!",0);
	FCEUD_NetworkClose();
}

void FCEUI_NetplayStop(void)
{
	if(FCEUnetplay)
	{
		FCEUnetplay = 0;
		FCEU_FlushGameCheats(0,1);  //Don't save netplay cheats.
		FCEU_LoadGameCheats(0);    //Reload our original cheats.
	}
	else puts("Check your code!");
}

int FCEUI_NetplayStart(int nlocal, int divisor)
{
	FCEU_FlushGameCheats(0, 0);  //Save our pre-netplay cheats.
	FCEU_LoadGameCheats(0);    // Load them again, for pre-multiplayer action.

	FCEUnetplay = 1;
	memset(netjoy,0,sizeof(netjoy));
	numlocal = nlocal;
	netdivisor = divisor;
	netdcount = 0;
	return(1);
}

int FCEUNET_SendCommand(uint8 cmd, uint32 len)
{
	//mbg merge 7/17/06 changed to alloca
	//uint8 buf[numlocal + 1 + 4];
	uint8 *buf = (uint8*)alloca(numlocal+1+4);


	buf[0] = 0xFF;
	FCEU_en32lsb(&buf[numlocal], len);
	buf[numlocal + 4] = cmd;
	if(!FCEUD_SendData(buf,numlocal + 1 + 4))
	{
		NetError();
		return(0);
	}
	return(1);
}

void FCEUI_NetplayText(uint8 *text)
{
	uint32 len;

	len = strlen((char*)text); //mbg merge 7/17/06 added cast

	if(!FCEUNET_SendCommand(FCEUNPCMD_TEXT,len)) return;

	if(!FCEUD_SendData(text,len))
		NetError();
}

int FCEUNET_SendFile(uint8 cmd, char *fn)
{
    // NOTE(ross): NESTEK doesn't need this.
    return 0;
}

static FILE *FetchFile(uint32 remlen)
{
    // NOTE(ross): NESTEK doesn't need this.
    return 0;
}

void NetplayUpdate(uint8 *joyp)
{
	static uint8 buf[5];  /* 4 play states, + command/extra byte */
	static uint8 joypb[4];

	memcpy(joypb,joyp,4);

	/* This shouldn't happen, but just in case.  0xFF is used as a command escape elsewhere. */
	if(joypb[0] == 0xFF)
		joypb[0] = 0xF;
	if(!netdcount)
		if(!FCEUD_SendData(joypb,numlocal))
		{
			NetError();
			return;
		}

	if(!netdcount)
	{
		do
		{
			if(!FCEUD_RecvData(buf,5))
			{
				NetError();
				return;
			}

			switch(buf[4])
			{
			default: FCEU_DoSimpleCommand(buf[4]);break;
			case FCEUNPCMD_TEXT:
				{
					uint8 *tbuf;
					uint32 len = FCEU_de32lsb(buf);

					if(len > 100000)  // Insanity check!
					{
						NetError();
						return;
					}
					tbuf = (uint8*)malloc(len + 1); //mbg merge 7/17/06 added cast
					tbuf[len] = 0;
					if(!FCEUD_RecvData(tbuf, len))
					{
						NetError();
						free(tbuf);
						return;
					}
					FCEUD_NetplayText(tbuf);
					free(tbuf);
				}
				break;
			case FCEUNPCMD_SAVESTATE:
				{
					//mbg todo netplay
					//char *fn;
					//FILE *fp;

					////Send the cheats first, then the save state, since
					////there might be a frame or two in between the two sendfile
					////commands on the server side.

					//fn = strdup(FCEU_MakeFName(FCEUMKF_CHEAT,0,0).c_str());

					////why??????
					////if(!
					//	FCEUNET_SendFile(FCEUNPCMD_LOADCHEATS,fn);
					//// {
					////  free(fn);
					////  return;
					//// }

					//free(fn);
					//if(!FCEUnetplay) return;

					//fn = strdup(FCEU_MakeFName(FCEUMKF_NPTEMP,0,0).c_str());
					//fp = fopen(fn, "wb");
					//if(FCEUSS_SaveFP(fp,Z_BEST_COMPRESSION))
					//{
					//	fclose(fp);
					//	if(!FCEUNET_SendFile(FCEUNPCMD_LOADSTATE, fn))
					//	{
					//		unlink(fn);
					//		free(fn);
					//		return;
					//	}
					//	unlink(fn);
					//	free(fn);
					//}
					//else
					//{
					//	fclose(fp);
					//	FCEUD_PrintError("File error.  (K)ill, (M)aim, (D)estroy?  Now!");
					//	unlink(fn);
					//	free(fn);
					//	return;
					//}

				}
				break;
			case FCEUNPCMD_LOADCHEATS:
				{
					FILE *fp = FetchFile(FCEU_de32lsb(buf));
					if(!fp) return;
					FCEU_FlushGameCheats(0,1);
					FCEU_LoadGameCheats(fp);
				}
				break;
				//mbg 6/16/08 - netplay doesnt work right now anyway
				/*case FCEUNPCMD_LOADSTATE:
				{
				FILE *fp = FetchFile(FCEU_de32lsb(buf));
				if(!fp) return;
				if(FCEUSS_LoadFP(fp,SSLOADPARAM_BACKUP))
			 {
			 fclose(fp);
			 FCEU_DispMessage("Remote state loaded.",0);
			 } else FCEUD_PrintError("File error.  (K)ill, (M)aim, (D)estroy?");
			 }
			 break;*/
			}
		} while(buf[4]);

		netdcount=(netdcount+1)%netdivisor;

		memcpy(netjoy,buf,4);
		*(uint32 *)joyp=*(uint32 *)netjoy;
	}
}
