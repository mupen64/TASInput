/*
 * Copyright (c) 2025, Mupen64 maintainers, contributors, and original authors (Hacktarux, ShadowPrince, linker).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Describes the Mupen64 Plugin API.
 */

#pragma once

/**
 * \brief Describes a controller.
 */
typedef struct
{
    int32_t Present;
    int32_t RawData;
    int32_t Plugin;
} core_controller;

/**
 * \brief Represents an extension for a controller.
 */
typedef enum {
    ce_none = 1,
    ce_mempak = 2,
    ce_rumblepak = 3,
    ce_transferpak = 4,
    ce_raw = 5
} core_controller_extension;

/**
 * \brief Describes generic information about a plugin.
 */
typedef struct
{
    uint16_t Version;
    uint16_t Type;
    char Name[100];

    int32_t NormalMemory;
    int32_t MemoryBswaped;
} core_plugin_info;

/**
 * \brief Describes framebuffer information.
 */
typedef struct
{
    uint32_t addr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
} core_fb_info;

/**
 * \brief Represents a plugin type.
 */
typedef enum {
    plugin_video = 2,
    plugin_audio = 3,
    plugin_input = 4,
    plugin_rsp = 1,
} core_plugin_type;


/**
 * \brief Describes information about a video plugin.
 */
typedef struct
{
    void* hWnd;
    void* hStatusBar;

    int32_t MemoryBswaped;
    uint8_t* HEADER;
    uint8_t* RDRAM;
    uint8_t* DMEM;
    uint8_t* IMEM;

    uint32_t* MI_INTR_REG;

    uint32_t* DPC_START_REG;
    uint32_t* DPC_END_REG;
    uint32_t* DPC_CURRENT_REG;
    uint32_t* DPC_STATUS_REG;
    uint32_t* DPC_CLOCK_REG;
    uint32_t* DPC_BUFBUSY_REG;
    uint32_t* DPC_PIPEBUSY_REG;
    uint32_t* DPC_TMEM_REG;

    uint32_t* VI_STATUS_REG;
    uint32_t* VI_ORIGIN_REG;
    uint32_t* VI_WIDTH_REG;
    uint32_t* VI_INTR_REG;
    uint32_t* VI_V_CURRENT_LINE_REG;
    uint32_t* VI_TIMING_REG;
    uint32_t* VI_V_SYNC_REG;
    uint32_t* VI_H_SYNC_REG;
    uint32_t* VI_LEAP_REG;
    uint32_t* VI_H_START_REG;
    uint32_t* VI_V_START_REG;
    uint32_t* VI_V_BURST_REG;
    uint32_t* VI_X_SCALE_REG;
    uint32_t* VI_Y_SCALE_REG;

    void(__cdecl* CheckInterrupts)(void);
} core_gfx_info;

/**
 * \brief Describes information about an audio plugin.
 */
typedef struct
{
    void* hwnd;
    void* hinst;

    int32_t MemoryBswaped;
    uint8_t* HEADER;
    uint8_t* RDRAM;
    uint8_t* DMEM;
    uint8_t* IMEM;

    uint32_t* MI_INTR_REG;

    uint32_t* AI_DRAM_ADDR_REG;
    uint32_t* AI_LEN_REG;
    uint32_t* AI_CONTROL_REG;
    uint32_t* AI_STATUS_REG;
    uint32_t* AI_DACRATE_REG;
    uint32_t* AI_BITRATE_REG;

    void(__cdecl* CheckInterrupts)(void);
} core_audio_info;

/**
 * \brief Describes information about an input plugin.
 */
typedef struct
{
    void* hMainWindow;
    void* hinst;

    int32_t MemoryBswaped;
    uint8_t* HEADER;
    core_controller* Controls;
} core_input_info;

/**
 * \brief Describes information about an RSP plugin.
 */
typedef struct
{
    void* hInst;
    int32_t MemoryBswaped;
    uint8_t* RDRAM;
    uint8_t* DMEM;
    uint8_t* IMEM;

    uint32_t* MI_INTR_REG;

    uint32_t* SP_MEM_ADDR_REG;
    uint32_t* SP_DRAM_ADDR_REG;
    uint32_t* SP_RD_LEN_REG;
    uint32_t* SP_WR_LEN_REG;
    uint32_t* SP_STATUS_REG;
    uint32_t* SP_DMA_FULL_REG;
    uint32_t* SP_DMA_BUSY_REG;
    uint32_t* SP_PC_REG;
    uint32_t* SP_SEMAPHORE_REG;

    uint32_t* DPC_START_REG;
    uint32_t* DPC_END_REG;
    uint32_t* DPC_CURRENT_REG;
    uint32_t* DPC_STATUS_REG;
    uint32_t* DPC_CLOCK_REG;
    uint32_t* DPC_BUFBUSY_REG;
    uint32_t* DPC_PIPEBUSY_REG;
    uint32_t* DPC_TMEM_REG;

    void(__cdecl* CheckInterrupts)(void);
    void(__cdecl* ProcessDlistList)(void);
    void(__cdecl* ProcessAlistList)(void);
    void(__cdecl* ProcessRdpList)(void);
    void(__cdecl* ShowCFB)(void);
} core_rsp_info;

/**
 * \brief Represents a controller state.
 */
typedef union {
    uint32_t Value;

    struct
    {
        unsigned R_DPAD : 1;
        unsigned L_DPAD : 1;
        unsigned D_DPAD : 1;
        unsigned U_DPAD : 1;
        unsigned START_BUTTON : 1;
        unsigned Z_TRIG : 1;
        unsigned B_BUTTON : 1;
        unsigned A_BUTTON : 1;

        unsigned R_CBUTTON : 1;
        unsigned L_CBUTTON : 1;
        unsigned D_CBUTTON : 1;
        unsigned U_CBUTTON : 1;
        unsigned R_TRIG : 1;
        unsigned L_TRIG : 1;
        unsigned Reserved1 : 1;
        unsigned Reserved2 : 1;

        signed Y_AXIS : 8;

        signed X_AXIS : 8;
    };
} core_buttons;

typedef void(__cdecl* GETDLLINFO)(core_plugin_info*);
typedef void(__cdecl* DLLCONFIG)(void*);
typedef void(__cdecl* DLLTEST)(void*);
typedef void(__cdecl* DLLABOUT)(void*);

typedef void(__cdecl* CHANGEWINDOW)();
typedef void(__cdecl* CLOSEDLL_GFX)();
typedef int32_t(__cdecl* INITIATEGFX)(core_gfx_info);
typedef void(__cdecl* PROCESSDLIST)();
typedef void(__cdecl* PROCESSRDPLIST)();
typedef void(__cdecl* ROMCLOSED_GFX)();
typedef void(__cdecl* ROMOPEN_GFX)();
typedef void(__cdecl* SHOWCFB)();
typedef void(__cdecl* UPDATESCREEN)();
typedef void(__cdecl* VISTATUSCHANGED)();
typedef void(__cdecl* VIWIDTHCHANGED)();
typedef void(__cdecl* READSCREEN)(void**, int32_t*, int32_t*);
typedef void(__cdecl* DLLCRTFREE)(void*);
typedef void(__cdecl* MOVESCREEN)(int32_t, int32_t);
typedef void(__cdecl* CAPTURESCREEN)(char*);
typedef void(__cdecl* GETVIDEOSIZE)(int32_t*, int32_t*);
typedef void(__cdecl* READVIDEO)(void**);
typedef void(__cdecl* FBREAD)(uint32_t);
typedef void(__cdecl* FBWRITE)(uint32_t addr, uint32_t size);
typedef void(__cdecl* FBGETFRAMEBUFFERINFO)(void*);

typedef void(__cdecl* AIDACRATECHANGED)(int32_t system_type);
typedef void(__cdecl* AILENCHANGED)();
typedef uint32_t(__cdecl* AIREADLENGTH)();
typedef void(__cdecl* CLOSEDLL_AUDIO)();
typedef int32_t(__cdecl* INITIATEAUDIO)(core_audio_info);
typedef void(__cdecl* PROCESSALIST)();
typedef void(__cdecl* ROMCLOSED_AUDIO)();
typedef void(__cdecl* ROMOPEN_AUDIO)();
typedef void(__cdecl* AIUPDATE)(int32_t wait);

typedef void(__cdecl* CLOSEDLL_INPUT)();
typedef void(__cdecl* CONTROLLERCOMMAND)(int32_t controller, unsigned char* command);
typedef void(__cdecl* GETKEYS)(int32_t controller, core_buttons* keys);
typedef void(__cdecl* SETKEYS)(int32_t controller, core_buttons keys);
typedef void(__cdecl* OLD_INITIATECONTROLLERS)(void* hwnd, core_controller controls[4]);
typedef void(__cdecl* INITIATECONTROLLERS)(core_input_info control_info);
typedef void(__cdecl* READCONTROLLER)(int32_t controller, unsigned char* command);
typedef void(__cdecl* ROMCLOSED_INPUT)();
typedef void(__cdecl* ROMOPEN_INPUT)();
typedef void(__cdecl* KEYDOWN)(uint32_t wParam, int32_t lParam);
typedef void(__cdecl* KEYUP)(uint32_t wParam, int32_t lParam);

typedef void(__cdecl* CLOSEDLL_RSP)();
typedef uint32_t(__cdecl* DORSPCYCLES)(uint32_t);
typedef void(__cdecl* INITIATERSP)(core_rsp_info rsp_info, uint32_t* cycles);
typedef void(__cdecl* ROMCLOSED_RSP)();

#if defined(CORE_PLUGIN_WITH_CALLBACKS)

// ReSharper disable CppInconsistentNaming

#define EXPORT __declspec(dllexport)
#define CALL _cdecl

#pragma region Base

EXPORT void CALL CloseDLL(void);
EXPORT void CALL DllAbout(void* hParent);
EXPORT void CALL DllConfig(void* hParent);
EXPORT void CALL GetDllInfo(core_plugin_info* PluginInfo);
EXPORT void CALL RomClosed(void);
EXPORT void CALL RomOpen(void);

#pragma endregion

#pragma region Video

EXPORT void CALL CaptureScreen(const char* Directory);
EXPORT void CALL ChangeWindow(void);
EXPORT int CALL InitiateGFX(core_gfx_info Gfx_Info);
EXPORT void CALL MoveScreen(int xpos, int ypos);
EXPORT void CALL ProcessDList(void);
EXPORT void CALL ProcessRDPList(void);
EXPORT void CALL ShowCFB(void);
EXPORT void CALL UpdateScreen(void);
EXPORT void CALL ViStatusChanged(void);
EXPORT void CALL ViWidthChanged(void);

#pragma endregion

#pragma region Audio

EXPORT void CALL AiDacrateChanged(int32_t SystemType);
EXPORT void CALL AiLenChanged(void);
EXPORT uint32_t CALL AiReadLength(void);
EXPORT void CALL AiUpdate(int32_t Wait);
EXPORT void CALL DllTest(void* hParent);
EXPORT int32_t CALL InitiateAudio(core_audio_info Audio_Info);
EXPORT void CALL ProcessAList(void);

#pragma endregion

#pragma region Input

EXPORT void CALL ControllerCommand(int32_t Control, uint8_t* Command);
EXPORT void CALL GetKeys(int32_t Control, core_buttons* Keys);
EXPORT void CALL InitiateControllers(core_input_info* ControlInfo);
EXPORT void CALL ReadController(int Control, uint8_t* Command);
EXPORT void CALL WM_KeyDown(uint32_t wParam, uint32_t lParam);
EXPORT void CALL WM_KeyUp(uint32_t wParam, uint32_t lParam);

#pragma endregion

#pragma region RSP

EXPORT uint32_t DoRspCycles(uint32_t Cycles);
EXPORT void InitiateRSP(core_rsp_info Rsp_Info, uint32_t* CycleCount);

#pragma endregion

#undef EXPORT
#undef CALL

// ReSharper restore CppInconsistentNaming

#endif
