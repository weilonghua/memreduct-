#include "main.h"
#include "cleanup.h"

typedef struct _APP_CLEANUP_JOB
{
    CLEANUP_SOURCE_ENUM source;
    ULONG mask;
    APP_CLEANUP_REPORT report;
} APP_CLEANUP_JOB;

// All job output belongs to the worker until it is posted to the UI thread.
static volatile LONG cleanup_busy;
static volatile LONG cleanup_cancelled;
static SRWLOCK cleanup_window_lock = SRWLOCK_INIT;
static HWND cleanup_window;
static ULONGLONG last_attempt_tick;

static NTSTATUS flush_fixed_volumes ()
{
    WCHAR name[MAX_PATH];
    HANDLE enumeration, volume;
    NTSTATUS status = STATUS_SUCCESS;
    DWORD error;
    enumeration = FindFirstVolumeW (name, RTL_NUMBER_OF (name));
    if (enumeration == INVALID_HANDLE_VALUE)
        return STATUS_UNSUCCESSFUL;
    do
    {
        SIZE_T length;
        if (InterlockedCompareExchange (&cleanup_cancelled, 0, 0))
        {
            status = STATUS_CANCELLED;
            break;
        }
        if (GetDriveTypeW (name) != DRIVE_FIXED)
            continue;
        length = wcslen (name);
        if (!length || name[length - 1] != L'\\')
        {
            status = STATUS_UNSUCCESSFUL;
            continue;
        }
        name[length - 1] = UNICODE_NULL;
        volume = CreateFileW (name, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (volume == INVALID_HANDLE_VALUE)
        {
            _r_log (LOG_LEVEL_ERROR, NULL, L"CreateFileW(volume)", GetLastError (), name);
            status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            if (!FlushFileBuffers (volume))
            {
                _r_log (LOG_LEVEL_ERROR, NULL, L"FlushFileBuffers", GetLastError (), name);
                status = STATUS_UNSUCCESSFUL;
            }
            CloseHandle (volume);
        }
    }
    while (FindNextVolumeW (enumeration, name, RTL_NUMBER_OF (name)));
    error = GetLastError ();
    FindVolumeClose (enumeration);
    if (error != ERROR_NO_MORE_FILES && status != STATUS_CANCELLED)
        status = STATUS_UNSUCCESSFUL;
    return status;
}

static NTSTATUS perform_step (ULONG mask)
{
    SYSTEM_MEMORY_LIST_COMMAND command;
    SYSTEM_FILECACHE_INFORMATION cache = {0};
    MEMORY_COMBINE_INFORMATION_EX combine = {0};
    switch (mask)
    {
        case REDUCT_WORKINGSET: command = MemoryEmptyWorkingSets; break;
        case REDUCT_STANDBYPRIORITY0LIST: command = MemoryPurgeLowPriorityStandbyList; break;
        case REDUCT_STANDBYLIST: command = MemoryPurgeStandbyList; break;
        case REDUCT_MODIFIEDLIST: command = MemoryFlushModifiedList; break;
        case REDUCT_SYSTEMFILECACHE:
            cache.MinimumWorkingSet = cache.MaximumWorkingSet = MAXSIZE_T;
            return NtSetSystemInformation (SystemFileCacheInformationEx, &cache, sizeof (cache));
        case REDUCT_MODIFIEDFILECACHE: return flush_fixed_volumes ();
        case REDUCT_REGISTRYCACHE:
            if (!_r_sys_isosversiongreaterorequal (WINDOWS_8_1)) return STATUS_NOT_SUPPORTED;
            return NtSetSystemInformation (SystemRegistryReconciliationInformation, NULL, 0);
        case REDUCT_COMBINEMEMORYLISTS:
            if (!_r_sys_isosversiongreaterorequal (WINDOWS_10)) return STATUS_NOT_SUPPORTED;
            return NtSetSystemInformation (SystemCombinePhysicalMemoryInformation, &combine, sizeof (combine));
        default: return STATUS_INVALID_PARAMETER;
    }
    return NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (command));
}

static APP_STEP_RESULT cleanup_perform (void *context, uint32_t mask)
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER (context);
    status = perform_step (mask);
    if (status == STATUS_NOT_SUPPORTED || status == STATUS_CANCELLED) return APP_STEP_SKIPPED;
    if (NT_SUCCESS (status)) return APP_STEP_SUCCESS;
    _r_log_v (LOG_LEVEL_ERROR, NULL, L"Memory cleanup", status, L"mask=0x%02X", mask);
    return APP_STEP_FAILED;
}

static int cleanup_is_cancelled (void *context)
{
    UNREFERENCED_PARAMETER (context);
    return InterlockedCompareExchange (&cleanup_cancelled, 0, 0) != 0;
}

static int cleanup_sample (void *context, uint64_t *available)
{
    MEMORYSTATUSEX memory = {.dwLength = sizeof (MEMORYSTATUSEX)};
    UNREFERENCED_PARAMETER (context);
    if (!GlobalMemoryStatusEx (&memory)) return 0;
    *available = memory.ullAvailPhys;
    return 1;
}

static uint64_t cleanup_tick (void *context)
{
    UNREFERENCED_PARAMETER (context);
    return GetTickCount64 ();
}

static VOID run_job (APP_CLEANUP_JOB *job)
{
    APP_CLEANUP_OPERATIONS operations = {NULL, cleanup_perform, cleanup_is_cancelled, cleanup_sample, cleanup_tick};
    app_cleanup_run (job->mask, &operations, &job->report);
}

static NTSTATUS NTAPI cleanup_worker (PVOID argument)
{
    APP_CLEANUP_JOB *job = argument;
    BOOLEAN posted = FALSE;
    run_job (job);
    AcquireSRWLockShared (&cleanup_window_lock);
    if (cleanup_window)
        posted = (BOOLEAN)PostMessageW (cleanup_window, RM_CLEANUP_RESULT, 0, (LPARAM)job);
    ReleaseSRWLockShared (&cleanup_window_lock);
    if (!posted)
    {
        _r_mem_free (job);
        InterlockedExchange (&cleanup_busy, 0);
    }
    return STATUS_SUCCESS;
}

VOID _app_cleanup_initialize (HWND hwnd)
{
    last_attempt_tick = GetTickCount64 ();
    AcquireSRWLockExclusive (&cleanup_window_lock);
    cleanup_window = hwnd;
    ReleaseSRWLockExclusive (&cleanup_window_lock);
}

VOID _app_cleanup_shutdown (HWND hwnd)
{
    MSG message;
    InterlockedExchange (&cleanup_cancelled, 1);
    AcquireSRWLockExclusive (&cleanup_window_lock);
    cleanup_window = NULL;
    ReleaseSRWLockExclusive (&cleanup_window_lock);
    while (PeekMessageW (&message, hwnd, RM_CLEANUP_RESULT, RM_CLEANUP_RESULT, PM_REMOVE))
        _r_mem_free ((PVOID)message.lParam);
}

LONG64 _app_cleanup_elapsed ()
{
    return (LONG64)((GetTickCount64 () - last_attempt_tick) / 1000);
}

BOOLEAN _app_cleanup_start (HWND hwnd, CLEANUP_SOURCE_ENUM source, ULONG mask)
{
    APP_CLEANUP_JOB *job;
    HANDLE thread;
    NTSTATUS status;
    mask &= REDUCT_MASK_ALL;
    if (!mask || InterlockedCompareExchange (&cleanup_busy, 1, 0)) return FALSE;
    job = _r_mem_allocate (sizeof (*job));
    RtlZeroMemory (job, sizeof (*job));
    job->source = source;
    job->mask = mask;
    last_attempt_tick = GetTickCount64 ();
    _r_config_setlong64 (L"StatisticLastAttempt", _r_unixtime_now ());
    if (!hwnd)
    {
        run_job (job);
        _app_cleanup_result (NULL, (LPARAM)job);
        return TRUE;
    }
    status = _r_sys_createthread (&thread, NtCurrentProcess (), cleanup_worker, job, NULL, L"MemoryCleanup");
    if (!NT_SUCCESS (status))
    {
        _r_mem_free (job);
        InterlockedExchange (&cleanup_busy, 0);
        _r_show_errormessage (hwnd, L"Could not start cleanup", status, NULL, ET_NATIVE);
        return FALSE;
    }
    NtClose (thread);
    _r_ctrl_enable (hwnd, IDC_CLEAN, FALSE);
    return TRUE;
}

VOID _app_cleanup_result (HWND hwnd, LPARAM argument)
{
    APP_CLEANUP_JOB *job = (APP_CLEANUP_JOB *)argument;
    WCHAR counts[256], observed[256], bytes[64], message[768];
    UINT title = job->report.failed || job->report.skipped ? IDS_SAFE_CLEAN_PARTIAL : IDS_SAFE_CLEAN_DONE;
    ULONG flags = job->report.failed ? NIIF_WARNING : NIIF_INFO;
    if (!job->report.succeeded) title = IDS_SAFE_CLEAN_FAILED;
    last_attempt_tick = GetTickCount64 ();
    if (job->report.succeeded) _r_config_setlong64 (L"StatisticLastReduct", _r_unixtime_now ());
    _r_str_printf (counts, RTL_NUMBER_OF (counts), _r_locale_getstring (IDS_SAFE_CLEAN_COUNTS), job->report.succeeded, job->report.failed, job->report.skipped);
    if (job->report.memory_valid)
    {
        _r_format_bytesize64 (bytes, RTL_NUMBER_OF (bytes), job->report.observed_bytes);
        _r_str_printf (observed, RTL_NUMBER_OF (observed), _r_locale_getstring (IDS_SAFE_CLEAN_OBSERVED), bytes, job->report.elapsed_ms);
    }
    else _r_str_copy (observed, RTL_NUMBER_OF (observed), _r_locale_getstring (IDS_SAFE_CLEAN_UNAVAILABLE));
    _r_str_printf (message, RTL_NUMBER_OF (message), L"%s\r\n%s\r\n%s", _r_locale_getstring (title), counts, observed);
    if (hwnd)
    {
        _r_ctrl_enable (hwnd, IDC_CLEAN, TRUE);
        if (_r_config_getboolean (L"BalloonCleanResults", TRUE))
        {
            if (!_r_config_getboolean (L"IsNotificationsSound", TRUE)) flags |= NIIF_NOSOUND;
            _r_tray_popup (hwnd, &GUID_TrayIcon, flags, _r_app_getname (), message);
        }
    }
    else _r_show_message (NULL, MB_OK | (job->report.failed ? MB_ICONWARNING : MB_ICONINFORMATION), NULL, message);
    if (_r_config_getboolean (L"LogCleanResults", FALSE))
        _r_log_v (LOG_LEVEL_INFO, NULL, L"Cleanup result", 0, L"source=%u succeeded=%u failed=%u skipped=%u observed_bytes=%llu duration_ms=%llu",
            job->source, job->report.succeeded, job->report.failed, job->report.skipped, job->report.observed_bytes, job->report.elapsed_ms);
    InterlockedExchange (&cleanup_busy, 0);
    _r_mem_free (job);
}
