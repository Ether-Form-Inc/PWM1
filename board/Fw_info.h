// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#pragma once

// Firmware identification kept in flash right after the vector table (see the .fw_info section in the linker
// script), so the version can be read over SWD without any CAN traffic: look for the magic in the first 512 bytes
// of flash. Layout (little endian, 48 bytes): magic[8] "PWMFWINF", major, minor, patch, dirty (1 = built with
// uncommitted changes), vcs_commit (first 8 hex digits of the git commit as a number, 0 if unknown), version[32]
// text such as "0.3.0+1a2b3c4d-dirty".

#include <fw_version.h>

#include <cstdint>

struct Fw_info
{
  char magic[8];
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t dirty;
  uint32_t vcs_commit;
  char version[32];
};
static_assert(sizeof(Fw_info) == 48, "Fw_info is read by tools/flash_and_check.py");

// Defined once, in main.cpp
extern const Fw_info fw_info;
