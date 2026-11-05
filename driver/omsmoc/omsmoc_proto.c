/*
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Modifications (2026-10-28) relative to the original driver by
 * BlueApple233 <blueapple233_@outlook.com>
 * (https://github.com/BlueApple233/OMS-fingerprint-linux, LGPL-2.1-or-later):
 *   - extracted the EF01 frame codec from omsmoc.c into this file so that it
 *     can be unit-tested without hardware or libfprint private headers
 *   - added a standalone unit test for the frame codec (tests/test-omsmoc-proto.c)
 * OMS match-on-chip 协议编解码（EF01 帧）
 *
 * 纯字节操作，独立于 libfprint 私有的设备状态机；见 omsmoc_proto.h。
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "omsmoc_proto.h"

#ifdef OMS_PROTO_STANDALONE
#define OMS_WARN(...) g_warning (__VA_ARGS__)
#else
#include "fpi-log.h"
#define OMS_WARN(...) fp_warn (__VA_ARGS__)
#endif

/* 校验范围：从类型字节（偏移 6）到负载末尾，累加后取低 16 位。 */
static guint16
oms_checksum (const guint8 *buf,
              gsize         start,
              gsize         end)
{
  guint16 checksum = 0;
  gsize i;

  for (i = start; i < end; i++)
    checksum += buf[i];

  return checksum;
}

gsize
oms_build_frame (guint8        *buf,
                 guint8         cmd,
                 const guint8  *params,
                 gsize          params_len)
{
  gsize len;
  guint16 checksum;

  g_return_val_if_fail (params_len <= OMS_MAX_PARAM_SIZE, 0);

  len = 1 + params_len + OMS_FRAME_TRAILER_SIZE;

  memset (buf, 0x00, OMS_PACKET_SIZE);
  buf[0] = 0xef;
  buf[1] = 0x01;
  buf[2] = 0xff;
  buf[3] = 0xff;
  buf[4] = 0xff;
  buf[5] = 0xff;
  buf[6] = OMS_FRAME_KIND_REQUEST;
  buf[7] = 0x00;
  buf[8] = len;
  buf[9] = cmd;
  if (params_len > 0)
    memcpy (&buf[10], params, params_len);

  checksum = oms_checksum (buf, 6,
                           OMS_FRAME_HEADER_SIZE + len - OMS_FRAME_TRAILER_SIZE);

  buf[OMS_FRAME_HEADER_SIZE + len - 2] = (checksum >> 8) & 0xff;
  buf[OMS_FRAME_HEADER_SIZE + len - 1] = checksum & 0xff;

  return OMS_FRAME_HEADER_SIZE + len;
}

gboolean
oms_parse_frame (const guint8  *buf,
                 gsize          buf_len,
                 const guint8 **payload,
                 gsize         *payload_len)
{
  gsize frame_len;
  guint16 checksum;
  guint16 expected;

  if (buf_len < OMS_FRAME_HEADER_SIZE)
    return FALSE;

  if (buf[0] != 0xef || buf[1] != 0x01 || buf[2] != 0xff || buf[3] != 0xff ||
      buf[4] != 0xff || buf[5] != 0xff || buf[6] != OMS_FRAME_KIND_RESPONSE)
    return FALSE;

  frame_len = buf[8];
  if (frame_len < OMS_FRAME_TRAILER_SIZE ||
      OMS_FRAME_HEADER_SIZE + frame_len > buf_len)
    return FALSE;

  checksum = oms_checksum (buf, 6,
                           OMS_FRAME_HEADER_SIZE + frame_len - OMS_FRAME_TRAILER_SIZE);

  expected = (buf[OMS_FRAME_HEADER_SIZE + frame_len - 2] << 8) |
             buf[OMS_FRAME_HEADER_SIZE + frame_len - 1];
  if (checksum != expected)
    {
      OMS_WARN ("Response frame checksum mismatch (0x%04x != 0x%04x)",
                checksum, expected);
      return FALSE;
    }

  *payload = &buf[OMS_FRAME_HEADER_SIZE];
  *payload_len = frame_len - OMS_FRAME_TRAILER_SIZE;

  return TRUE;
}

/* The device pads its answers to the requested window size and reports
 * "nothing to say" with an all zero packet. */
gboolean
oms_data_is_empty (const guint8 *data,
                   gsize         len)
{
  gsize i;

  for (i = 0; i < len; i++)
    {
      if (data[i] != 0x00)
        return FALSE;
    }

  return TRUE;
}
