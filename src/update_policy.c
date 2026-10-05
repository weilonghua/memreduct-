#include "update_policy.h"
#include <stdint.h>
#include <string.h>

static int parse_version (const char *text, uint32_t parts[4])
{
    unsigned int index = 0;
    unsigned int digits = 0;
    size_t length = strlen (text);
    if (!length || length >= 32)
        return 0;
    memset (parts, 0, sizeof (uint32_t) * 4);
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char value = (unsigned char)text[i];
        if (value == '.')
        {
            if (!digits || index == 3)
                return 0;
            ++index;
            digits = 0;
        }
        else if (value >= '0' && value <= '9')
        {
            uint32_t digit = (uint32_t)(value - '0');
            if (parts[index] > (65535u - digit) / 10u)
                return 0;
            parts[index] = parts[index] * 10u + digit;
            ++digits;
        }
        else
            return 0;
    }
    return digits != 0;
}

int app_update_version_compare (const char *left, const char *right)
{
    uint32_t a[4], b[4];
    if (!parse_version (left, a) || !parse_version (right, b))
        return 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

int app_update_parse_manifest (const char *data, size_t length, char version[32])
{
    const char key[] = "memreduct=";
    size_t position = 0;
    int found = 0;
    version[0] = '\0';
    if (!data || !length || length > 4096)
        return 0;
    // VERSION is an ASCII document. Reject embedded NUL and control characters.
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = (unsigned char)data[i];
        if ((c < 32 && c != '\r' && c != '\n' && c != '\t') || c > 126)
            return 0;
    }
    while (position < length)
    {
        size_t end = position;
        while (end < length && data[end] != ';') ++end;
        if (end == length) return 0; // incomplete response
        while (position < end && (data[position] == '\r' || data[position] == '\n' || data[position] == ' ' || data[position] == '\t'))
            ++position;
        if (end - position >= sizeof (key) - 1 && !memcmp (data + position, key, sizeof (key) - 1))
        {
            uint32_t parts[4];
            size_t begin = position + sizeof (key) - 1;
            size_t stop = begin;
            if (found) return 0;
            while (stop < end && data[stop] != '|') ++stop;
            if (stop == end || stop == begin || stop - begin >= 32)
                return 0;
            memcpy (version, data + begin, stop - begin);
            version[stop - begin] = '\0';
            if (!parse_version (version, parts)) return 0;
            found = 1;
        }
        position = end + 1;
        while (position < length && (data[position] == '\r' || data[position] == '\n' || data[position] == ' ' || data[position] == '\t'))
            ++position;
    }
    return found;
}
