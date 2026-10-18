#!/usr/bin/env bash
#
# Generates the screenshots for docs/USER_GUIDE.md by driving a real OpenCPN
# on a virtual X display: Xvfb for the screen, a fake GPS fix over UDP,
# xdotool to fill in a demo case, ImageMagick to capture and crop.
#
# Linux only -- run it on the sandbox. Needs opencpn (5.8+), xvfb, xdotool,
# imagemagick and python3, and a built tarball in the build tree: it installs
# the plugin from there with scripts/install-local.sh (into ~/.local).
#
# Usage: scripts/guide-screenshots/make-screenshots.sh [build-tree] [out-dir]
#   build-tree defaults to "build", out-dir to build/guide-screenshots.
#   Copy the pictures you want from out-dir to docs/images/guide/.
#
# The pictures show Linux/GTK widgets. Click positions and crops are relative
# to the panel and follow its current layout; after a layout change, look at
# the full-screen shots in out-dir/steps/ and adjust the numbers below.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
tree=${1:-build}
out=${2:-$repo/build/guide-screenshots}
grib=$repo/test/data/gfs_10m_wind_simple_drt5.0.grib2

export DISPLAY=:${GUIDE_DISPLAY:-77}
screen_w=1700
screen_h=1000
panel_x=1105  # Right of the 1100 px chart window: a panel overlapping it
              # would be drawn over, as there is no window manager.

# Demo case, south of Sicily -- inside the GRIB test fixture (30-40N, 10-20E).
own_lat=35.3333 own_lon=13.3333 own_sog=6.0 own_cog=45
reported="35 40.0 N, 013 50.0 E"
drift_set=70 drift_rate=1.5
slow_speed=1.0  # For the no-intercept shot: slower than the drift.
view_lat=35.57 view_lon=13.82 view_scale=0.0075
hours_ago=6

# Panel layout: y of each row, and of the boxes cropped, relative to the
# panel's top -- read off a full-screen shot in steps/ after a layout change.
# x positions are in the clicks below; right-aligned ones use panel_w.
y_speed=86 y_reported=157 y_time=197 y_set=291 y_rate=331
y_grib=400 y_craft=440 y_leeway=474 y_pob=507
y_ownship_box=0 h_ownship_box=112 y_report_box=116 h_report_box=114
y_drift_box=230 h_drift_box=305 y_result_box=539 h_result_box=335
y_course=687 h_course=135 y_recalc=800 y_auto=840 h_auto=40
y_display=877 h_display=40

for tool in Xvfb xdotool import convert opencpn python3; do
  command -v "$tool" >/dev/null || { echo "missing: $tool" >&2; exit 1; }
done
[[ -f $grib ]] || { echo "missing GRIB fixture: $grib" >&2; exit 1; }

work=$(mktemp -d /tmp/intercept-guide.XXXXXX)
mkdir -p "$out/steps"
pids=()
cleanup() {
  for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
  sleep 2
  # OpenCPN ignores a plain kill while it has a dialog up.
  for p in "${pids[@]}"; do kill -9 "$p" 2>/dev/null || true; done
  rm -rf "$work"
}
trap cleanup EXIT

log() { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }

step=0
snap() {  # Full-screen shot into steps/, for checking and adjusting.
  step=$((step + 1))
  import -window root "$out/steps/$(printf '%02d' "$step")-$1.png"
}

wait_window() {  # wait_window NAME-REGEX [SECONDS] -> prints the window id
  local i id
  for ((i = 0; i < ${2:-60}; i++)); do
    id=$(xdotool search --onlyvisible --name "$1" 2>/dev/null | head -1) || true
    if [[ -n $id ]]; then echo "$id"; return 0; fi
    sleep 1
  done
  echo "timed out waiting for window: $1" >&2
  snap timeout
  return 1
}

# --- Plugin, config, screen, GPS, OpenCPN -------------------------------------

log "installing the plugin from $tree"
"$repo/scripts/install-local.sh" "$tree" >/dev/null

mkdir -p "$work/config"
sed -e "s/@OWN_LAT@/$own_lat/; s/@OWN_LON@/$own_lon/" \
    -e "s/@VIEW_LAT@/$view_lat/; s/@VIEW_LON@/$view_lon/" \
    -e "s/@VIEW_SCALE@/$view_scale/" \
    "$here/opencpn.conf" >"$work/config/opencpn.conf"

log "starting Xvfb on $DISPLAY"
Xvfb "$DISPLAY" -screen 0 "${screen_w}x${screen_h}x24" -nolisten tcp \
  >"$work/xvfb.log" 2>&1 &
pids+=($!)
sleep 2

python3 "$here/fake_gps.py" "$own_lat" "$own_lon" "$own_sog" "$own_cog" \
  >"$work/gps.log" 2>&1 &
pids+=($!)

log "starting OpenCPN"
LANG=C.UTF-8 LC_ALL=C.UTF-8 opencpn -c "$work/config" -l info \
  >"$work/opencpn.out" 2>&1 &
pids+=($!)
wait_window '^OpenCPN [0-9]' >/dev/null
sleep 5
warn=$(xdotool search --onlyvisible --name 'Welcome to OpenCPN' 2>/dev/null |
       head -1) || true
if [[ -n $warn ]]; then
  # Cancel would quit OpenCPN; Return accepts.
  log "accepting the navigation warning -- ConfigVersionString in" \
      "opencpn.conf no longer matches this OpenCPN"
  xdotool windowfocus --sync "$warn"
  xdotool key --window "$warn" Return
fi
for ((i = 0; i < 60; i++)); do
  grep -q 'opencpn-intercept_pi' "$work/config/opencpn.log" 2>/dev/null && break
  sleep 1
done
sleep 8  # GPS fix, chart redraw.
snap started

# --- Panel helpers -----------------------------------------------------------
# The panel sits at (panel_x, 0); all x/y below are relative to it.

panel=
panel_w=0
panel_h=0
place_panel() {
  xdotool windowmove --sync "$panel" "$panel_x" 0
  sleep 1
  eval "$(xdotool getwindowgeometry --shell "$panel")"
  panel_w=$WIDTH
  panel_h=$HEIGHT
}
click() {  # click X Y (relative to the panel)
  xdotool mousemove $((panel_x + $1)) "$2" click 1
  sleep 0.7
}
focus_panel() {  # With no window manager a click doesn't move the keyboard
  xdotool windowfocus --sync "$panel"  # focus -- keys would go to OpenCPN.
  sleep 0.3
}
type_field() {  # type_field X Y TEXT -- replace a field's contents
  click "$1" "$2"
  focus_panel
  xdotool key ctrl+a
  xdotool type --delay 30 "$3"
  xdotool key Tab
  sleep 0.7
}
recalculate() {  # Recalculate sits right-aligned in the ETA row.
  click $((panel_w - 72)) "$y_recalc"
  sleep 2
  local err
  err=$(xdotool search --onlyvisible --name 'Invalid position' 2>/dev/null |
        head -1) || true
  if [[ -n $err ]]; then
    snap invalid-position
    echo "the panel rejected an input -- see the last shot in $out/steps" >&2
    exit 1
  fi
  place_panel  # A manual run re-fits the panel to its contents.
}
crop() {  # crop X Y W H FILE -- a part of the panel
  import -window root -crop "${3}x${4}+$((panel_x + $1))+$2" +repage "$out/$5"
  log "wrote $5"
}
crop_screen() {  # crop_screen X Y W H FILE -- any part of the screen
  import -window root -crop "${3}x${4}+$1+$2" +repage "$out/$5"
  log "wrote $5"
}

# --- Open the panel ------------------------------------------------------------
# With only this plugin enabled, its button is the 9th in the toolbar (after
# Help); buttons are 40 px apart from y = 52.

log "opening the panel"
xdotool mousemove 27 372 click 1
panel=$(wait_window '^Intercept$' 20)
place_panel
snap panel-open
# The toolbar with a bit of chart, and an arrow at the Intercept button
# (y 372 on screen = 344 in the crop).
import -window root "$work/screen.png"
convert "$work/screen.png" -crop 260x410+0+28 +repage \
  -stroke '#e0301e' -strokewidth 5 -fill '#e0301e' \
  -draw 'line 200,290 70,337' -draw 'polygon 52,344 82,322 92,343' \
  "$out/01-toolbar-button.png"
log "wrote 01-toolbar-button.png"

# --- Case 1: explicit drift -------------------------------------------------

log "entering the demo case"
type_field 304 "$y_reported" "$reported"
# The time picker takes typed digits into the field under the click: the
# hour. Going back further than midnight would need the date too, so keep it
# within today.
hour=$(date +%H)
report_hour=$((10#$hour >= hours_ago ? 10#$hour - hours_ago : 0))
click 297 "$y_time"
focus_panel
xdotool type --delay 80 "$(printf '%02d' "$report_hour")"
xdotool key Tab
sleep 0.7
type_field 140 "$y_set" "$drift_set"
type_field 140 "$y_rate" "$drift_rate"
recalculate
snap explicit-drift

crop 0 0 "$panel_w" "$panel_h" 02-panel-overview.png
crop 0 "$y_ownship_box" "$panel_w" "$h_ownship_box" 03-own-ship.png
crop 0 "$y_report_box" "$panel_w" "$h_report_box" 04-report.png
crop 0 "$y_drift_box" "$panel_w" "$h_drift_box" 05-target-drift.png
crop 0 "$y_result_box" "$panel_w" "$h_result_box" 08-result.png
crop 0 "$y_display" "$panel_w" "$h_display" 11-display-checkboxes.png
crop_screen 0 26 1100 950 10-chart-overview.png

# --- Case 2: GRIB file + craft type, auto-recalculate (worked example) ------

log "selecting the GRIB file"
click 364 "$y_grib"
dialog=$(wait_window 'Select GRIB file' 20)
xdotool windowfocus --sync "$dialog"
xdotool key --window "$dialog" ctrl+l
xdotool type --window "$dialog" --delay 20 "$grib"
xdotool key --window "$dialog" Return
sleep 2
# Craft type: without a window manager a click neither opens the dropdown's
# list nor focuses it. Focus it from the keyboard instead -- click into
# "Persons on board", Shift+Tab back past the leeway checkbox -- and step it
# with Down: "Unknown" -> "Rubber boat (inflatable / RIB)".
click 230 "$y_pob"
focus_panel
xdotool key shift+Tab shift+Tab Down
sleep 0.7
recalculate
snap grib
crop 0 "$y_drift_box" "$panel_w" "$h_drift_box" 06-target-drift-grib.png

type_field 220 "$y_auto" 5
click 29 "$y_auto"  # Auto-recalculate: runs once straight away.
sleep 3
snap auto
crop 0 $((y_auto - 18)) "$panel_w" "$h_auto" 07-auto-recalculate.png
crop 0 0 "$panel_w" "$panel_h" 12-example-panel.png
crop_screen 0 26 1100 950 13-example-chart.png

# --- Case 3: no intercept -----------------------------------------------------

log "own ship slower than the drift"
click 29 "$y_auto"                   # Auto-recalculate off again.
click $((panel_w - 60)) "$y_grib"    # Clear the GRIB file: back to the
                                     # explicit drift.
click $((panel_w - 215)) "$y_speed"  # Untick "use GPS speed".
type_field 115 "$y_speed" "$slow_speed"
recalculate
snap no-intercept
crop 0 "$y_course" "$panel_w" "$h_course" 09-no-intercept.png

log "done: $out"
