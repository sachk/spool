# Frontend Refactoring Report — 2026-07-07

Deep audit of `src/` (~19k LoC) and `qml/` (~11.3k LoC) focused on
foundations to fix **before public release**: LoC reduction, bug-proneness,
library leverage, and usability. Items already covered by
`PERFORMANCE_PLAN.md` are excluded. `docs/codebase-audit.md` (2026-05-28) is
partially stale — several of its items have since shipped
(PlaybackPositionTracker/TrackParser split, HTTP retry policy, transcode
negotiation, VERSION file, schema-driven settings); this report reflects the
current tree.

**Headline:** the ~30k-line frontend can realistically get to ~24–25k with
items §1–§4 alone, and near the ~20k target once §5–§6 and the QML dialog
consolidation land. More importantly, §1–§3 remove the three patterns most
likely to breed bugs after release: hand-mirrored data schemas, a
forwarding god-object, and hand-rolled async joins.

Effort tags: **S** ≈ hours, **M** ≈ 1–3 days, **L** ≈ a week.

---

## 1. The `MovieItem` data pipeline is hand-mirrored five times (≈ −1,500 LoC, biggest bug-surface) — **L**

The same ~38 fields are maintained by hand in five places that can silently
drift:

1. The struct itself — `src/common/JellyfinTypes.h:117-154`.
2. Server-JSON parser (PascalCase keys) — `mediaItemFromJson`,
   `src/api/JellyfinApiFacade.cpp:313-411`, plus its own copies of the
   stream/source parsers (`mediaStreamsFromApiJson` :249-286,
   `mediaSourcesFromApiJson` :288-311).
3. Cache-JSON round-trip (camelCase keys, a *different* schema) —
   `toJson(MovieItem)`/`movieFromJson`, `src/common/JellyfinTypes.cpp:488-602`,
   plus a second, near-identical stream parser at :85-133.
4. The model, three more times — `MovieGridModel::data()` switch
   (`src/models/MovieGridModel.cpp:132-218`), `roleNames()` (:220-262), and
   `get()` building a 38-entry `QVariantMap` (:264-310), plus
   `detailsAt()`/`peopleVariantList`/`mediaStreamsVariantList`(:40-111) which
   mirror the streams a *fourth* time.
5. QML "snapshots": `get()` hands QML a `QVariantMap`; when QML passes it
   back, `AppController::movieFromSnapshot`
   (`src/app/AppController.cpp:1358-1364`) converts QVariantMap →
   `QJsonObject::fromVariantMap` → `movieFromJson` — a full JSON round-trip
   per context-menu action, dependent on the cache-schema key names.

Concrete hazards already present:

- `movieFromJson` (`JellyfinTypes.cpp:562-602`) and `mediaStreamFromJson`
  (:85-113) use **positional aggregate init with 38/24 values**. Reordering
  or inserting a struct field silently misassigns every later field —
  no compiler error. (Flagged in the old audit; still true.)
- Ticks are serialized as *strings* in the cache schema
  (`JellyfinTypes.cpp:507-508`, :144-146) but as numbers by the server;
  both parsers must remember to go through `.toVariant().toLongLong()`.
- `movieFromJson` invents `itemType` default `"Movie"`
  (`JellyfinTypes.cpp:570`) — a snapshot missing `itemType` becomes playable.

**Recommendation** (one coherent change, not five):

- Make `MovieItem`, `PersonItem`, `MediaStreamInfo`, `MediaSourceInfo`,
  `LibraryItem`, `MediaSegment` **`Q_GADGET`s with `Q_PROPERTY(... MEMBER ...)`**.
- Write one generic ~60-line serializer that walks `QMetaObject` properties
  (`fromJson<T>(QJsonObject, KeyStyle)` / `toJson(const T&)`, with a
  camelCase/PascalCase key policy and a small per-type override table for the
  few derived fields). This deletes essentially all of
  `JellyfinTypes.cpp:15-185,456-602` and the facade's field-copy loops; only
  real logic (image-URL building, subtitle derivation, playability rules in
  `JellyfinApiFacade.cpp:326-399`) remains, as a small post-parse step.
- Collapse `MovieGridModel` to **one gadget role** (`item`) plus the handful
  of derived display roles (`displayTitle`, `progress`, `playActionLabel`).
  QML reads `model.item.title`; `get(i)` returns the gadget by value;
  QML hands the gadget back to `Q_INVOKABLE`s — deleting `movieFromSnapshot`,
  the QVariantMap snapshot convention, `detailsAt()`, and the role
  boilerplate. Gadgets are value types in QML; no lifetime issues.
- Optional, if appetite exists: generate the gadget structs + key tables from
  the Jellyfin OpenAPI spec with a ~200-line in-tree Python generator —
  `tools/reduce_openapi.py` already slices the spec. Do **not** retry
  `openapi-generator cpp-qt6-client`; it was tried and is broken on
  Jellyfin's recursive `BaseItemDto` (`docs/codebase-audit.md:181`).
- Third-party alternatives considered and *not* recommended:
  `QtJsonSerializer` (Skycoder42) does meta-object JSON but is
  effectively unmaintained for Qt 6.11; `reflect-cpp` is excellent but
  doesn't know Qt value types. The 60-line in-house mapper wins.

**Status:** Phase 1 implemented the meta-object DTO mapper and cache/API
serialization cutover. `MovieGridModel` single-gadget role and QML snapshot
removal remain Phase 2 work.

## 2. `AppController` is a forwarding façade (≈ −700 LoC C++) — **M/L**

`src/app/AppController.{h,cpp}` (286 + 1,819 lines) mostly re-exports its
sub-controllers:

- ~200 lines of one-line delegating getters
  (`AppController.cpp:137-339`).
- ~60 lines re-emitting child signals as its own
  (`AppController.cpp:74-135`) so QML can observe them under one name.
- A **12-member `play*` wrapper family** that exists only to bind a model to
  an index: `playMovie/playResumeItem/playNextUpItem/playLatestLibraryItem/
  playSuggestionItem/playSearchResult/playDetailSeasonItem/
  playDetailSimilarItem/playPersonItem/...`
  (`AppController.cpp:565-676`, :861-884). QML already holds these model
  pointers (`appController.movies`, `resumeItems`, …), and
  `playOrOpenFromModel(MovieGridModel*, int, bool)` already exists (:547).
  Make **that** the single `Q_INVOKABLE` and delete the family.
- The playlist/collection/rename/delete block
  (`AppController.cpp:966-1299`, ~330 lines) is a self-contained feature —
  move to a `LibraryManagementController` alongside its policy flags and
  targets (`AppController.h:50-55`, :122-127). Each of its seven mutations
  repeats the same setBusy/run/refresh/error 15-line scaffold; one
  `runManagementMutation(label, task)` helper collapses them
  (:1077-1299 → ~80 lines).

**Structural recommendation:** expose `BrowseSessionController`,
`HomeModelController`, `ContentModelController`, `SearchController` (and the
new management controller) to QML the same way `player`/`settings`/`session`
already are (`AppController.h:71-77`) — or better, as QML singletons (§3).
`AppController` shrinks to app lifecycle + playback orchestration + the
busy/error surface.

## 3. Context properties → QML singleton/`QML_ELEMENT` registration — **M**

`src/main.cpp:684-692` injects `appController`, `router`, `nativeWindow`,
`i18n`, `isWebOS` as context properties; there is **zero**
`QML_ELEMENT`/`qmlRegister*` in `src/`. Consequences:

- Every unqualified `appController.…` lookup is a dynamic scope walk that
  qmlcachegen cannot compile to direct calls —
  `QT_QMLCACHEGEN_DIRECT_CALLS` (`CMakeLists.txt:370-372`) is largely
  neutralized for exactly the hottest bindings.
- `qmllint`/`pragma Strict` can never be turned on (everything is an
  unqualified unknown), and the performance plan's qmltc idea (§6) is
  impossible: qmltc does not support context properties.
- QML cannot type-check any controller access; typos surface at runtime on
  TV.

Register controllers with `QML_ELEMENT`/`QML_SINGLETON` (using
`qmlRegisterSingletonInstance` for the ones constructed in `main`), add the
`all_qmllint` target to CI, then incrementally enable `pragma
ComponentBehavior: Bound` + qualified access. This is the single best
"do it before release" foundation change: it makes the whole QML layer
statically analyzable.

## 4. QML duplicated key-handling state machines (≈ −800–1,200 LoC QML) — **M**

Grep facts: 75 `Keys.onPressed/handleKey/handlePressed` sites, 82
`InputKeys.focus()` calls, 10+ independent `function handleKey(key)`
implementations.

- `qml/pages/LibraryGridPage.qml:487-580` contains **three copies** of the
  same list-menu machine (libraryList / sortList / filterList): Up/Down with
  clamping, section skipping, Accept, Back-to-close, focus hand-back.
- `qml/shell/AppShell.qml:426-461` (`handleManagementKey`) is a fourth copy;
  `qml/shell/SyncPlayMenu.qml:97`, `qml/pages/PlayerOverlayMenu.qml`,
  `qml/shell/ItemContextMenu.qml`, `qml/pages/SettingsPage.qml:434` repeat
  the pattern again.
- Meanwhile `qml/primitives/NavList.qml` (35 lines) and `NavGrid.qml`
  (46 lines) already implement exactly this, and are barely used.

**Recommendation:** two primitives —
`MenuListView` (NavList + focus highlight + `section` skip + `accepted`/
`dismissed` signals) and `OverlayDialog` (scrim + `Surface` + focus grab +
back-to-close + click-outside) — then port every popup/menu onto them. This
also fixes behavioral drift (some menus wrap, some don't; some close on
Left, some only on Back).

Also extract the management dialog out of the shell: its state
(`AppShell.qml:37-44`), its logic (:322-461) and its UI (:778-904) are ~330
lines of a 1,024-line file that should be `ManagementDialog.qml`.

## 5. Navigation state has three owners — **M**

**Status (2026-07 refactor): done.** `RouterController` now owns route state,
`AppController` no longer carries a parallel page/NavigationState owner, and
details/search/person routes use router arguments.

- `RouterController` (C++ stack of route+args,
  `src/app/RouterController.cpp`),
- `AppController`'s `page` / `NavigationState`
  (`src/app/AppController.cpp:1453-1464`), reconciled into routes by
  `AppShell.controllerRoute()` (`qml/shell/AppShell.qml:108-126`) with a
  `Connections` bridge that `replace()`s on every page change,
- the shell's own `detailsModel/detailsIndex/detailsSource/detailsRoute/
  lastGridIndex/lastSearchIndex` blob (`AppShell.qml:14-27`, :177-213) —
  route arguments living outside the router that `RouteStack.args` already
  supports.

Two sources of truth means back-navigation bugs are structurally likely
(e.g. `back()` at `AppShell.qml:277-296` consults `routeStack`, then
`detailsRoute`, then route names, then `router.pop`). Make
`RouterController` the single owner: `page` disappears from
`AppController`, details/person routes carry their payload in `router.args`,
and `RoutePolicy.js` shrinks to pure helpers. Delete `NavigationState`.

Related usability surprise while here: on the **home** route, Back invokes
`switchUser()` (`AppShell.qml:287-289`) — pressing Back at Home dumps the
user to the login screen. If intended as the TV "exit" gesture, prompt or
minimize instead.

## 6. Let QCoro reach the controllers (clarity + real bugs avoided) — **M**

**Status (2026-07 refactor): done for the identified joins.**
`AppController::startPlayback` and `HomeModelController::refreshAsync` use
`QCoro::whenAll`; `AsyncTask.h` remains for one-shot leaf calls.

QCoro stops at the facade; every controller converts tasks back to callbacks
via `Async::runScoped/runLatest` (`src/common/AsyncTask.h:41-113`). The cost
is visible wherever two results must be joined:

- `AppController::playMediaItem` hand-rolls a join with
  `std::make_shared<int>(2)` + `kickoff` countdown lambda
  (`src/app/AppController.cpp:1381-1404`).
- `HomeModelController` maintains a whole `PendingHomeRefresh` struct with a
  `remaining` counter (`src/app/HomeModelController.h:62-69`).

With controller methods as `QCoro::Task<>` these become:

```cpp
auto [segments, trickplay] = co_await QCoro::whenAll(
    m_api->fetchMediaSegments(id), m_api->fetchTrickplay(id, msid));
```

Keep `RequestGeneration` for latest-wins (as
`const auto token = m_gen.next(); … if (!m_gen.isCurrent(token)) co_return;`)
and keep a thin `QPointer` guard for `this`. `AsyncTask.h` can stay for leaf
fire-and-forget calls, but stop using it as the only bridge — the counter
pattern will be copy-pasted again the next time two fetches must be joined.

## 7. `PlayerController`'s monolithic `stateChanged` notify — **M** (perf + binding hygiene)

18 of ~24 properties share `NOTIFY stateChanged`
(`src/player/PlayerController.h:28-60`), including `positionSeconds` (updated
4×/s by `m_uiPositionTimer`, `PlayerController.cpp:191-200`, plus every mpv
`time-pos` event) and `trickplaySheetUrls`, whose getter **builds a
QStringList of URLs on every evaluation** (`PlayerController.cpp:1322-1333`).
Every position tick therefore re-evaluates every binding on paused/buffering/
title/tracks/segments/trickplay across the entire player overlay — on a TV
GPU/CPU. Split the notifies (position/duration on their own signal at
minimum; tracks and segment state on theirs), or migrate the class to
`Q_OBJECT_BINDABLE_PROPERTY`. This is the one perf item not already in
`PERFORMANCE_PLAN.md`.

Smaller player items:

- `mpvCommand` builds a command string then re-tokenizes it by whitespace
  into argv (`PlayerController.cpp:947-965`); call sites could pass
  `QByteArrayList` argv directly and delete the parser.
- `rotateLogFile` is duplicated (`PlayerController.cpp:73-78` vs
  `src/main.cpp:100-111`).

## 8. API facade details — **S/M each**

**Status (2026-07 refactor): done except broader future API generation.** Image
and trickplay URLs are token-free, artwork fetches attach `Authorization`,
common headers are centralized, trickplay data comes from negotiation/detail
responses, and shared variant-query helpers live in `src/common/VariantUtils.*`.

- **Access token embedded in every image/trickplay URL**:
  `buildImageUrl` (`src/api/JellyfinApiFacade.cpp:531`) and
  `trickplayTileUrl` (:1367) append `api_key=<token>`. These URLs are written
  to the on-disk network cache, artwork cache keys, logs (sanitizer exists
  but only for diagnostics paths), and QML. They also invalidate every
  cached artwork entry when the token rotates. `ArtworkService` performs the
  fetches — attach the `Authorization` header there instead and key caches
  on token-free URLs. Security + cache-efficiency fix; do before release.
- `setAcceptLanguage` re-creates the common-header set and must remember to
  duplicate the `Accept` header from the constructor
  (`JellyfinApiFacade.cpp:460-468` vs :437-439) — drift waiting to happen;
  factor one `applyCommonHeaders()`.
- Legacy `X-Emby-Authorization` header (:1571); modern servers accept
  `Authorization: MediaBrowser …` — switch while no released clients depend
  on it.
- `fetchTrickplay` refetches the *entire item* to read the `Trickplay` field
  (:1313-1352) right after `PlaybackInfo`; ask for the field in the
  negotiation/detail fetch instead.
- URL secret redaction is now centralized in `redactedUrl`; variant query-list
  conversion is shared by the facade and library-cache key code.

## 9. `DatabaseManager` blocks the GUI thread per call — **M**

**Status (2026-07 refactor): done.** GUI-facing reads now return
`QCoro::Task<T>` and are awaited by session/settings/app controllers; only
database startup/shutdown retain synchronous worker barriers.

Every read (`loadSetting`, `loadAuthSession`, …) is a synchronous
`BlockingQueuedConnection` hop to the worker thread and back
(`src/cache/DatabaseManager.cpp:325-339`). This is the worst of both
worlds: GUI-thread stalls *and* thread-pool complexity — a slow write
(`saveHomePayload` of a large JSON blob) blocks the next GUI-thread read for
its full duration. Either (a) run SQLite directly on the GUI thread with WAL
(reads here are tiny), or (b) go genuinely async: return `QCoro::Task<T>`
from the manager (QCoro can wrap the queued call trivially), which also
unlocks performance-plan §5 (moving DB init off the critical path) without a
second mechanism.

## 10. Model-layer efficiency — **S**

- `MovieGridModel::removeUnresumable` (`src/models/MovieGridModel.cpp:435-451`)
  emits one `beginRemoveRows` per row — batch contiguous ranges.
- `setMovies` same-count path (:324-338) emits a full-range `dataChanged`
  with *all* roles, rebinding every delegate; pass the changed-role vector
  or just reset.
- `get()` allocates a 38-key QVariantMap per call and is invoked per
  keypress via `AppShell.currentMediaItem()` (`qml/shell/AppShell.qml:474-494`).
  Superseded by the §1 gadget role.

## 11. Usability findings

**Status (2026-07 refactor): done.** Management success/error signals and
background browse/content/search/settings errors route to queued toasts;
ordinary browse loads use inline `Browse.loadingMore` state instead of the
global busy scrim. Session and Quick Connect remain modal busy flows.

- **Success toasts never fire**: `ToastLayer` is instantiated
  (`qml/shell/AppShell.qml:993-997`) but nothing calls `toast.show()`, and
  `AppController::managementOperationSucceeded` (declared
  `src/app/AppController.h:206`, emitted in 8 places) has **zero QML
  consumers** (grep-verified). Users get no feedback after
  add-to-playlist/delete/rename.
- **Modal busy scrim for ordinary loads**: `setBusy(true, "Loading …")` on
  every non-cached library open (`src/app/AppController.cpp:498`, :827,
  :1563) blanks the whole UI behind a scrim (`AppShell.qml:749-776`).
  Prefer per-page skeletons/inline spinners (pairs with performance-plan
  §6's skeleton-first-frame item) and reserve the modal for auth flows.
- **Single-slot error surface**: one global `errorText` string; a second
  error overwrites the first, and background-refresh failures use the same
  channel as fatal ones (`AppShell.qml:999-1023`). Route background-refresh
  failures to the (fixed) toast instead.
- Back-at-Home = switch-user (§5 above).

## 12. Build & test structure (≈ −220 LoC CMake, faster builds) — **S**

**Status (2026-07 refactor): done.** `jellyfin-core` is a shared static target
for the app and focused tests; CTest currently registers 21 tests.

Historical finding: `CMakeLists.txt:455-736` used to make test executables
re-list and recompile shared sources (`DatabaseManager.cpp`, the facade,
`Diagnostics.cpp`) up to 3×. The refactor moved common code into
`jellyfin-core`; remaining work is CI wiring for `qt_add_qml_module`'s
generated `all_qmllint` target once §3 lands.

## Final size snapshot after the 2026-07 refactor

- `src/**/*.cpp,h`: 97 files, 18,662 lines.
- `qml/**/*.qml,js`: 62 files, 12,336 lines.
- `tests/**/*.{cpp,h,qml,js}`: 21 files, 2,583 lines.
- Large UI/code anchors: `AppShell.qml` 800 lines,
  `PlayerOverlayPage.qml` 692, `SettingsPage.qml` 797,
  `LibraryGridPage.qml` 1,003, `AppController.cpp` 1,136,
  `JellyfinApiFacade.cpp` 1,636, `PlayerController.cpp` 1,591.

## 13. Library verdicts & opportunities

**Keep — pulling their weight:** QCoro (the facade would be ~2× the code
with signal/slot chains; extend it upward, §6); `QRestAccessManager` +
`QNetworkRequestFactory` + `QHttpHeaders` (modern Qt 6.7+ HTTP used
correctly — factory-owned base URL/headers/timeout is exactly right);
`qmlcachegen` AOT; the Material-font icon approach; the in-house
`HttpRequestPolicy`/`RequestGeneration` (small, tested, right-sized).

**Adopt (small, in-tree):** the meta-object JSON mapper (§1) — this is the
"library" that saves the most lines, and it's 60 lines you own.

**Do not adopt:** `openapi-generator` cpp-qt6 client (known-broken on
Jellyfin's spec, and generated Qt clients are far larger than the current
facade); `QtJsonSerializer` (unmaintained risk); `reflect-cpp`/`glaze`
(no Qt value-type support); Kirigami/Felgo (desktop/mobile-shaped, heavy
deps, wrong fit for TV D-pad UX). There is no credible off-the-shelf QML
TV-spatial-navigation library — consolidating `NavList`/`NavGrid`/
`InputKeys` (§4) *is* the right play.

**Qt 6.11 features currently unused that would pay:** singleton/
`QML_ELEMENT` registration + qmllint (§3);
`Q_OBJECT_BINDABLE_PROPERTY`/`QProperty` for the player (§7); `Q_GADGET`
value types in models (§1); `QCoro::whenAll` (§6).

---

## Priority order

| # | Item | Effort | Payoff |
|---|------|--------|--------|
| 1 | §1 gadget DTOs + generic JSON mapper + single-role model | L | −1,500 LoC, kills the top bug surface |
| 2 | §3 QML registration + qmllint in CI | M | static analysis for all QML, unblocks qmltc |
| 3 | §2 dissolve AppController façade (incl. management controller) | M/L | −700 LoC, real module boundaries |
| 4 | §4 MenuListView/OverlayDialog + shell extraction | M | −1,000 LoC QML, consistent input UX |
| 5 | §8 token-free artwork URLs | S/M | security before release |
| 6 | §7 player notify split | M | overlay perf, binding hygiene |
| 7 | §5 single router | M | back-nav correctness |
| 8 | §6 QCoro in controllers | M | deletes counter-join pattern |
| 9 | §11 toasts/error routing/busy scrim | S | visible polish |
| 10 | §9 DB async, §10 model batching, §12 CMake core lib | S each | hygiene |

Sequencing note: do §1 and §3 first — both change QML-facing surfaces, and
every later item (and every page still to be written) gets cheaper once
models expose gadgets and QML is lint-clean.
