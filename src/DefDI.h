/*
Copyright (C) 2001 Deflection

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
	Modified for TAS by Nitsuja
*/

#ifndef _DEFDI_H_INCLUDED__
#define _DEFDI_H_INCLUDED__

#define NUMBER_OF_CONTROLS	4
#define NUMBER_OF_BUTTONS	25

#define SUBKEY "Software\\N64 Emulation\\DLL\\TASDI"

//Used for Input structure type.
#define INPUT_TYPE_NOT_USED		0
#define INPUT_TYPE_KEY_BUT		1
#define INPUT_TYPE_JOY_BUT		2
#define INPUT_TYPE_JOY_AXIS		3
#define INPUT_TYPE_JOY_POV		4

#define DIJOFS_YN		0
#define DIJOFS_YP		1
#define DIJOFS_XN		2
#define DIJOFS_XP		3
#define DIJOFS_ZN		4
#define DIJOFS_ZP		5
#define DIJOFS_RYN		6
#define DIJOFS_RYP		7
#define DIJOFS_RXN		8
#define DIJOFS_RXP		9
#define DIJOFS_RZN		10
#define DIJOFS_RZP		11
#define DIJOFS_SLIDER0N	12
#define DIJOFS_SLIDER0P	13
#define DIJOFS_SLIDER1N	14
#define DIJOFS_SLIDER1P	15
#define DIJOFS_POV0N	16
#define DIJOFS_POV0E	17
#define DIJOFS_POV0S	18
#define DIJOFS_POV0W	19
#define DIJOFS_POV1N	20
#define DIJOFS_POV1E	21
#define DIJOFS_POV1S	22
#define DIJOFS_POV1W	23
#define DIJOFS_POV2N	24
#define DIJOFS_POV2E	25
#define DIJOFS_POV2S	26
#define DIJOFS_POV2W	27
#define DIJOFS_POV3N	28
#define DIJOFS_POV3E	29
#define DIJOFS_POV3S	30
#define DIJOFS_POV3W	31

#define BUTTONDOWN(name, key) (name[key] & 0x80)

#define IDT_TIMER3 3

//combo tasks
#define C_IDLE 0
#define C_RUNNING 1
#define C_LOOP 2
#define C_PAUSE 3
#define C_RECORD 4

//custom messages
#define EDIT_END 10001

typedef struct
{
    BYTE Device;
    BYTE type;
    BYTE vkey;
    DWORD button;
} INPUT_CTRL;

typedef struct
{
    TCHAR szName[16];
    BYTE NDevices;
    DWORD Devices[MAX_DEVICES];
    BOOL bActive;
    BOOL bMemPak;
    BYTE SensMax;
    BYTE SensMin;
    INPUT_CTRL Input[NUMBER_OF_BUTTONS];
} DEFCONTROLLER;

extern DEFCONTROLLER Controller[NUMBER_OF_CONTROLS];

extern HINSTANCE g_hInstance;

typedef struct s_menu_config
{
	// dummy element to pad offset of next element to non-0 
	bool dummy_element = false;
    bool always_on_top = false;
    bool float_from_parent = true;
} t_menu_config;

//----

void WINAPI GetNegAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed,
                          int& M2Speed);
void WINAPI GetPosAxisVal(LONG AxisValue, int Control, LONG count, BUTTONS* ControllerInput, int& M1Speed,
                          int& M2Speed);
void WINAPI InitializeAndCheckDevices(HWND hMainWindow);
BOOL WINAPI CheckForDeviceChange(HKEY hKey);
LRESULT CALLBACK StatusDlgProc(HWND hDlg, UINT Message, WPARAM wParam, LPARAM lParam);
VOID CALLBACK StatusDlgProcTimer(UINT idEvent, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

#define UPDATEAUTO(idc,field) \
{ \
	if(IsMouseOverControl(statusDlg,idc)) \
	{ \
		CheckDlgButton(statusDlg,idc,buttonAutofire.field|buttonAutofire2.field ? 0 : 1); \
		BUTTONS &autoFire1 = (frameCounter%2 == 0) ? buttonAutofire : buttonAutofire2; \
		BUTTONS &autoFire2 = (frameCounter%2 == 0) ? buttonAutofire2 : buttonAutofire; \
		autoFire1.field = 0; \
		autoFire2.field = !(autoFire1.field|autoFire2.field); \
		buttonOverride.field ^= 1; \
		buttonDisplayed.field = autoFire2.field; \
	} \
}

#endif
