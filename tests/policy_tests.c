#include "update_policy.h"
#include "cleanup_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void manifest(const char *input, int valid, const char *expected)
{
    char version[32];
    assert(app_update_parse_manifest(input, strlen(input), version) == valid);
    if (valid) assert(!strcmp(version, expected));
}

typedef struct {
    uint32_t called, fail, skip;
    unsigned int samples, ticks, calls, cancel_after;
    int invalid_sample;
    uint64_t before, after;
} FAKE;
static APP_STEP_RESULT perform(void *context, uint32_t mask)
{
    FAKE *fake = context;
    assert(!(fake->called & mask));
    fake->called |= mask;
    ++fake->calls;
    return (fake->fail & mask) ? APP_STEP_FAILED : (fake->skip & mask) ? APP_STEP_SKIPPED : APP_STEP_SUCCESS;
}
static int cancelled(void *context)
{
    FAKE *fake = context;
    return fake->calls >= fake->cancel_after;
}
static int sample(void *context, uint64_t *available)
{
    FAKE *fake = context;
    *available = fake->samples++ ? fake->after : fake->before;
    return !fake->invalid_sample;
}
static uint64_t tick(void *context)
{
    FAKE *fake = context;
    return fake->ticks++ ? 1250 : 1000;
}
static APP_CLEANUP_REPORT run(FAKE *fake, uint32_t mask)
{
    APP_CLEANUP_REPORT report;
    APP_CLEANUP_OPERATIONS ops = {fake, perform, cancelled, sample, tick};
    app_cleanup_run(mask, &ops, &report);
    return report;
}

int main(void)
{
    char version[32], oversized[4097], embedded[] = "memreduct=3.6|https://example.org;";
    manifest("memreduct=3.5.3|https://example.org/app.exe;\r\nmemreduct.lng=1|https://example.org/lang;", 1, "3.5.3");
    manifest(" \nmemreduct=65535.65535.65535.65535|javascript:ignored;\n", 1, "65535.65535.65535.65535");
    manifest("memreduct=3.6|file://ignored;", 1, "3.6"); // URLs have no authority.
    manifest("memreduct=3.6|https://example.org", 0, ""); // Truncated row.
    manifest("memreduct=3.6|a;memreduct=3.7|b;", 0, "");
    manifest("memreduct=3.6|a;incomplete", 0, "");
    manifest("memreduct=65536|a;", 0, "");
    manifest("memreduct=999999999999999999999999|a;", 0, "");
    manifest("memreduct=1.2.3.4.5|a;", 0, "");
    manifest("memreduct=1..2|a;", 0, "");
    manifest("memreduct=1.|a;", 0, "");
    manifest("memreduct=-1|a;", 0, "");
    manifest("memreduct=3.6\ncmd|a;", 0, "");
    manifest("other=3.6|a;", 0, "");
    manifest("memreduct=3.6;", 0, "");
    manifest("", 0, "");
    embedded[4] = 0;
    assert(!app_update_parse_manifest(embedded, sizeof(embedded) - 1, version));
    memset(oversized, 'x', sizeof(oversized));
    assert(!app_update_parse_manifest(oversized, sizeof(oversized), version));
    assert(!app_update_parse_manifest(NULL, 1, version));
    assert(app_update_version_compare("3.10", "3.9.99") > 0);
    assert(app_update_version_compare("3.5.3", "3.5.3.0") == 0);
    assert(app_update_version_compare("3.5.2", "3.5.3") < 0);
    assert(app_update_version_compare("65536", "3") == 0);

    assert(!(app_cleanup_automatic_mask(0xffffffffu, 0) & (REDUCT_MODIFIEDFILECACHE | REDUCT_MASK_FREEZES)));
    assert(app_cleanup_automatic_mask(REDUCT_MASK_ALL, 1) == (REDUCT_MASK_ALL & ~REDUCT_MODIFIEDFILECACHE));
    assert(app_cleanup_automatic_mask(REDUCT_MASK_DEFAULT, 0) == REDUCT_MASK_DEFAULT);
    FAKE all = {.cancel_after = 100, .before = 100, .after = 500};
    APP_CLEANUP_REPORT report = run(&all, 0xffffffffu);
    assert(all.called == REDUCT_MASK_ALL && report.succeeded == 8 && report.failed == 0 && report.skipped == 0);
    assert(report.memory_valid && report.observed_bytes == 400 && report.elapsed_ms == 250);
    FAKE mixed = {.fail = REDUCT_WORKINGSET, .skip = REDUCT_REGISTRYCACHE, .cancel_after = 100, .before = 500, .after = 100};
    report = run(&mixed, REDUCT_WORKINGSET | REDUCT_REGISTRYCACHE | REDUCT_SYSTEMFILECACHE);
    assert(report.succeeded == 1 && report.failed == 1 && report.skipped == 1 && report.observed_bytes == 0);
    FAKE stopped = {.cancel_after = 1};
    report = run(&stopped, REDUCT_MASK_ALL);
    assert(stopped.called == REDUCT_WORKINGSET && report.succeeded == 1 && report.skipped == 7);
    FAKE invalid = {.invalid_sample = 1, .cancel_after = 100};
    report = run(&invalid, 0);
    assert(!report.memory_valid && !invalid.called && !report.succeeded && !report.failed && !report.skipped);
    puts("PASS: update metadata/version, automatic cleanup policy, mixed results, cancellation and memory measurement");
    return 0;
}
