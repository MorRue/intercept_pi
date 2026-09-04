/******************************************************************************
 * Intercept plugin for OpenCPN -- chart overlay drawing.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "overlay_draw.h"

namespace {

constexpr int kMarkerRadiusPx = 3;

const wxColour kOverlayColour(220, 30, 30);

void DrawMarker(wxDC& dc, const wxPoint& p) {
  dc.DrawCircle(p, kMarkerRadiusPx);
}

}  // namespace

void DrawInterceptOverlay(wxDC& dc, const OverlayViewport& vp,
                           const OverlayState& state) {
  if (!state.has_datum && state.route.empty()) return;
  if (!vp.LatLonToPix) return;

  dc.SetPen(wxPen(kOverlayColour));
  dc.SetBrush(wxBrush(kOverlayColour));

  if (state.route.size() >= 2) {
    std::vector<wxPoint> pts;
    pts.reserve(state.route.size());
    for (const auto& ll : state.route)
      pts.push_back(vp.LatLonToPix(ll.first, ll.second));
    for (size_t i = 0; i + 1 < pts.size(); ++i) dc.DrawLine(pts[i], pts[i + 1]);
    // DrawLine's toolkit convention leaves the second endpoint undrawn, so
    // mark every vertex explicitly -- callers can rely on exact endpoint
    // pixels being set.
    for (const auto& p : pts) DrawMarker(dc, p);
  }

  if (state.has_datum) {
    DrawMarker(dc, vp.LatLonToPix(state.datum_lat, state.datum_lon));
    // TODO(Next #4, CLAUDE.md): render the uncertainty ring at
    // state.datum_radius_nm once Case carries a real value for it.
  }
}
