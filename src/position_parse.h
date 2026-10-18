/******************************************************************************
 * Intercept plugin for OpenCPN -- parsing typed latitude / longitude.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_POSITION_PARSE_H__
#define INTERCEPT_PI_POSITION_PARSE_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

/** Outcome of parsing one coordinate (latitude or longitude) from text. */
struct PositionParseResult {
  bool ok = false;
  double degrees = 0.0;  // Valid only when ok is true. Signed: +N/+E, -S/-W.
  wxString error;         // Valid only when ok is false.
};

/**
 * Parses a single latitude or longitude given as DD ("45.5"),
 * DDM ("45 30.5 N") or DMS ("45 30 30 N"). The caller states which axis
 * is expected via is_latitude, since that fixes both the valid hemisphere
 * letters (N/S vs E/W) and the degree range (0-90 vs 0-180). Degree, minute
 * and second marks (° ' " and the typographic ′ ″) are accepted as
 * separators, so a position copied from OpenCPN ("45° 30.5000' N") parses.
 */
PositionParseResult ParseCoordinate(const wxString& text, bool is_latitude);

/**
 * Splits free-form position text into a latitude and a longitude part. A
 * comma is the preferred separator ("45 30.5 N, 015 20.3 E"); without one,
 * the split falls right after the first N/S hemisphere letter. Returns false
 * if it cannot find two non-empty parts.
 */
bool SplitPosition(const wxString& text, wxString* lat_text,
                   wxString* lon_text);

#endif  // INTERCEPT_PI_POSITION_PARSE_H__
