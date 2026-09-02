# Plan: television latency work

Working notes for whoever picks this up next. Everything here was measured on
Sacha's rooted LG webOS 24 television (armv7 userspace, 2 cores online of 4,
1.4 GHz) against his real Jellyfin server, not extrapolated from a desktop.

## Why this exists

Switching pages feels like ~200 ms on the television and a few frames on the
desktop, while the render benchmark reported 6-7 ms warm. The instrument was
not wrong, it was measuring a narrower window than the experience, and nobody
had run it on the device. Now it runs on the device.

## What the device actually says

Route walk `home,libraryGrid` with the Movies library open, 4 iterations,
`dist/tv-benchmark/`:

| | wall | construct | render thread | gui cpu | delegates | swaps |
| --- | --- | --- | --- | --- | --- | --- |
| home warm | **19.8 ms** | 0 | 3.2 ms | 23.5 | 0 | 2 |
| libraryGrid warm | 16.3 ms | 0 | 1.9 ms | 15.1 | 0 | 2 |
| home cold | **378.4 ms** | 164 ms | 120.1 ms | 156.1 | +28 / -14 | 16 |
| libraryGrid cold | 219.6 ms | 163 ms | 90.6 ms | 107.6 | +14 / -27 | 10 |

Cold home breaks down as `164 construct + 166 waiting for its first row +
46 present`. Warm costs 19.8 ms. **The entire difference is whether the page
was still resident.**

Library scroll (`--mode library`, 12 screens, grid): settle median 171 ms,
first screen **3392 ms** with a **1850 ms** worst frame gap. Screens that
introduce no new artwork settle in ~4 ms, so the cost tracks image work
directly.

Note `instanceMs` is wall-clock to the instance mark, not CPU. On the desktop
it is mostly asynchronous incubation (5.2 ms of GUI CPU against 19 ms of
"construct"). On the television it is real work -- 156 ms of GUI CPU against
164 ms of construct. Do not repeat the mistake of reading the desktop's
`instanceMs` as CPU; read `guiCpuMs`.

## Work, in the order the numbers justify

### 1. The prewarmer thrashes its own cache (cheapest, biggest)

`qml/shell/RouteStack.qml`:

    prewarmQueue = ["settings", "subtitleSettings", "libraryGrid", "itemDetails", "personDetails"]  // 5
    readonly property int residentPageBudget: ... gigabytes < 2.5 ? 3 ...                            // 3 on a 2 GB TV

Five pages prewarmed into a three-page budget, and `evictBeyondBudget()` is
pure LRU with nothing protecting `home`. So the prewarmer evicts as fast as it
builds, and home -- least recently *shown* while you are on a details page --
goes first. Returning home then costs the full 378 ms instead of 19.8 ms.

Fix: cap the prewarm queue to what the budget holds, and exempt the home route
from eviction. Verify with `--script home,libraryGrid` warm and cold before and
after. This is a logic bug, not a performance trade.

### 2. Render thread, 120 ms of the cold switch

Separate from the 156 ms of GUI CPU, spread over 16 swaps while 28 delegates
are created: scene-graph build plus texture upload.

The `layer.enabled` / `MultiEffect` removal from earlier in this work targets
exactly this. It was written, measured as within noise on a desktop GPU, and
reverted -- on the wrong instrument. Re-measure it on the television. If it is
taken up again it needs ratio-based corner baking (radius as a fraction of the
image width), because client-side `sourceSize` is off the table: see
[[artwork-codec-decode-cost]] in memory, and note libwebp has no fractional
decode so `setScaledSize` costs a full decode plus a rescale.

### 3. qmltc, 164 ms of construct

`perf/qmltc-homepage` (rebased on master). Read commit `c288654` first: it
records what works, what does not, and why.

The earlier conclusion that qmltc was not worth it was based on extrapolating
~12 ms of television construct cost from the desktop. **The device says ~164 ms
of real CPU.** A measured 12.1x on that is worth roughly 150 ms, so the
trade-off is genuinely different from what that commit message concludes --
its measurements stand, its recommendation does not.

Still unsolved on that branch, and all real work:

- Unqualified calls in QML resolve at runtime, not build time. `Component.
  onCompleted: rebuildSections()` compiles and dies with "is not a function";
  qualifying it (`root.rebuildSections()`) fixes it, but finding them across
  21k lines has no compiler help.
- `QML_init` evaluates the document's bindings in `QML_endInit` and only then
  runs the `PropertyInitializer`, so a property the document binds to is read
  as undefined once regardless. Re-assigning after construction recovers it.
- Even past both, HomePage never populates: `delegates_created=0`,
  `contentReady` never fires, the route waits out its 5 s watchdog. Not root
  caused.
- `SettingRow` exposes `property alias trailing: trailingRow.data`, an alias
  into the deferred `contentItem`, so the id cannot simply be deleted --
  removing it changes a public API five call sites use.

### 4. Present, 46 ms across 16 swaps

Home cold swaps 16 times against 2 warm. Check whether the route transition
animation is running for roughly ten frames of that.

## Running the harness

    tools/webos/bench-device.sh --script home,libraryGrid --library Movies --iterations 4
    tools/webos/bench-device.sh --script home,libraryGrid --library Movies --cold
    tools/webos/bench-device.sh --mode library --library Movies          # scroll walk
    tools/webos/bench-device.sh --mode library --library Movies --list

`--host` defaults to `root@192.168.0.200` (`SPOOL_TV_HOST`). Reports land in
`dist/tv-benchmark/`. **list-warm has not been captured yet** -- it failed to
produce a report and was not rerun; every other combination is in
`dist/tv-benchmark/`.

Deploying a build to the television: `bash build-ipk.sh`, then extract
`build/com.sachk.spool_*.ipk` (`ar x`, then `data.tar.gz`) and untar the
`com.sachk.spool` subtree over the app directory. `opkg install` does not work
-- it checks free space on `/`, which is 100% full, and extracts to
`/usr/palm/applications` rather than the developer app path. Afterwards run
`chown -R 5486:5000 .config .cache .local` in the app directory.

## Traps, all of which cost time already

- **Never run the binary over ssh as root.** It leaves root-owned files through
  the app's `.config` and `.cache`; the app runs as uid 5486 and then cannot
  write its settings or database, reports local data as unavailable, and asks
  for a login again. The script launches through the application manager for
  this reason.
- **Dropping to uid 5486 directly does not work either** -- the Wayland socket
  is `root:compositor 0660`, so it needs supplementary group 505.
- **Editing appinfo.json's `main` is ignored**: the application manager
  launches from its own cached appinfo. The script stands in for
  `bin/jellyfin-native` instead and restores it on exit.
- **luna-send** takes a full `luna://` URI first, payload second, over a tty --
  copy `tools/webos/verify-device.sh`. The `-f` form is a no-op.
- **The television's root filesystem is 100% full** (`/dev/root`, 1.3 G). This
  may be worth telling Sacha about independently; a full root filesystem is its
  own source of system-wide latency.
- The app keeps its profile *inside* the app directory, so a manual launch
  needs `HOME` set there or it runs signed-out and renders empty pages that
  settle far faster than real ones.

## Loose ends not in the critical path

- Commit `8d35d77` changed `setOrganizationName` from `sachk` to
  `spool-jellyfin`, which moves both the settings file and the database. Every
  user upgrading past it silently loses their saved server and login. Both
  trees exist side by side on the television. Needs either a one-time migration
  or a release note; Sacha has not decided.
- The cold run reports a **74 ms noise floor** (idle-timer drift) on the
  television. That is four frame budgets of scheduler jitter and nobody has
  looked at why.
- `local-docs/pgo.md` has the PGO plan. PGO would speed up the same interpreted
  construct path as qmltc with no QML changes and no private API dependency,
  and Qt is not wired for it yet.
