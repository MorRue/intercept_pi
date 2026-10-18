/******************************************************************************
 * Intercept plugin for OpenCPN -- parsing typed latitude / longitude.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "position_parse.h"

#include <cctype>
#include <cmath>
#include <exception>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace {

/** Parses token as a number, requiring the whole token to be consumed. */
bool ParseNumber(const std::string& token, double* out) {
  if (token.empty()) return false;
  try {
    size_t consumed = 0;
    double value = std::stod(token, &consumed);
    if (consumed != token.size()) return false;
    *out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

/**
 * Splits a trailing hemisphere letter off the last token, either as its
 * own token ("N") or attached to a number ("30.5N"). Returns '\0' (and
 * leaves tokens untouched) if the last token does not end in a letter.
 */
char ExtractHemisphere(std::vector<std::string>* tokens) {
  if (tokens->empty()) return '\0';
  std::string& last = tokens->back();
  if (last.empty() || !std::isalpha(static_cast<unsigned char>(last.back())))
    return '\0';
  char letter = std::toupper(static_cast<unsigned char>(last.back()));
  if (last.size() == 1)
    tokens->pop_back();
  else
    last.pop_back();
  return letter;
}

}  // namespace

PositionParseResult ParseCoordinate(const wxString& text, bool is_latitude) {
  PositionParseResult result;

  // OpenCPN shows positions as "45° 30.5000' N" or "45° 30' 30.000\" N":
  // treat the degree / minute / second marks as spaces so a position copied
  // from it parses.
  wxString cleaned = text;
  for (wxUniChar mark : {wxUniChar(0x00B0), wxUniChar(0x00BA), wxUniChar('\''),
                         wxUniChar('"'), wxUniChar(0x2032), wxUniChar(0x2033)}) {
    cleaned.Replace(wxString(mark), wxT(" "));
  }

  std::istringstream iss(cleaned.ToStdString());
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) tokens.push_back(token);

  if (tokens.empty()) {
    result.error = _("Position is empty.");
    return result;
  }

  char hemisphere = ExtractHemisphere(&tokens);
  bool has_hemisphere = hemisphere != '\0';
  if (has_hemisphere && hemisphere != 'N' && hemisphere != 'S' &&
      hemisphere != 'E' && hemisphere != 'W') {
    result.error =
        wxString::Format(_("'%c' is not a hemisphere letter (use N, S, E "
                            "or W)."),
                          hemisphere);
    return result;
  }
  if (has_hemisphere) {
    bool letter_is_lat = (hemisphere == 'N' || hemisphere == 'S');
    if (letter_is_lat != is_latitude) {
      result.error = is_latitude
          ? wxString::Format(
                _("'%c' is not valid for latitude; use N or S."), hemisphere)
          : wxString::Format(
                _("'%c' is not valid for longitude; use E or W."),
                hemisphere);
      return result;
    }
  }

  if (tokens.empty() || tokens.size() > 3) {
    result.error = wxString::Format(
        _("Expected 1 to 3 numbers (degrees[, minutes[, seconds]]), "
          "found %zu."),
        tokens.size());
    return result;
  }

  std::vector<double> values;
  for (const auto& t : tokens) {
    double v;
    if (!ParseNumber(t, &v)) {
      result.error =
          wxString::Format(_("'%s' is not a number."), t.c_str());
      return result;
    }
    values.push_back(v);
  }

  double max_degrees = is_latitude ? 90.0 : 180.0;
  double degrees = 0.0;

  if (values.size() == 1) {
    // DD: a single signed value, hemisphere letter is optional.
    degrees = values[0];
    if (has_hemisphere) {
      if (degrees < 0) {
        result.error = _(
            "A negative value cannot be combined with a hemisphere letter.");
        return result;
      }
      if (hemisphere == 'S' || hemisphere == 'W') degrees = -degrees;
    }
  } else {
    // DDM or DMS: hemisphere letter is required, degrees/minutes/seconds
    // are all non-negative magnitudes.
    if (!has_hemisphere) {
      result.error = is_latitude
          ? _("Latitude in degrees-minutes(-seconds) format needs a "
              "hemisphere letter (N or S).")
          : _("Longitude in degrees-minutes(-seconds) format needs a "
              "hemisphere letter (E or W).");
      return result;
    }
    double deg_part = values[0];
    if (deg_part < 0) {
      result.error =
          _("Degrees must not be negative; use a hemisphere letter "
            "instead.");
      return result;
    }
    double min_part = values[1];
    if (min_part < 0 || min_part >= 60.0) {
      result.error = wxString::Format(
          _("Minutes must be within [0, 60), got %g."), min_part);
      return result;
    }
    double sec_part = 0.0;
    if (values.size() == 3) {
      sec_part = values[2];
      if (sec_part < 0 || sec_part >= 60.0) {
        result.error = wxString::Format(
            _("Seconds must be within [0, 60), got %g."), sec_part);
        return result;
      }
    }
    if (deg_part > max_degrees) {
      result.error = wxString::Format(
          _("Degrees must be within [0, %g], got %g."), max_degrees,
          deg_part);
      return result;
    }
    degrees = deg_part + min_part / 60.0 + sec_part / 3600.0;
    if (hemisphere == 'S' || hemisphere == 'W') degrees = -degrees;
  }

  if (std::fabs(degrees) > max_degrees) {
    result.error = is_latitude
        ? wxString::Format(_("Latitude must be within [-90, 90], got %g."),
                            degrees)
        : wxString::Format(
              _("Longitude must be within [-180, 180], got %g."), degrees);
    return result;
  }

  result.ok = true;
  result.degrees = degrees;
  return result;
}

bool SplitPosition(const wxString& text, wxString* lat_text,
                   wxString* lon_text) {
  wxString trimmed = text;
  trimmed.Trim(true).Trim(false);

  int comma = trimmed.Find(',');
  if (comma != wxNOT_FOUND) {
    *lat_text = trimmed.Mid(0, comma);
    *lon_text = trimmed.Mid(comma + 1);
  } else {
    size_t split_at = wxString::npos;
    for (size_t i = 0; i < trimmed.Length(); ++i) {
      wxChar c = wxToupper(trimmed[i]);
      if (c == 'N' || c == 'S') {
        split_at = i + 1;
        break;
      }
    }
    if (split_at == wxString::npos) return false;
    *lat_text = trimmed.Mid(0, split_at);
    *lon_text = trimmed.Mid(split_at);
  }
  lat_text->Trim(true).Trim(false);
  lon_text->Trim(true).Trim(false);
  return !lat_text->IsEmpty() && !lon_text->IsEmpty();
}
