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
 * omsmoc 帧编解码的单元测试（不需要硬件）
 *
 * 只测试 omsmoc/omsmoc_proto.c 里的纯函数：帧构造、帧解析、空包判断。
 * 这些用例覆盖了驱动与芯片通信里最容易出错的边界：长度、magic、
 * 校验和、截断包、空负载。
 *
 * 两种跑法：
 *   driver/tests/run-proto-tests.sh          单文件编译，不依赖 libfprint 源码树
 *   meson test -C build/libfprint-build omsmoc-proto   在 libfprint 树里跑
 */

#include <glib.h>
#include "omsmoc_proto.h"

/* 在 libfprint 源码树里编译时，整个项目带 -DG_LOG_DOMAIN="libfprint"，
 * 警告的日志域就是 libfprint；独立编译时域是 NULL。
 * g_test_expect_message() 必须用同一个域才能匹配到预期日志。 */
#ifndef G_LOG_DOMAIN
#define G_LOG_DOMAIN NULL
#endif

/* 构造一个应答帧（kind=0x07），用于喂给 oms_parse_frame()。 */
static gsize
build_response (guint8        *buf,
                const guint8  *payload,
                gsize          payload_len)
{
  gsize frame_len = payload_len + OMS_FRAME_TRAILER_SIZE;
  guint16 checksum = 0;
  gsize i;

  g_assert_cmpuint (OMS_FRAME_HEADER_SIZE + frame_len, <=, OMS_PACKET_SIZE);

  memset (buf, 0x00, OMS_PACKET_SIZE);
  buf[0] = 0xef;
  buf[1] = 0x01;
  buf[2] = 0xff;
  buf[3] = 0xff;
  buf[4] = 0xff;
  buf[5] = 0xff;
  buf[6] = OMS_FRAME_KIND_RESPONSE;
  buf[7] = 0x00;
  buf[8] = frame_len;
  if (payload_len > 0)
    memcpy (&buf[OMS_FRAME_HEADER_SIZE], payload, payload_len);

  for (i = 6; i < OMS_FRAME_HEADER_SIZE + frame_len - OMS_FRAME_TRAILER_SIZE; i++)
    checksum += buf[i];

  buf[OMS_FRAME_HEADER_SIZE + frame_len - 2] = (checksum >> 8) & 0xff;
  buf[OMS_FRAME_HEADER_SIZE + frame_len - 1] = checksum & 0xff;

  return OMS_FRAME_HEADER_SIZE + frame_len;
}

static void
test_constants (void)
{
  g_assert_cmpuint (OMS_FRAME_HEADER_SIZE, ==, 9);
  g_assert_cmpuint (OMS_FRAME_TRAILER_SIZE, ==, 2);
  g_assert_cmpuint (OMS_MAX_PARAM_SIZE, ==, 52);
  /* 满负载时整帧正好填满一个 USB 包 */
  g_assert_cmpuint (OMS_FRAME_HEADER_SIZE + 1 + OMS_MAX_PARAM_SIZE +
                    OMS_FRAME_TRAILER_SIZE, ==, OMS_PACKET_SIZE);
}

static void
test_build_no_params (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  gsize len = oms_build_frame (buf, 0x34, NULL, 0);

  g_assert_cmpuint (len, ==, 12);            /* 9 + (1 + 0 + 2) */
  g_assert_cmpuint (buf[0], ==, 0xef);
  g_assert_cmpuint (buf[1], ==, 0x01);
  g_assert_cmpuint (buf[6], ==, OMS_FRAME_KIND_REQUEST);
  g_assert_cmpuint (buf[7], ==, 0x00);
  g_assert_cmpuint (buf[8], ==, 3);          /* 负载 + 校验和 */
  g_assert_cmpuint (buf[9], ==, 0x34);
  /* 校验和 = 0x01 + 0x00 + 0x03 + 0x34 */
  g_assert_cmpuint (buf[10], ==, 0x00);
  g_assert_cmpuint (buf[11], ==, 0x38);
  /* 包尾必须补零到 64 字节 */
  g_assert_cmpuint (buf[12], ==, 0x00);
  g_assert_cmpuint (buf[63], ==, 0x00);
}

static void
test_build_with_params (void)
{
  static const guint8 params[] = { 0x00, 0x01, 0x06, 0x00, 0x00 };
  guint8 buf[OMS_PACKET_SIZE];
  gsize len = oms_build_frame (buf, 0x32, params, sizeof params);

  g_assert_cmpuint (len, ==, 17);            /* 9 + (1 + 5 + 2) */
  g_assert_cmpuint (buf[8], ==, 8);
  g_assert_cmpuint (buf[9], ==, 0x32);
  g_assert_true (memcmp (&buf[10], params, sizeof params) == 0);
  g_assert_cmpuint (buf[15], ==, 0x00);      /* 参数之后才是校验和 */
}

static void
test_build_full_payload (void)
{
  guint8 params[OMS_MAX_PARAM_SIZE];
  guint8 buf[OMS_PACKET_SIZE];
  gsize len;

  memset (params, 0xa5, sizeof params);
  len = oms_build_frame (buf, 0x31, params, sizeof params);

  g_assert_cmpuint (len, ==, OMS_PACKET_SIZE);
  g_assert_true (memcmp (&buf[10], params, sizeof params) == 0);
}

static void
test_build_rejects_oversized (void)
{
  guint8 params[OMS_MAX_PARAM_SIZE + 1];
  guint8 buf[OMS_PACKET_SIZE];

  memset (params, 0, sizeof params);
  /* 必须返回 0，且不能越界写。g_return_val_if_fail 会打一条 critical，
   * 在 GLib 测试框架下 critical 默认是致命的，所以先声明预期消息。 */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*params_len*");
  g_assert_cmpuint (oms_build_frame (buf, 0x31, params, sizeof params), ==, 0);
  g_test_assert_expected_messages ();
}

static void
test_parse_roundtrip (void)
{
  static const guint8 payload[] = { 0x00, 0x05, 0x00, 0x00, 0x03, 0xe8 };
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = build_response (buf, payload, sizeof payload);

  g_assert_true (oms_parse_frame (buf, len, &out, &out_len));
  g_assert_cmpuint (out_len, ==, sizeof payload);
  g_assert_true (out == &buf[OMS_FRAME_HEADER_SIZE]);
  g_assert_true (memcmp (out, payload, sizeof payload) == 0);
}

static void
test_parse_accepts_empty_payload (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = build_response (buf, NULL, 0);

  g_assert_cmpuint (len, ==, OMS_FRAME_HEADER_SIZE + OMS_FRAME_TRAILER_SIZE);
  g_assert_true (oms_parse_frame (buf, len, &out, &out_len));
  g_assert_cmpuint (out_len, ==, 0);
}

static void
test_parse_rejects_short_buffer (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = build_response (buf, NULL, 0);

  /* 只有 8 字节，连帧头都不完整 */
  g_assert_false (oms_parse_frame (buf, OMS_FRAME_HEADER_SIZE - 1, &out, &out_len));
  /* 完整帧，但只交付了 len-1 字节 */
  g_assert_false (oms_parse_frame (buf, len - 1, &out, &out_len));
}

static void
test_parse_rejects_bad_magic (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = build_response (buf, NULL, 0);

  buf[0] = 0xee;
  g_assert_false (oms_parse_frame (buf, len, &out, &out_len));
}

static void
test_parse_rejects_request_kind (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = oms_build_frame (buf, 0x34, NULL, 0);

  /* 请求帧不能被当作应答接受 */
  g_assert_false (oms_parse_frame (buf, len, &out, &out_len));
}

static void
test_parse_rejects_short_frame_len (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;

  build_response (buf, NULL, 0);
  buf[8] = 0;                               /* 小于 trailer */
  g_assert_false (oms_parse_frame (buf, OMS_PACKET_SIZE, &out, &out_len));

  build_response (buf, NULL, 0);
  buf[8] = 1;
  g_assert_false (oms_parse_frame (buf, OMS_PACKET_SIZE, &out, &out_len));
}

static void
test_parse_rejects_truncated_frame (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;

  build_response (buf, NULL, 0);
  buf[8] = 60;                              /* 声明长度超过实际交付 */
  g_assert_false (oms_parse_frame (buf, OMS_PACKET_SIZE, &out, &out_len));
}

static void
test_parse_rejects_bad_checksum (void)
{
  guint8 buf[OMS_PACKET_SIZE];
  const guint8 *out = NULL;
  gsize out_len = 0;
  gsize len = build_response (buf, NULL, 0);

  buf[len - 1] ^= 0xff;
  /* 校验和不符时解析器会打一条 warning（GLib 测试里致命），先声明预期消息 */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*checksum mismatch*");
  g_assert_false (oms_parse_frame (buf, len, &out, &out_len));
  g_test_assert_expected_messages ();
}

static void
test_data_is_empty (void)
{
  static const guint8 zeros[] = { 0, 0, 0, 0 };
  static const guint8 mixed[] = { 0, 0, 1, 0 };

  g_assert_true (oms_data_is_empty (zeros, sizeof zeros));
  g_assert_false (oms_data_is_empty (mixed, sizeof mixed));
  /* 零长度视为空 */
  g_assert_true (oms_data_is_empty (zeros, 0));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/omsmoc/proto/constants", test_constants);
  g_test_add_func ("/omsmoc/proto/build/no-params", test_build_no_params);
  g_test_add_func ("/omsmoc/proto/build/with-params", test_build_with_params);
  g_test_add_func ("/omsmoc/proto/build/full-payload", test_build_full_payload);
  g_test_add_func ("/omsmoc/proto/build/oversized", test_build_rejects_oversized);
  g_test_add_func ("/omsmoc/proto/parse/roundtrip", test_parse_roundtrip);
  g_test_add_func ("/omsmoc/proto/parse/empty-payload", test_parse_accepts_empty_payload);
  g_test_add_func ("/omsmoc/proto/parse/short-buffer", test_parse_rejects_short_buffer);
  g_test_add_func ("/omsmoc/proto/parse/bad-magic", test_parse_rejects_bad_magic);
  g_test_add_func ("/omsmoc/proto/parse/request-kind", test_parse_rejects_request_kind);
  g_test_add_func ("/omsmoc/proto/parse/short-frame-len", test_parse_rejects_short_frame_len);
  g_test_add_func ("/omsmoc/proto/parse/truncated", test_parse_rejects_truncated_frame);
  g_test_add_func ("/omsmoc/proto/parse/bad-checksum", test_parse_rejects_bad_checksum);
  g_test_add_func ("/omsmoc/proto/data-is-empty", test_data_is_empty);

  return g_test_run ();
}
