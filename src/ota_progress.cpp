#include "ota_progress.h"

#include <ctype.h>
#include <string.h>

static int status_base_progress(const char *status)
{
    if (status == nullptr)
        return 0;

    if (strcmp(status, "checking") == 0)
        return 5;
    if (strcmp(status, "available") == 0)
        return 10;
    if (strcmp(status, "downloading") == 0)
        return 25;
    if (strcmp(status, "verifying") == 0)
        return 95;
    if (strcmp(status, "installing") == 0)
        return 97;
    if (strcmp(status, "rebooting") == 0 || strcmp(status, "booting") == 0)
        return 99;
    if (strcmp(status, "installed") == 0 || strcmp(status, "completed") == 0 ||
        strcmp(status, "uptodate") == 0 || strcmp(status, "skipped") == 0)
        return 100;
    if (strcmp(status, "failed") == 0 || strcmp(status, "triggered") == 0)
        return 0;

    return 0;
}

static void lowercase_copy(const char *src, char *dst, size_t dst_len)
{
    if (dst_len == 0)
        return;
    if (src == nullptr)
    {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    for (; src[i] != '\0' && i + 1 < dst_len; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

int firmngin_normalize_ota_progress(const char *status, const char *message)
{
    char status_lc[32];
    lowercase_copy(status, status_lc, sizeof(status_lc));
    int progress = status_base_progress(status_lc);

    if (message != nullptr)
    {
        int start = -1;
        size_t len = strlen(message);
        for (size_t i = 0; i < len; i++)
        {
            if (isdigit((unsigned char)message[i]))
            {
                start = (int)i;
                break;
            }
        }

        if (start >= 0)
        {
            int end = start;
            while ((size_t)end < len && isdigit((unsigned char)message[end]))
                end++;

            int parsed = 0;
            for (int i = start; i < end; i++)
                parsed = parsed * 10 + (message[i] - '0');

            if (parsed >= 0 && parsed <= 100)
                progress = parsed;
            else if (parsed > 100)
                progress = 100;
        }
    }

    if (progress < 0)
        progress = 0;
    if (progress > 100)
        progress = 100;
    return progress;
}
