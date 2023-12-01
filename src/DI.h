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

#ifndef _DI_H_INCLUDED__
#define _DI_H_INCLUDED__

#define MAX_DEVICES 5
#include "DefDI.h"

extern BYTE nCurrentDevices;

extern LPDIRECTINPUT8 g_lpDI;

extern GUID Guids[MAX_DEVICES];

using DIINPUTDEVICE = struct
{
    LPDIRECTINPUTDEVICE8 lpDIDevice;
    DIDEVICEINSTANCE DIDevInst;
};

extern DIINPUTDEVICE DInputDev[MAX_DEVICES];

void WINAPI FreeDirectInput();
BOOL WINAPI InitDirectInput(HWND hMainWindow);
BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, LPVOID pContext);


/**
 * \brief Gets the current input for a specific controller index
 * \param controllers An array of controllers
 * \param index The index into the array of the controls
 * \param x_scale The multiplier for the X joystick axis
 * \param y_scale The multiplier for the Y joystick axis
 * \return The held controller input
 */
BUTTONS get_controller_input(DEFCONTROLLER* controllers, size_t index, float x_scale, float y_scale);

#endif
