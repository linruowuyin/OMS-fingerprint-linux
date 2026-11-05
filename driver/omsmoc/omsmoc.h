/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * OMS Match-on-Chip driver for libfprint
 * Copyright (C) 2026 The OMS Linux driver contributors
 *
 * Modifications (2026-10-28) relative to the original driver by
 * BlueApple233 <blueapple233_@outlook.com>
 * (https://github.com/BlueApple233/OMS-fingerprint-linux, LGPL-2.1-or-later):
 *   - split the EF01 frame codec out of the driver into omsmoc_proto.c/.h so
 *     that it can be unit-tested without hardware or libfprint private headers
 *   - added a standalone unit test for the frame codec (tests/test-omsmoc-proto.c)
 *   - added a bring-up tool and a sanitized umockdev regression test
 *
 * The original copyright notice above is retained as required by the LGPL.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include "fpi-device.h"
#include "fpi-ssm.h"

G_DECLARE_FINAL_TYPE (FpiDeviceOmsMoc, fpi_device_omsmoc, FPI, DEVICE_OMS_MOC, FpDevice)
