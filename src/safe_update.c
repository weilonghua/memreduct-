// A check-only updater: remote metadata can never select an executable or URL.
#include "routine.h"
#include "safe_update.h"
#include "update_policy.h"

typedef struct _APP_UPDATE_RESULT
{
    ULONG error;
    BOOLEAN manual;
    CHAR version[32];
} APP_UPDATE_RESULT;

static volatile LONG update_busy;
static SRWLOCK update_window_lock = SRWLOCK_INIT;
static HWND update_window;

static ULONG fetch_version (HINTERNET session, CHAR version[32])
{
    R_STRINGREF url;
    HINTERNET connection = NULL, request = NULL;
    CHAR data[4097];
    ULONG read = 0, total = 0, expected = 0;
    ULONG error;
    _r_obj_initializestringref (&url, APP_UPDATE_MANIFEST_URL);
    error = _r_inet_openurl (session, &url, &connection, &request, &expected);
    if (error) return error;
    for (;;)
    {
        if (!WinHttpReadData (request, data + total, sizeof (data) - total, &read))
        {
            error = GetLastError ();
            break;
        }
        if (!read) break;
        total += read;
        if (total > 4096)
        {
            error = ERROR_INVALID_DATA;
            break;
        }
    }
    // Decompression is disabled below: advertised bytes must match the body.
    if (!error && ((expected && expected != total) || !app_update_parse_manifest (data, total, version)))
        error = ERROR_INVALID_DATA;
    _r_inet_close (request);
    _r_inet_close (connection);
    return error;
}

VOID _app_update_initialize (HWND hwnd)
{
    AcquireSRWLockExclusive (&update_window_lock);
    update_window = hwnd;
    ReleaseSRWLockExclusive (&update_window_lock);
}

VOID _app_update_shutdown (HWND hwnd)
{
    MSG message;
    AcquireSRWLockExclusive (&update_window_lock);
    update_window = NULL;
    ReleaseSRWLockExclusive (&update_window_lock);
    while (PeekMessageW (&message, hwnd, RM_SAFE_UPDATE_RESULT, RM_SAFE_UPDATE_RESULT, PM_REMOVE))
        _r_mem_free ((PVOID)message.lParam);
}

VOID _app_update_enable (BOOLEAN enabled)
{
    _r_config_setlong (L"CheckUpdatesPeriod", enabled ? 6 : 0);
}

BOOLEAN _app_update_isenabled (BOOLEAN check_timestamp)
{
    LONG hours = _r_config_getlong (L"CheckUpdatesPeriod", 6);
    if (hours <= 0)
        return FALSE;
    if (hours > 168) hours = 168;
    return !check_timestamp || (_r_unixtime_now () - _r_config_getlong64 (L"CheckUpdatesLast", 0)) >= (LONG64)hours * 3600;
}

static NTSTATUS NTAPI update_worker (PVOID argument)
{
    APP_UPDATE_RESULT *result = argument;
    HINTERNET session;
    BOOLEAN posted = FALSE;

    session = _r_inet_createsession (_r_app_getuseragent (), _r_app_getproxyconfiguration ());
    if (!session)
        result->error = GetLastError ();
    else if (!WinHttpSetTimeouts (session, 5000, 5000, 5000, 5000) ||
        !WinHttpSetOption (session, WINHTTP_OPTION_REDIRECT_POLICY, &(ULONG){WINHTTP_OPTION_REDIRECT_POLICY_NEVER}, sizeof (ULONG)) ||
        !WinHttpSetOption (session, WINHTTP_OPTION_SECURE_PROTOCOLS, &(ULONG){WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2}, sizeof (ULONG)))
        result->error = GetLastError ();
    else if (_r_sys_isosversiongreaterorequal (WINDOWS_8) &&
        !WinHttpSetOption (session, WINHTTP_OPTION_DECOMPRESSION, &(ULONG){0}, sizeof (ULONG)))
        result->error = GetLastError ();
    else
        result->error = fetch_version (session, result->version);
    if (session) _r_inet_close (session);

    AcquireSRWLockShared (&update_window_lock);
    if (update_window)
        posted = (BOOLEAN)PostMessageW (update_window, RM_SAFE_UPDATE_RESULT, 0, (LPARAM)result);
    ReleaseSRWLockShared (&update_window_lock);
    if (!posted)
    {
        _r_mem_free (result);
        InterlockedExchange (&update_busy, 0);
    }
    return STATUS_SUCCESS;
}

BOOLEAN _app_update_check (HWND parent)
{
    HANDLE thread;
    NTSTATUS status;
    APP_UPDATE_RESULT *result;
    if (!parent && !_app_update_isenabled (TRUE))
        return FALSE;
    if (InterlockedCompareExchange (&update_busy, 1, 0))
        return FALSE;
    result = _r_mem_allocate (sizeof (*result));
    RtlZeroMemory (result, sizeof (*result));
    result->manual = (parent != NULL);
    status = _r_sys_createthread (&thread, NtCurrentProcess (), update_worker, result, NULL, L"SafeUpdateCheck");
    if (!NT_SUCCESS (status))
    {
        _r_mem_free (result);
        InterlockedExchange (&update_busy, 0);
        if (parent) _r_show_errormessage (parent, L"Could not check updates", status, NULL, ET_NATIVE);
        return FALSE;
    }
    _r_config_setlong64 (L"CheckUpdatesLast", _r_unixtime_now ());
    NtClose (thread);
    return TRUE;
}

VOID _app_update_result (HWND hwnd, LPARAM argument)
{
    APP_UPDATE_RESULT *result = (APP_UPDATE_RESULT *)argument;
    WCHAR message[512], version[32];
    PR_STRING previous;
    InterlockedExchange (&update_busy, 0);
    if (result->error)
    {
        if (result->manual)
        {
            _r_str_printf (message, RTL_NUMBER_OF (message), _r_locale_getstring (IDS_SAFE_UPDATE_ERROR), result->error);
            _r_show_message (hwnd, MB_OK | MB_ICONWARNING, NULL, message);
        }
    }
    else if (app_update_version_compare (result->version, APP_VERSION_ASCII) > 0)
    {
        MultiByteToWideChar (CP_UTF8, 0, result->version, -1, version, RTL_NUMBER_OF (version));
        previous = _r_config_getstring (L"LastUpdateNotified", L"");
        if (result->manual || !previous || !previous->length || wcscmp (previous->buffer, version))
        {
            _r_config_setstring (L"LastUpdateNotified", version);
            _r_str_printf (message, RTL_NUMBER_OF (message), _r_locale_getstring (IDS_SAFE_UPDATE_AVAILABLE), version);
            if (_r_show_message (hwnd, MB_YESNO | MB_ICONINFORMATION, NULL, message) == IDYES)
                _r_shell_opendefault (APP_UPDATE_RELEASES_URL);
        }
        if (previous) _r_obj_dereference (previous);
    }
    else if (result->manual)
        _r_show_message (hwnd, MB_OK | MB_ICONINFORMATION, NULL, _r_locale_getstring (IDS_UPDATE_NO));
    _r_mem_free (result);
}
