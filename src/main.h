// Mem Reduct
// Copyright (c) 2011-2026 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

DEFINE_GUID (GUID_TrayIcon, 0xAE9053F0, 0x8D59, 0x4803, 0x9A, 0xBB, 0x74, 0xAF, 0xE6, 0x6B, 0x5F, 0xD2);


#define TIMER 1000
#define UID 1337

#define LANG_SUBMENU 1
#define LANG_MENU 4

#define TRAY_SUBMENU_1 4
#define TRAY_SUBMENU_2 5
#define TRAY_SUBMENU_3 6

#define DEFAULT_AUTOREDUCT_VAL 90
#define DEFAULT_AUTOREDUCTINTERVAL_VAL 30

// minimum pause (in seconds) between usage-based automatic cleanups
#define AUTOREDUCT_COOLDOWN 30

#define DEFAULT_DANGER_LEVEL 90
#define DEFAULT_WARNING_LEVEL 70

// colors
#define TRAY_COLOR_BLACK RGB(0x00, 0x00, 0x00)
#define TRAY_COLOR_WHITE RGB(0xFF, 0xFF, 0xFF)
#define TRAY_COLOR_TEXT RGB(0xFF, 0xFF, 0xFF)
#define TRAY_COLOR_BG RGB(0x00, 0x80, 0x40)
#define TRAY_COLOR_WARNING RGB(0xFF, 0x80, 0x40)
#define TRAY_COLOR_DANGER RGB(0xEC, 0x1C, 0x24)

#include "cleanup_engine.h"

typedef struct _STATIC_DATA
{
	HDC hdc;
	HDC hdc_mask;
	HBITMAP hbitmap;
	HBITMAP hbitmap_mask;
	HFONT hfont;
	RECT icon_size;
	ULONG ms_prev;
} STATIC_DATA, *PSTATIC_DATA;

typedef enum _CLEANUP_SOURCE_ENUM
{
	SOURCE_AUTO,
	SOURCE_MANUAL,
	SOURCE_HOTKEY,
	SOURCE_CMDLINE
} CLEANUP_SOURCE_ENUM, *PCLEANUP_SOURCE_ENUM;


