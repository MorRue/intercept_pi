/******************************************************************************
 * Intercept plugin for OpenCPN -- chart overlay drawing.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#ifndef INTERCEPT_PI_OVERLAY_DRAW_H__
#define INTERCEPT_PI_OVERLAY_DRAW_H__

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <functional>
#include <utility>
#include <vector>

/**
 * Pixel-space projection the overlay needs, supplied by the caller. Kept as
 * a std::function rather than a template parameter so DrawInterceptOverlay
 * can live in an ordinary, separately-compiled .cpp: the plugin passes
 * OpenCPN's GetCanvasPixLL(), test/test_overlay.cpp passes a fake.
 */
struct OverlayViewport {
  wxSize pixel_size;
  std::function<wxPoint(double lat, double lon)> LatLonToPix;
};

/**
 * Everything DrawInterceptOverlay needs from a Case to draw the overlay.
 * has_datum false together with an empty route means "nothing to draw" --
 * distinct from a datum that legitimately sits at (0, 0). datum_radius_nm
 * is the uncertainty radius E (Next #4 in CLAUDE.md); carried here already
 * but not yet rendered -- that lands with the uncertainty-circle drawing.
 */
struct OverlayState {
  bool has_datum = false;
  double datum_lat = 0.0;
  double datum_lon = 0.0;
  double datum_radius_nm = 0.0;
  std::vector<std::pair<double, double>> route;  // (lat, lon), in draw order
};

/**
 * Renders the intercept overlay into dc: a marker at the datum, and the
 * course-to-steer route as a connected line with a marker at every vertex.
 * Draws nothing when state has neither a datum nor a route point. Safe to
 * call with a datum or route point that projects off the visible canvas.
 */
void DrawInterceptOverlay(wxDC& dc, const OverlayViewport& vp,
                           const OverlayState& state);

#endif  // INTERCEPT_PI_OVERLAY_DRAW_H__
