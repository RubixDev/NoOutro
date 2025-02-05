/*
 *   This file is part of Universal-Updater
 *   Copyright (C) 2019-2021 Universal-Team
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

// Based on Universal-Updater
// https://github.com/Universal-Team/Universal-Updater/blob/cfd764f955c7a85527af883d2ca5836e0e5597c3/source/utils/DownloadUtils.cpp

#include "download.h"

#include "version.h"
#include "wifi.h"

#include <curl/curl.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE (1 << 20) // 1 MiB

#define USER_AGENT ("dsidl-" VER_NUMBER)

extern bool verbose;

static size_t bufSize = 0;
static size_t bufWritten = 0;
static u8 *buffer = NULL;
static FILE *out = NULL;

static size_t handleData(const char *ptr, size_t size, size_t nmemb, const void *userData) {
    const size_t realSize = size * nmemb;

    if (bufWritten + realSize > bufSize) {
        if (!out)
            return 0;
        size_t bytes = fwrite(buffer, 1, bufWritten, out);
        if (bytes != bufWritten)
            return bytes;
        bufWritten = 0;
    }

    if (!buffer)
        return 0;
    memcpy(buffer + bufWritten, ptr, realSize);
    bufWritten += realSize;

    return realSize;
}

static int handleProgress(
    CURL *hnd, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow
) {
    iprintf("\x1B[2;0H%lld/%lld bytes\n", dlNow, dlTotal);
    if (dlTotal > 0) {
        char bar[31];
        bar[30] = 0;
        for (int i = 0; i < 30; i++)
            bar[i] = (dlNow * 30 / (dlTotal | 1) > i) ? '=' : ' ';
        iprintf("[%s]\n", bar);
    }

    return 0;
}

int handleLog(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    iprintf("\x1B[40m%.*s\x1B[47m", size, data);
    return 0;
}

int download(const char *url, const char *path) {
    if (!wifiConnected())
        return -1;

    int ret = 0;
    CURLcode cRes;

    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
    curl_easy_setopt(hnd, CURLOPT_URL, url);
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, handleData);
    curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, handleProgress);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(hnd, CURLOPT_VERBOSE, (long)verbose);
    curl_easy_setopt(hnd, CURLOPT_FAILONERROR, 1L);
    if (verbose)
        curl_easy_setopt(hnd, CURLOPT_DEBUGFUNCTION, handleLog);

    bufSize = BUF_SIZE;
    buffer = (u8 *)malloc(bufSize);
    if (!buffer) {
        ret = -2;
        goto cleanup;
    }

    out = fopen(path, "wb");
    if (!out) {
        ret = -3;
        goto cleanup;
    }

    consoleClear();

    cRes = curl_easy_perform(hnd);
    long response_code;
    curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(hnd);
    hnd = NULL;

    if (cRes == CURLE_HTTP_RETURNED_ERROR) {
        iprintf("Error: Server responded with non-success status code: %ld", response_code);
        ret = -5;
        goto cleanup;
    } else if (cRes != CURLE_OK) {
        iprintf("Error in:\ncurl\n");
        ret = -4;
        goto cleanup;
    }

    if (bufWritten > 0) {
        fwrite(buffer, 1, bufWritten, out);
    }

cleanup:
    if (out) {
        fclose(out);
        out = NULL;
    }

    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    if (hnd) {
        curl_easy_cleanup(hnd);
        hnd = NULL;
    }

    bufSize = 0;
    bufWritten = 0;

    return ret;
}

int downloadBuffer(const char *url, void *retBuffer, unsigned int size) {
    if (!wifiConnected() || !retBuffer)
        return -1;

    int ret = 0;
    CURLcode cRes;

    CURL *hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
    curl_easy_setopt(hnd, CURLOPT_URL, url);
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, handleData);
    curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, handleProgress);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(hnd, CURLOPT_VERBOSE, (long)verbose);
    if (verbose)
        curl_easy_setopt(hnd, CURLOPT_DEBUGFUNCTION, handleLog);

    bufSize = size;
    buffer = retBuffer;

    consoleClear();
    iprintf("%-32.32s\n", url);

    cRes = curl_easy_perform(hnd);
    curl_easy_cleanup(hnd);
    hnd = NULL;

    if (cRes != CURLE_OK) {
        iprintf("Error in:\ncurl\n");
        ret = -3;
        goto cleanup;
    }

cleanup:
    if (hnd) {
        curl_easy_cleanup(hnd);
        hnd = NULL;
    }

    bufSize = 0;
    bufWritten = 0;

    return ret;
}
