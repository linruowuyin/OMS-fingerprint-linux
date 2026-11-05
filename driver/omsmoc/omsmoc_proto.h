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
 * 这些函数是纯函数：只做字节操作，不接触设备状态，因此可以脱离硬件和
 * libfprint 私有头文件单独做单元测试（见 tests/test-omsmoc-proto.c）。
 *
 * 本文件同时被驱动（omsmoc.c）和测试使用。
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef OMSMOC_PROTO_H
#define OMSMOC_PROTO_H

#include <glib.h>

/* 一次 USB 批量传输的固定包长（设备按 64 字节窗口应答） */
#define OMS_PACKET_SIZE 64

/* EF01 帧布局：9 字节头 + 负载 + 2 字节校验和 */
#define OMS_FRAME_HEADER_SIZE   9
#define OMS_FRAME_TRAILER_SIZE  2
#define OMS_FRAME_KIND_REQUEST  0x01
#define OMS_FRAME_KIND_RESPONSE 0x07
#define OMS_MAX_PARAM_SIZE      (OMS_PACKET_SIZE - OMS_FRAME_HEADER_SIZE - 1 - OMS_FRAME_TRAILER_SIZE)

/* 构造一个请求帧，返回写入的总字节数；参数过长时返回 0。 */
gsize    oms_build_frame (guint8        *buf,
                          guint8         cmd,
                          const guint8  *params,
                          gsize          params_len);

/* 解析一个应答帧；成功时 payload 指向负载起点（不含头/尾）。 */
gboolean oms_parse_frame (const guint8  *buf,
                          gsize          buf_len,
                          const guint8 **payload,
                          gsize         *payload_len);

/* 全零包表示“设备无话可说”（设备会把应答补齐到窗口长度）。 */
gboolean oms_data_is_empty (const guint8 *data,
                            gsize         len);

#endif /* OMSMOC_PROTO_H */
