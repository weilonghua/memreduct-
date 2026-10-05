#pragma once

#define RM_CLEANUP_RESULT (WM_APP + 51)
VOID _app_cleanup_initialize (HWND hwnd);
VOID _app_cleanup_shutdown (HWND hwnd);
LONG64 _app_cleanup_elapsed ();
BOOLEAN _app_cleanup_start (HWND hwnd, CLEANUP_SOURCE_ENUM source, ULONG mask);
VOID _app_cleanup_result (HWND hwnd, LPARAM argument);
