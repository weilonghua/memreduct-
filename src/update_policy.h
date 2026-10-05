#pragma once
#include <stddef.h>

// Only the version is consumed. URLs in the remote document are never executed.
int app_update_parse_manifest (const char *data, size_t length, char version[32]);
int app_update_version_compare (const char *left, const char *right);
