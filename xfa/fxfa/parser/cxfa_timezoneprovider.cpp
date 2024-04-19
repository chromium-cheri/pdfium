// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_timezoneprovider.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "build/build_config.h"

static bool g_bProviderTimeZoneSet = false;
#if defined(OS_FREEBSD)
static long g_lTimeZoneOffset = 0;
#endif

#if BUILDFLAG(IS_WIN)
#define TIMEZONE _timezone
#define TZSET _tzset
#else
#define TZSET tzset
#define TIMEZONE timezone
#endif

CXFA_TimeZoneProvider::CXFA_TimeZoneProvider() {
  if (!g_bProviderTimeZoneSet) {
    g_bProviderTimeZoneSet = true;
#if defined(OS_FREEBSD)
    time_t now = time(nullptr);
    struct tm tm = {};

    localtime_r(&now, &tm);
    g_lTimeZoneOffset = tm.tm_gmtoff;
#else
    TZSET();
#endif
  }
#if defined(OS_FREEBSD)
  tz_minutes_ = static_cast<int8_t>((abs(g_lTimeZoneOffset) % 3600) / 60);
#else
  tz_minutes_ = TIMEZONE / -60;
#endif
}

CXFA_TimeZoneProvider::~CXFA_TimeZoneProvider() = default;
