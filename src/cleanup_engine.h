#pragma once
#include <stdint.h>

#define REDUCT_WORKINGSET 0x01u
#define REDUCT_SYSTEMFILECACHE 0x02u
#define REDUCT_STANDBYPRIORITY0LIST 0x04u
#define REDUCT_STANDBYLIST 0x08u
#define REDUCT_MODIFIEDLIST 0x10u
#define REDUCT_COMBINEMEMORYLISTS 0x20u
#define REDUCT_REGISTRYCACHE 0x40u
#define REDUCT_MODIFIEDFILECACHE 0x80u
#define REDUCT_MASK_ALL 0xffu
#define REDUCT_MASK_DEFAULT (REDUCT_WORKINGSET | REDUCT_SYSTEMFILECACHE | REDUCT_STANDBYPRIORITY0LIST | REDUCT_REGISTRYCACHE | REDUCT_COMBINEMEMORYLISTS)
#define REDUCT_MASK_FREEZES (REDUCT_STANDBYLIST | REDUCT_MODIFIEDLIST)

typedef enum { APP_STEP_SUCCESS, APP_STEP_FAILED, APP_STEP_SKIPPED } APP_STEP_RESULT;
typedef struct {
    unsigned int succeeded, failed, skipped;
    uint64_t observed_bytes, elapsed_ms;
    int memory_valid;
} APP_CLEANUP_REPORT;
typedef struct {
    void *context;
    APP_STEP_RESULT (*perform)(void *context, uint32_t mask);
    int (*cancelled)(void *context);
    int (*sample)(void *context, uint64_t *available);
    uint64_t (*tick)(void *context);
} APP_CLEANUP_OPERATIONS;

uint32_t app_cleanup_automatic_mask(uint32_t selected, int allow_expensive);
void app_cleanup_run(uint32_t mask, const APP_CLEANUP_OPERATIONS *operations, APP_CLEANUP_REPORT *report);
