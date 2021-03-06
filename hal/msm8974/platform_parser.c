/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "platform_parser"
#define LOG_NDDEBUG 0

#include <errno.h>
#include <stdio.h>
#include <expat.h>
#include <cutils/log.h>
#include <audio_hw.h>
#include <platform_api.h>
#include "platform.h"
#include "platform_parser.h"

#define PLATFORM_XML_PATH        "/system/etc/platform_info.xml"
#define BUF_SIZE                    1024

static void process_device(void *userdata, const XML_Char **attr)
{
    unsigned int    *snd_device_index = userdata;

    if (strcmp(attr[0], "name") != 0)
        goto done;

    if (platform_get_snd_device_name(*snd_device_index) == NULL)
        goto next;
    if (strcmp(attr[1], platform_get_snd_device_name(*snd_device_index)) != 0) {
        ALOGE("%s: %s in platform.h at index %d does not match %s, from %s no ACDB ID set!",
            __func__, platform_get_snd_device_name(*snd_device_index),
            *snd_device_index, attr[1], PLATFORM_XML_PATH);
        goto done;
    }

    if (strcmp(attr[2], "acdb_id") != 0) {
        ALOGE("%s: Device %s at index %d in %s has no acdb_id, no ACDB ID set!",
              __func__, attr[1], *snd_device_index, PLATFORM_XML_PATH);
        goto done;
    }

    if(platform_set_snd_device_acdb_id(*snd_device_index,
                                atoi((char *)attr[3])) != 0)
        goto done;

next:
     (*snd_device_index)++;
done:
    return;
}

static void start_tag(void *userdata, const XML_Char *tag_name,
                      const XML_Char **attr)
{
    const XML_Char              *attr_name = NULL;
    const XML_Char              *attr_value = NULL;
    unsigned int                i;

    if (strcmp(tag_name, "device") == 0)
        process_device(userdata, attr);

    return;
}

static void end_tag(void *userdata, const XML_Char *tag_name)
{

}

int platform_info_init(void)
{
    XML_Parser      parser;
    FILE            *file;
    int             ret = 0;
    int             bytes_read;
    unsigned int    snd_device_index = SND_DEVICE_MIN;
    void            *buf;

    file = fopen(PLATFORM_XML_PATH, "r");
    if (!file) {
        ALOGD("%s: Failed to open %s, using defaults.",
            __func__, PLATFORM_XML_PATH);
        ret = -ENODEV;
        goto done;
    }

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        ALOGE("%s: Failed to create XML parser!", __func__);
        ret = -ENODEV;
        goto err_close_file;
    }

    XML_SetUserData(parser, &snd_device_index);
    XML_SetElementHandler(parser, start_tag, end_tag);

    while (1) {
        buf = XML_GetBuffer(parser, BUF_SIZE);
        if (buf == NULL) {
            ALOGE("%s: XML_GetBuffer failed", __func__);
            ret = -ENOMEM;
            goto err_free_parser;
        }

        bytes_read = fread(buf, 1, BUF_SIZE, file);
        if (bytes_read < 0) {
            ALOGE("%s: fread failed, bytes read = %d", __func__, bytes_read);
             ret = bytes_read;
            goto err_free_parser;
        }

        if (XML_ParseBuffer(parser, bytes_read,
                            bytes_read == 0) == XML_STATUS_ERROR) {
            ALOGE("%s: XML_ParseBuffer failed, for %s",
                __func__, PLATFORM_XML_PATH);
            ret = -EINVAL;
            goto err_free_parser;
        }

        if (bytes_read == 0)
            break;
    }

    if (snd_device_index != SND_DEVICE_MAX) {
        ALOGE("%s: Only %d/%d ACDB ID's set! Fix %s!",
            __func__, snd_device_index, SND_DEVICE_MAX, PLATFORM_XML_PATH);
        ret = -EINVAL;
    }

err_free_parser:
    XML_ParserFree(parser);
err_close_file:
    fclose(file);
done:
    return ret;
}
