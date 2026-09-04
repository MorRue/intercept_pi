/******************************************************************************
 * Intercept plugin for OpenCPN -- standalone smoke-test for the overlay
 * drawing harness (CLAUDE.md Next #3).
 *
 * Needs the wxWidgets "core" component (wxBitmap, wxMemoryDC), which pulls
 * in the toolkit's GUI init and therefore a display connection -- run this
 * under xvfb-run on a headless Linux box. See CMakeLists.txt.
 *
 * Copyright (C) 2026 momo
 * License: GPLv3+  (see COPYING)
 ******************************************************************************/

#include "overlay_draw.h"

#include <wx/app.h>
#include <wx/image.h>

#include <cstdio>

namespace {

int g_failures = 0;

// Sentinel: OnInit() never ran (e.g. wxWidgets' GTK backend couldn't reach
// a display), as opposed to a real 0/1 result from RunTests().
int g_exit_code = -1;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
  } else {
    std::printf("ok:   %s\n", what);
  }
}

constexpr int kWidth = 200;
constexpr int kHeight = 150;

bool IsBackground(const wxImage& img, int x, int y) {
  if (x < 0 || y < 0 || x >= img.GetWidth() || y >= img.GetHeight())
    return true;  // Off-canvas: nothing to inspect here.
  return img.GetRed(x, y) == 255 && img.GetGreen(x, y) == 255 &&
         img.GetBlue(x, y) == 255;
}

bool AnyNonBackgroundNear(const wxImage& img, const wxPoint& centre,
                           int radius) {
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      if (!IsBackground(img, centre.x + dx, centre.y + dy)) return true;
    }
  }
  return false;
}

bool AllBackground(const wxImage& img) {
  for (int y = 0; y < img.GetHeight(); ++y) {
    for (int x = 0; x < img.GetWidth(); ++x) {
      if (!IsBackground(img, x, y)) return false;
    }
  }
  return true;
}

wxImage RenderToImage(const OverlayViewport& vp, const OverlayState& state) {
  wxBitmap bitmap(vp.pixel_size);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(*wxWHITE_BRUSH);
  dc.Clear();
  DrawInterceptOverlay(dc, vp, state);
  dc.SelectObject(wxNullBitmap);
  return bitmap.ConvertToImage();
}

int RunTests() {
  const wxSize size(kWidth, kHeight);

  // A fixed linear projection: (lat, lon) in [0,1] maps onto the bitmap.
  // Real callers pass OpenCPN's GetCanvasPixLL(); this is the fake.
  auto project = [size](double lat, double lon) {
    return wxPoint(static_cast<int>(lon * size.GetWidth()),
                    static_cast<int>(lat * size.GetHeight()));
  };

  // (a) Non-empty state: a mark appears near the projected datum.
  {
    OverlayViewport vp{size, project};
    OverlayState state;
    state.has_datum = true;
    state.datum_lat = 0.5;
    state.datum_lon = 0.5;
    wxImage img = RenderToImage(vp, state);
    wxPoint expected = project(state.datum_lat, state.datum_lon);
    Check(AnyNonBackgroundNear(img, expected, 4),
          "datum: a mark appears near the projected datum");
  }

  // (b) Route points: a mark lands at every route point's projected pixel.
  {
    OverlayViewport vp{size, project};
    OverlayState state;
    state.route = {{0.1, 0.1}, {0.5, 0.5}, {0.9, 0.2}};
    wxImage img = RenderToImage(vp, state);
    for (const auto& ll : state.route) {
      wxPoint expected = project(ll.first, ll.second);
      Check(AnyNonBackgroundNear(img, expected, 2),
            "route: a mark appears at each route point");
    }
  }

  // (c) Empty state (no datum, no route): nothing is drawn at all.
  {
    OverlayViewport vp{size, project};
    OverlayState state;
    wxImage img = RenderToImage(vp, state);
    Check(AllBackground(img), "empty state: nothing is drawn");
  }

  // (d) Off-screen datum: renders without crashing.
  {
    OverlayViewport vp{size, project};
    OverlayState state;
    state.has_datum = true;
    state.datum_lat = 50.0;   // Far outside the [0,1] projection above.
    state.datum_lon = -50.0;
    wxImage img = RenderToImage(vp, state);
    Check(img.IsOk(), "off-screen datum: renders without crashing");
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("All checks passed.\n");
  return 0;
}

/**
 * wxBitmap/wxMemoryDC need the toolkit's GUI properly initialized (GTK
 * style contexts, stock objects like wxWHITE_BRUSH) -- wxInitialize()'s
 * console-only init isn't enough and crashes on first use. Running a real
 * (if event-loop-less) wxApp through wxEntry() does the same GUI init a
 * plugin's OpenCPN host process gets for free.
 */
class OverlayTestApp : public wxApp {
public:
  bool OnInit() override {
    if (!wxApp::OnInit()) return false;
    g_exit_code = RunTests();
    return false;  // No event loop to run -- the tests already ran.
  }
};

}  // namespace

wxIMPLEMENT_APP_NO_MAIN(OverlayTestApp);

int main(int argc, char** argv) {
  wxEntry(argc, argv);
  if (g_exit_code < 0) {
    std::fprintf(stderr,
                  "wxWidgets GUI failed to initialize -- no display? run "
                  "under xvfb-run.\n");
    return 1;
  }
  return g_exit_code;
}
