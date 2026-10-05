#pragma once

#define RM_SAFE_UPDATE_RESULT (WM_APP + 50)
VOID _app_update_initialize (HWND hwnd);
VOID _app_update_shutdown (HWND hwnd);
VOID _app_update_enable (BOOLEAN enabled);
BOOLEAN _app_update_isenabled (BOOLEAN check_timestamp);
BOOLEAN _app_update_check (HWND parent);
VOID _app_update_result (HWND hwnd, LPARAM result);
