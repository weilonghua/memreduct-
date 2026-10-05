#include "cleanup_engine.h"
#include <string.h>

uint32_t app_cleanup_automatic_mask(uint32_t selected, int allow_expensive)
{
    selected &= REDUCT_MASK_ALL & ~REDUCT_MODIFIEDFILECACHE;
    return allow_expensive ? selected : selected & ~REDUCT_MASK_FREEZES;
}

void app_cleanup_run(uint32_t mask, const APP_CLEANUP_OPERATIONS *operations, APP_CLEANUP_REPORT *report)
{
    uint64_t before = 0, after = 0;
    uint64_t started = operations->tick(operations->context);
    int before_valid = operations->sample(operations->context, &before);
    memset(report, 0, sizeof(*report));
    mask &= REDUCT_MASK_ALL;
    for (unsigned int i = 0; i < 8; ++i)
    {
        uint32_t step = 1u << i;
        APP_STEP_RESULT result;
        if (!(mask & step)) continue;
        result = operations->cancelled(operations->context) ? APP_STEP_SKIPPED : operations->perform(operations->context, step);
        if (result == APP_STEP_SUCCESS) ++report->succeeded;
        else if (result == APP_STEP_FAILED) ++report->failed;
        else ++report->skipped;
    }
    report->memory_valid = operations->sample(operations->context, &after) && before_valid;
    if (report->memory_valid && after > before) report->observed_bytes = after - before;
    report->elapsed_ms = operations->tick(operations->context) - started;
}
