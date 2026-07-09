# NEXT_PLAN — consolidation & performance, round 2 (2026-07-09)

Successor to `REFACTOR_PLAN.md` and `PERFORMANCE_PLAN.md`. Written after a
full audit of what the previous refactor actually did to the tree, plus
root-cause analysis of the current bug reports. Read the post-mortem first —
the process rules in §1 exist because of it and are not optional.

Current tree: `src/` 18,662 + `qml/` 12,336 = **30,998 LoC**.
Baseline before the refactor (b6ec9cf): 19,037 + 11,312 = 30,349.
The refactor plan promised ≈26k; we ended at +649.

Target after this plan: **≈ 26.0–26.5k** (qml ≈ 9.5–10k, src ≈ 16.5–17k),
with the input-bug class structurally eliminated, not patched.

---

## 0. Post-mortem: why the refactor didn't shrink anything

Per-commit accounting (`git diff --numstat b6ec9cf..HEAD`):

| Commit | What it claimed | Reality |
|---|---|---|
| 2374650 meta-json DTO mapping | Phase 1, promised −900 | **+286 net.** `MetaJson.h` is 197 lines (promised ~80); `JellyfinTypes.cpp` only lost 39 lines; the facade's `mediaItemFromJson` (~150 lines) survived untouched |
| 5863920 "cut duplicate UI and controller plumbing" | Phases 2/3/4/6 | 162 files, +11,708/−11,756 = **−48 net.** A near-total rewrite/reformat that moved code instead of deleting it |
| 366d04f "complete native app cleanup" | Phase 11 cleanup | **+183 net** |
| d4aaa5f "delete unused navigation and UI plumbing" | Phase 5 | −430 (the only genuinely net-negative phase) |

Where the growth went: `LibraryManagementController` +415 (extracted from
AppController, which only shed 682−415 ≈ −267 net), `MediaInfoOverlay` +202,
`ManagementDialog` +172, `SettingsPage` +146, `MenuRow` +113,
`MenuListView` +108, PlayerOverlay* +300, `OverlayDialog` +38,
`PopupMenuPanel` +21.

### The five systemic causes

1. **Extraction ≠ deletion.** Every new QML file costs 15–25 lines of
   boilerplate (imports, root item, property plumbing, signal forwarding).
   Six new primitives were added, but the code they were meant to replace was
   *reshaped around them* rather than deleted. The acceptance greps in
   REFACTOR_PLAN were never run: `function handleKey` count is **18**
   (target ≤4).
2. **The forwarding tax.** Signals/properties are relayed through 3–4 layers:
   delegate → row → page → shell (`favoriteToggled`, `playedToggled`,
   `mediaInfoRequested` × three row components × every page). Every layer
   re-declares and re-emits. ~12 `handleAcceptPressed/Released` forwarder
   functions exist whose bodies are one line.
3. **Old paths kept alongside new ones.** Controllers are registered as QML
   singletons (`Browse`, `Home`, `Content`, …) *and* AppController kept its
   full wrapper surface (`App.openLibrary`, `App.setLibrarySort`,
   `App.playFromModel` → 7-deep play call chain). Nothing was retired.
4. **The DTO refactor didn't fit the data.** `MovieItem` is not a DTO — it's
   a view-model with nine pre-built image-URL strings, a pre-formatted
   `subtitle`, and `playable` baked in at parse time
   (`JellyfinApiFacade.cpp:321-418`). MetaJson can't parse derived state, so
   the hand-written parser stayed, and MetaJson became *additional* code.
5. **Format churn hid the accounting.** The ±11.7k-line commit mixed
   reformatting with logic changes, so nobody could see that net deletion was
   ~zero.

## 1. Process rules for this round (hard gates)

- **Net-negative gate:** every phase lands only if
  `git diff --shortstat <phase-start>..HEAD` shows net deletions ≥ the
  phase's floor (listed per phase below). If a phase comes in over budget,
  the deletion half wasn't finished — do not move on.
- **Delete in the same commit** as the replacement. Never "wire the new
  thing, remove the old one later".
- **No new files** unless the same commit deletes more lines than the new
  file adds. New abstractions must have ≥3 call sites on day one.
- **No reformat sweeps** mixed with logic. `qmlformat`/style-only changes go
  in dedicated commits marked `style:` so the accounting stays honest.
- **Run the acceptance greps** at the end of each phase and paste the counts
  into the commit message.
- Verification per phase: `cmake --build build/linux-release/app --target
  jellyfin-native` + `bash tools/smoke-native-app.sh` (offscreen boot), plus
  the phase's manual flow on desktop. Device (IPK) checks at milestones M1/M2
  only (see §10).

## 2. State of the bugs (2026-07-09 fixes already in tree)

Fixed tonight (this session, uncommitted):

- **Missing logs**: the app moved its logs to `/tmp/<appid>/` and
  `removeLegacyAppLogs()` *deletes* the old flat `/tmp/*.log` files at boot —
  but `tools/webos/diagnose.sh`, `collect-diagnostics-bundle.sh`, and
  `verify-device.sh` still read the old flat paths. All three now read the
  new dir (with legacy + `.cache/logs` fallbacks). `rotateLogFile()` also
  now `mkdir -p`s the parent, so mpv's log can never be lost to a missing
  `/tmp` subdir regardless of which code path runs first.
- **Focus highlight invisible**: the purple underline was deleted
  (786dc7c) on the theory that `ImageCard`'s accent border took over — but a
  QML `Rectangle` paints its border *under* its children, and the poster
  `Image` fills the frame, so the border was always overpainted once art
  loaded. The scale cue (1.025) is disabled under `Theme.reducedMotion` and
  clipped by the cards' `clip: true`. Fix: explicit focus-ring `Rectangle`
  drawn *above* the artwork inside `ImageCard`.
- **Right-key double-move**: `MediaPosterScrollerRow`/`PersonScrollerRow`
  handled direction keys in their own `Keys.onReleased` while
  `AppShell.Keys.onPressed` dispatched the same key on press
  (`dispatchNavigationKey` → page → row). One tap = two moves, deterministic,
  in every page hosting those rows. Fix: internal release handlers now only
  process accept keys.
- **Held-OK phantom clicks**: zero `isAutoRepeat` checks existed in the whole
  codebase. Held OK auto-repeats: each repeat press restarted the 520 ms
  long-press timer (`MediaItemActions`), repeat press/release pairs completed
  full "clicks" (play!), and once the context menu opened, the *release of
  the very key that opened it* activated the focused menu entry. Fixes:
  repeat-accept swallowed at AppShell (press+release), `pendingAccept` guard
  in MediaItemActions, arm-on-fresh-press + 300 ms settle window in
  ItemContextMenu and MenuListView.
- **Wonky dropdowns**: same double-dispatch (page moved the list on press,
  `MenuListView.Keys.onReleased` moved again on release), plus an OK press
  with a dropdown open bubbled to `routeStack.handlePressedKey` and armed the
  *underlying grid card's* long-press timer. MenuListView now owns direction
  on press and accept on armed-release, and swallows the stray press.

These are tactical patches. §4 removes the architecture that produced them.

Still open: **player not starting** — do not guess; the log tooling now
works. Runbook in §9.

## 3. LoC budget for this round

| Phase | Area | Floor (net) | Stretch |
|---|---|---|---|
| A | Input: one router, one protocol | −500 qml | −800 |
| B | Rows & cards: 3 rows → 1, 2 cards → 1 | −350 qml | −450 |
| C | Player overlay consolidation | −450 qml | −650 |
| D | Details/Settings/Login page diets | −450 qml | −700 |
| E | AppController browse/play collapse | −350 src | −450 |
| F | MovieItem diet (view-model → DTO) | −250 src | −350 |
| G | C++ hygiene (DB awaiter, Diagnostics, CpuTopology) | −200 src | −350 |
| H | Shell/overlay unification | −150 qml | −250 |
| **Total** | | **−2,700** | **−4,000** |

Even the floor beats the previous round's *entire* net result by 50×.

---

## 4. Phase A — one input router, one protocol (the big one)

The current system has **four** overlapping dispatch mechanisms:

1. `AppShell.Keys.onPressed/onReleased` (root, fires *last* in bubble order —
   the code reads as if it fires first, which is the core misunderstanding);
2. per-page `handleKey(key)` / `handlePressedKey(key)` chains
   (18 implementations), reached from (1);
3. per-component `Keys.onReleased` on inner ListViews (fire *first*, race
   (1)+(2) — the double-move bug);
4. overlay-specific `handlePressed/handleReleased(event)` pairs
   (ItemContextMenu, MediaInfoOverlay, ManagementDialog, VideoSurface), fed
   by (1) — except when a `QQC.Popup` grabs focus, in which case (3) runs
   and (4) is dead code. Whether a given key is handled on press or release
   depends on which page you're on. `SelectRow.handledNavigationPress` is a
   literal flag-hack around the resulting double-fire.

### Design

- **`qml/shell/KeyRouter.qml`** (singleton-ish object owned by AppShell).
  All `Keys.*` handlers in the app live *only* here and in text inputs.
  Acceptance grep: `grep -rn "Keys.on" qml | grep -v KeyRouter | grep -v
  TextField` → 0.
- The router normalizes: `{key, phase: press|release, repeat}` and applies
  global policy in one place: accept-repeat swallowed, direction-repeat
  allowed, back handled on press with release-claim, webOS scancode noise
  (`InputKeys.isBackEvent`, `isIgnoredPlayerNoise`) normalized here and
  **nowhere else**.
- **One target interface.** The router resolves a single *active target*
  (top-most of: management dialog → context menu → media info → player →
  navbar → page) from explicit state, not focus archaeology. Each target
  implements exactly one function:
  `handleKey(key, phase, repeat) -> bool`.
  Delete every `handlePressedKey`, `handleReleased(event)`,
  `handleAcceptPressed/Released` forwarder (12 of them are one-liners).
- **Accept protocol** owned by the router: press arms the active target's
  optional `longPress()` timer (router-owned, single QTimer); release before
  timeout = `activate()`; timer fire = `longPress()` + swallow release.
  `MediaItemActions` shrinks to the pointer/hover half (~80 lines);
  cards stop forwarding accept at all.
- **Focus is presentation, not routing.** `activeFocus` decides what the
  ring highlights; the router decides who gets keys. This ends the
  `if (x.activeFocus) … else if (y.activeFocus)` 120-line chains in
  `ItemDetailsPage.handleKey` (:708-836) and `SettingsPage`/`LoginPage` —
  replace with small per-page focus-zone tables:
  `zones: [header, actions, seasons, cast, similar]` + `move(dir)` walking
  the table. Expect ItemDetailsPage's key code to drop from ~250 to ~60.
- Delete `SelectRow`'s release fallback + flag (SelectRow loses ~20 lines,
  gains nothing to replace it — the router already moves on press).
- Rows/lists keep only `move(dir)`/`activate()`; NavList/MenuListView merge
  (NavList's 42 lines fold into MenuListView; MenuListView's tactical
  arm/guard code from tonight moves into the router and is deleted).

### Why this kills the bug class

Every reported input bug (double-move, phantom clicks, dropdown wonk,
menu-open-then-play) is a *race between two dispatch paths*. With one
dispatcher there is nothing left to race. "Some builds/some places" symptoms
disappear because behavior no longer depends on which item happens to hold
`activeFocus` or whether a popup stole the event chain.

Order: do this **first**. Every other QML phase touches the same files and
gets cheaper once the forwarding chains are gone.

## 5. Phase B — one row, one card

- `HomeHorizontalRow` (227) + `MediaPosterScrollerRow` (194) +
  `PersonScrollerRow` (162) are the same component: horizontal ListView +
  header + current-index + ensure-visible + activate. Replace with one
  `MediaRow.qml` (~180): `kind: poster|landscape|library|person`, model
  injected, no per-kind delegate components (the card handles kind).
- Fold `LandscapeCard` (74) into `MediaItemCard` — it's the landscape
  configuration of the same layout. Person cards are `MediaItemCard` with
  `actionable: false` (already supported).
- Delete the 18 `readonly property` re-declarations in `MediaItemCard`
  (:19-36): read `item.x` directly in bindings. Less code *and* fewer
  binding objects per delegate (hundreds of delegates × ~14 saved bindings —
  this is also a scroll-perf win on the TV).
- Qt 6.11: replace all 17 hand-rolled `ensureVisible`/`ensureItemVisible`
  implementations with the new Flickable position-to-child APIs
  ([What's New in Qt 6.11](https://doc.qt.io/qt-6/whatsnew611.html)) behind
  one 10-line helper.
- Add `pragma ComponentBehavior: Bound` to every delegate-bearing file while
  in there (qmllint will demand it anyway).

## 6. Phase C/D — page diets

**Player overlay** (2,309 lines across 9 `Player*.qml` files):
- The four menus (subtitles/audio/queue/debug) are option lists — use the
  same OverlayDialog+MenuListView as the rest of the app; delete
  `PlayerOverlayMenu` (213) and the menu-mode half of `PlayerOverlayPage`'s
  state (15 ad-hoc properties: `mode/row/menuIndex/audioSyncRow/seekHold*…`).
- Replace the three hold/repeat timer groups (`seekHoldTimer` +
  `previewBurstTimer` + `downHold` in PlayerOverlayInput) with the router's
  single long-press/repeat facility from Phase A.
- Target: overlay ≤ 1,500 lines total without feature loss.

**ItemDetailsPage** (1,404 — the largest QML file, *grew* during the
refactor): focus-zone table from Phase A removes ~200; the metadata panel
and header link rows become dumb components fed by `item` directly; kill the
remaining `detailsModel/detailsIndex/detailsSource` plumbing in AppShell
(:16-19,147-175) by resolving models in `RoutePolicy` (finish REFACTOR P5.2).

**SettingsPage** (797) + `SettingsSchema.cpp` (258): the schema already
describes rows in C++; the page should be a ListView over the schema with
one delegate per row-type (Select/Slider/Toggle/TextField already exist).
Audit for rows declared in *both* places and keep only the schema.

**LoginPage** (686 for a TV login form): extract nothing, delete: the
per-field focus dance and duplicated server-list key handling collapse under
the router; expected ≤ 450.

## 7. Phase E/F/G — C++

**E. AppController browse/play collapse** (`AppController.cpp` 1,136):
- Ten functions share one body shape (generation token → `m_browse->enterX`
  → `resetPaging` → `clear` → `setLoadingMore` → `runLatest(fetchBrowsePage)`
  → `showCurrentItemsPage`): `openLibrary`, `refreshCurrentLibrary`,
  `loadCurrentBrowsePage`, `openSeries`, `openSeason`, `openPlaylist`,
  `openBoxSet`, `openFolder`, `openGenre`, `openStudio` (:330-1022). One
  private `beginBrowse(enterFn, cacheKeyFn)` + thin publics ≈ −250.
- Play chain is 7 deep: `playOrOpen → playOrOpenFromModel → playFromModel →
  playQueuedItem(s) → playQueueCurrent → playMediaItem → startPlayback`.
  `playFromModel` is a literal alias (:410-413). Collapse to
  `playFromModel(model, index, fromStart)` + `playItem(item, fromStart)` +
  private `startPlayback`. QML call sites: 3.
- Move the library-query mutators (`setLibrarySort`,
  `setLibraryQueryListValue/BoolValue/NullableBoolValue`,
  `clearLibraryFilters`, :524-594) onto `BrowseSessionController` — the
  query lives there and `Browse` is already a QML singleton.
- AppController target: ≤ 700 lines, lifecycle + session glue only.

**F. MovieItem diet** — the fix MetaJson actually needed:
- Strip the nine URL fields (`posterUrl, seriesPosterUrl, thumbUrl,
  backdropUrl, logoUrl, bannerUrl, landscapeCardUrl…`), `subtitle`, and
  `playable` from `MovieItem`. Keep raw tags (`posterTag`, `thumbTag`,
  `backdropTag`, `logoTag`, `bannerTag`, `seriesPrimaryImageTag`).
- Image URLs are composed on demand by `ArtworkService` behind the existing
  `image://artwork` provider: QML asks
  `Art.url(item, "poster"|"landscape"|"backdrop"|…, width)` (one C++
  invokable). `buildImageUrl` moves out of the facade wholesale.
- `subtitle`/`playable` become pure functions used by `MovieGridModel`'s
  existing `DisplaySubtitleRole`/`PlayActionLabelRole` — the display-role
  layer already exists, the duplication in the DTO does not need to.
- `mediaItemFromJson` then reduces to `metaFromJson<MovieItem>(PascalCase)` +
  ~20 fixup lines; delete `stringsFromJsonArray`/`studioNamesFromJsonArray`
  duplicates via a `QStringList` path in MetaJson.
- Side wins: ~9 heap QStrings × every cached/browsed item (thousands) ≈
  multi-MB RAM + measurable parse time on armv7; home-payload cache blobs
  shrink ~40%. **Bump `kHomePayloadSchemaVersion`.**

**G. Hygiene:**
- `DatabaseManager`: replace the 60-line `WorkerAwaiter` with
  `QPromise/QFuture` fulfilled on the worker (QCoro awaits QFuture natively);
  keeps semantics, deletes the coroutine-handle plumbing (REFACTOR P9.2).
- `Diagnostics` (489): audit `NetworkRequest`/`ThreadScope`/
  `EventLoopWatchdog` usage; anything not consumed by the current-instance
  report or actively used for on-device debugging goes.
- `CpuTopology` (157): `std::thread::hardware_concurrency()` + one sysfs
  read ≈ 40 lines.
- Dedup the secret-redaction regex (`JellyfinTypes.cpp` vs `Diagnostics.cpp`,
  REFACTOR P11 leftover) and `exceptionMessage` helpers.

## 8. Performance, round 2

Unfinished PERFORMANCE_PLAN items, re-prioritized (top = do first):

1. **Strip staged shared libs in `build-ipk.sh`** — libmpv still ships 19 MiB
   with full debug info. One flag, −17+ MiB install, faster load. (§1)
2. **`HEAPTRACK_UNWIND_FLAGS` opt-in** — release builds still carry `-g` +
   frame pointers app-wide. 1–3 % CPU on armv7 for free. (§1)
3. **Kill the remaining startup blocker**: `QCoro::waitFor(
   database.loadDeviceIdAsync())` at `main.cpp:751` serializes sqlite before
   window show (§5's leftover). Defer API identity instead.
4. **qtvirtualkeyboard**: can't delete (LSM IME unreliable) but stop paying
   for it at launch — move the import behind a `Loader` that only
   instantiates on Login/Search focus; verify the plugin init leaves the
   cold path.
5. **Qt static rebuild** with `-mthumb -mcpu=cortex-a53` + `FEATURE_ltcg` +
   gcc-ar LTO app link — the single biggest remaining binary/cold-page lever
   (§2/§3). Schedule a long build slot.
6. mpv `-Db_lto=true`, ffmpeg `--enable-lto` piggyback on the same rebuild.
7. **Delegate binding diet** (Phase B above) — scroll jank is binding count ×
   delegate churn; removing ~14 bindings/card and the forwarding layers is
   the cheapest scroll-perf win available.
8. `requiredMemory` 300→150 A/B; `posix_fadvise` readahead experiment;
   `LD_DEBUG=statistics` one-off; exec→main measurement (§0) — still worth
   one instrumented session to size the remaining pre-main second.
9. PGO cycle last (§8), after the Qt rebuild settles the binary.

New this round:
- **TCP keep-alive**: Qt 6.11 changed QNAM's default to closing idle
  connections after 2 min ([What's New in Qt 6.11](https://doc.qt.io/qt-6/whatsnew611.html)).
  Verify the facade's long-lived connections/websocket reconnect behavior on
  6.11 and set explicit keep-alive on `QNetworkRequestFactory` if browse
  latency regressed after the Qt bump.
- **Home payload shrink** falls out of Phase F (smaller JSON to parse at the
  2.07 s warm-cache mark).

## 9. Player-won't-start runbook (logs first, then fix)

Tooling is fixed; collect before touching code:

```sh
tools/webos/diagnose.sh                       # paths + processes
tools/webos/collect-diagnostics-bundle.sh     # full bundle incl. /tmp/<appid>/
```

Read in order: app log (`play requested` → `prepareForPlaybackSurface
completed in N ms` → `mpv initialized in N ms idlePrepared=…` → `file
loaded`), then the mpv log next to it.

Ranked hypotheses:

1. **Idle-prepared mpv predates the playback surface** (commit 904733d).
   `prepareIdleMpv()` runs `mpv_initialize` right after the post-first-frame
   libmpv preload — at that moment `STARFISH_WINDOW_ID/WIDTH/HEIGHT` are
   unset (they're exported only inside `prepareForPlaybackSurface()`,
   `NativeAppWindow.cpp:114-116`). If any starfish/AO/VO init latches env or
   registry state at initialize time, every playback that *adopts* the idle
   handle fails, while cold-created handles work — matching an
   intermittent/"since that build" failure. Cheap bisect: env-gate idle prep
   (`JELLYFIN_DISABLE_IDLE_MPV=1`, ~5 lines) and A/B on device. Real fix if
   confirmed: export the window id as soon as the shell surface exists (at
   startup), or refuse to adopt an idle handle prepared without a window id.
2. **dlopen/preload failure** — look for MpvRuntime load-failure lines; the
   RTLD_LAZY caveat means a missing TV-side symbol only explodes at first
   call. The shim logs resolution failures loudly; they'd be in the app log.
3. **`prepareForPlaybackSurface` timeout** — the 5 s
   `wl_webos_foreign` export loop returning false produces "Failed to
   prepare the native playback surface" with no mpv log at all.
4. mpv log-file dir missing (fixed this session — `rotateLogFile` now
   creates it; if the bundle still has no mpv log, the handle never got that
   far → hypothesis 1–3).

## 10. Qt 6.11 adoption checklist

- Flickable position-to-child APIs → replaces all hand-rolled ensure-visible
  code (Phase B).
- New QML `override`/`virtual` property keywords → turn on and fix the
  shadowing warnings across the card/row hierarchy (the `item`/`title`
  redeclarations are exactly the accident class this catches).
- qmllint as a CI/pre-commit gate (`jellyfin-native_qmllint` target exists);
  context-property warnings can be silenced per-file via
  `.contextProperties.ini` — but we have zero context properties left, so
  run it clean.
- qmlls workspace support for day-to-day editing.
- qmltc for leaf theme/primitive types only after Phases A–D settle the
  file set; measure instantiation before/after on one page first.
- Sources: [What's New in Qt 6.11](https://doc.qt.io/qt-6/whatsnew611.html),
  [QML type compiler](https://doc.qt.io/qt-6/qtqml-qml-type-compiler.html),
  [qmlls in 6.11](https://www.qt.io/blog/whats-new-in-qml-language-server-in-6.11).

## 11. What NOT to do (standing decisions)

- No OpenAPI/DTO codegen (already tried, generator is broken — stays dead).
- No new single-consumer QML components; three call sites or it stays inline.
- No new controllers. If a feature needs a home, it goes in an existing
  owner or replaces one.
- Don't re-add per-component `Keys.*` handlers "just for this one case" —
  that is the disease this plan cures.
- Don't mix `qmlformat` sweeps with logic commits.

## 12. Order & milestones

```
A (input router)  ──►  B (rows/cards)  ──►  C (player overlay)
                                       └──► D (page diets)
E (AppController)  ──►  F (MovieItem diet)          [C++ track, parallel]
G (hygiene)        independent, fill-in work
Perf items 1–4     same-day quick wins, any time
Perf 5–6 (Qt rebuild), 8, 9   after M1
```

- **M1 (device check):** after A+B — full D-pad walkthrough on the TV:
  home rows, grid, details, dropdowns, context menu long-press, player entry.
  This validates both the router and tonight's tactical fixes under real
  webOS key repeat, which desktop cannot reproduce.
- **M2:** after C+E+F — playback session end-to-end + memory snapshot
  (`memstats` line) before/after MovieItem diet.
- Record final LoC counts here and in `DESIGN.md` at close.

## Acceptance greps (run at every phase close)

```sh
grep -rn "Keys.on" qml | grep -v KeyRouter | grep -vi textfield | wc -l   # → 0 after A
grep -rn "function handleKey" qml | wc -l                                 # → ≤3 after A
grep -rn "function handleAcceptPressed" qml | wc -l                       # → 0 after A
grep -rn "isAutoRepeat" qml | wc -l              # → only KeyRouter after A
grep -rn "ensureVisible\|ensureItemVisible" qml | wc -l                   # → ≤2 after B
grep -rn "posterUrl\|landscapeCardUrl" src/common/JellyfinTypes.h | wc -l # → 0 after F
wc -l src/app/AppController.cpp                                           # → ≤700 after E
find qml/pages -name 'Player*' | xargs wc -l | tail -1                    # → ≤1500 after C
```
