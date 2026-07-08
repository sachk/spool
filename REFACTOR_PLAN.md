# REFACTOR_PLAN — pre-release foundation work

Execution plan for `docs/refactor-report-2026-07.md`. Written 2026-07-07 for
the next agent(s). The OpenAPI→DTO generator idea is **explicitly dropped**:
DTO structs stay hand-written; only the serialization becomes generic.

## Ground rules

- One phase = one reviewable unit. Land phases in order; later phases assume
  earlier ones. Within a phase, commit in the listed step order.
- Current completion pass verification is desktop-only unless the user explicitly
  asks otherwise: use a foreground `nix develop .#native -c ...` app run that
  exits or is killed after about 10 seconds, and inspect its stdout/stderr for
  homepage render/startup regressions. Do not spend time on webOS cross-builds,
  IPK install, on-device smoke, or broader acceptance loops during this pass.
- Do not pick up `PERFORMANCE_PLAN.md` items opportunistically; the only
  sanctioned overlaps are noted inline (Phase 9 DB, Phase 10 skeletons).
- When a phase says "sweep", run the given grep first, record the count, and
  verify the count is zero (or the expected residue) afterwards.
- Update `DESIGN.md` and mark the corresponding section of
  `docs/refactor-report-2026-07.md` as done at the end of each phase.

Current baseline: `src/` ≈ 19,037 LoC, `qml/` ≈ 11,312 LoC (30.3k total).
Target after all phases: ≈ 20–22k.

---

## Phase 0 — Build safety net (S, ~−220 LoC CMake)

Goal: make every later phase cheaper to verify.

1. In `CMakeLists.txt`, create a `jellyfin-core` STATIC library holding
   everything currently listed in `qt_add_executable(jellyfin-native …)`
   (`CMakeLists.txt:164-254`) **except** `src/main.cpp` and the
   platform-window sources. Link it into `jellyfin-native`.
   Keep `qt_add_qml_module` attached to the executable target (QML module
   registration must stay on the app target).
2. Rewrite the test block (`CMakeLists.txt:455-736`): each of the 18 tests
   becomes `add_executable(<name> tests/…/<Name>Test.cpp)` +
   `target_link_libraries(<name> PRIVATE jellyfin-core …)` inside a
   `foreach()` over a `NAME;SOURCE` list. Delete the per-test source
   re-listings (they currently recompile `DatabaseManager.cpp`,
   `JellyfinApiFacade.cpp`, `Diagnostics.cpp` up to 3×).
3. Acceptance: `ctest` passes; desktop and webOS configure+build succeed;
   `nm`-visible symbols unchanged in the app binary.

## Phase 1 — Gadget DTOs + one generic JSON mapper (L, ~−900 LoC)

Report §1. C++-only phase; QML sees no change yet.

1. In `src/common/JellyfinTypes.h`, add `Q_GADGET` +
   `Q_PROPERTY(<type> <name> MEMBER <name>)` to: `MovieItem` (:117-154),
   `PersonItem` (:68-75), `MediaStreamInfo` (:77-102), `MediaSourceInfo`
   (:104-115), `LibraryItem` (:20-26), `MediaSegment` (:163-168),
   `DiscoveredServer` (:14-18). Rename `MovieItem` fields **only if** you
   standardize `id` now (recommended — see Phase 2 step 4, they must land
   together or not at all; if unsure, keep names).
   - Container members: switch `std::vector<PersonItem>` /
     `std::vector<MediaStreamInfo>` / `std::vector<MediaSourceInfo>` to
     `QList<…>` on the DTOs. Rationale: implicit sharing (these get copied
     into models/sessions today) and native QML sequence support later.
     `PagedMovieItems::items` and controller-internal vectors may stay
     `std::vector` — converting at the facade boundary is fine.
2. Create `src/common/MetaJson.h` (~80 lines): a header-only mapper walking
   `T::staticMetaObject`:
   - `template<typename T> QJsonObject metaToJson(const T&)`
   - `template<typename T> T metaFromJson(const QJsonObject&)`
   - Key policy parameter: `CamelCase` (property name as-is) and
     `PascalCase` (uppercase first letter) — the latter is for Phase 8's
     optional server-side reuse; Phase 1 only needs CamelCase.
   - Type dispatch: QString, bool, int, qint64 (serialize as JSON number,
     read via `toVariant().toLongLong()` so legacy string ticks still
     parse), double, QStringList, nested gadget, `QList<gadget>`.
     Unknown property type = `static_assert`/`qFatal` at registration, not
     silent skip.
3. Replace hand-written serialization with the mapper:
   - Delete `src/common/JellyfinTypes.cpp:15-185` (array/stream/source
     helpers) and `:456-602` (`toJson`/`…FromJson` for all structs); keep
     `BrowseDescriptor` (it has real key logic), `exceptionMessage`,
     resume-ticks helpers.
   - The four call sites of the old free functions
     (`AppController.cpp:129`, `:971`, `:1363`; `HomeModelController`
     payload code; `DatabaseManager` consumers) switch to
     `metaToJson`/`metaFromJson`.
   - Post-parse fixups that were baked into `movieFromJson` (default
     `itemType` `"Movie"` at `JellyfinTypes.cpp:570`) move to explicit call
     sites or are dropped (report flags the default as a playability
     hazard — drop it; empty `itemType` must stay non-playable).
4. **Bump cache schema versions** so old blobs (string ticks, old defaults)
   are discarded rather than misparsed: `kHomePayloadSchemaVersion`
   (`src/app/AppController.cpp:40`) and the discovered-servers /
   DB schema version in `src/cache/DatabaseManager.cpp`.
5. Facade: keep `mediaItemFromJson` (`src/api/JellyfinApiFacade.cpp:313-411`)
   hand-written for now (it contains real logic: image URLs, subtitles,
   playability) but delete its private duplicates
   `mediaStreamsFromApiJson`/`mediaSourcesFromApiJson` (:249-311) by adding
   a PascalCase read path to the mapper for `MediaStreamInfo` /
   `MediaSourceInfo` + tiny fixups (frameRate fallback, container cleaning).
6. Tests: extend `tests/common/BrowseDescriptorTest.cpp` siblings with a
   `MetaJsonTest` covering round-trip of every DTO, legacy string-ticks
   input, and unknown-key tolerance. Update `PlaybackTrackParser`/queue
   tests if container types changed signatures.
7. Acceptance: `ctest` green; app runs against a live server; grep
   `"QStringLiteral(\"id\"), " src/common` returns nothing.

## Phase 2 — Single-gadget-role model + kill snapshots (M, ~−450 C++ / −significant QML churn)

Report §1 items 4–5. Depends on Phase 1.

1. `src/models/MovieGridModel.{h,cpp}`: reduce roles to
   `{ItemRole ("item"), DisplayTitleRole, DisplaySubtitleRole, ProgressRole,
   PlayActionLabelRole}`. `ItemRole` returns
   `QVariant::fromValue(m_movies[row])`. Delete the 38-case `data()` switch
   (`MovieGridModel.cpp:132-218`), the `roleNames()` table (:220-262),
   `get()`'s map (:264-310 → return the gadget: `Q_INVOKABLE MovieItem
   get(int) const`), and `detailsAt()` + its variant-list helpers (:40-111 —
   QML reads `item.people` / `item.mediaSources` directly once `QList<gadget>`
   sequences are registered; verify iteration works in QML, else expose two
   tiny invokables returning `QVariantList`).
2. QML delegate sweep. Every delegate switches from context roles to
   `required property var item` (+ the four derived roles). Sweep greps:
   `grep -rn "movieId\|posterUrl\|resumeTicks\|itemType" qml | wc -l`
   before/after. Files with the heaviest usage: `MediaItemCard.qml`,
   `LandscapeCard.qml`, `HomeHorizontalRow.qml`, `MediaPosterScrollerRow.qml`,
   `ItemDetailsPage.qml`, `ItemContextMenu.qml`, `MediaInfoOverlay.qml`,
   `RoutePolicy.js` (`itemIdFor` at `qml/shell/RoutePolicy.js:3-9`).
3. Snapshot removal: `Q_INVOKABLE`s taking `const QVariantMap &item`
   (`AppController.h:144-184`) now take `const MovieItem &` — QML passes the
   gadget it got from `get()`/`item` role. Delete
   `AppController::movieFromSnapshot` (`AppController.cpp:1358-1364`) and
   `itemIdsForManagement`'s snapshot path (:986-990).
4. Naming: standardize the gadget's id property as `id` is **not** usable in
   QML (reserved) — keep `movieId` as the Q_PROPERTY name for the `id`
   member (`Q_PROPERTY(QString movieId MEMBER id)`) so QML keeps reading
   `item.movieId`. Do the same reserved-name check for every property.
5. Acceptance: all pages browse/play correctly on desktop; context menu,
   media info, management actions work; `ctest` green; no
   `QVariantMap` parameters remain on `AppController` invokables.

## Phase 3 — QML singletons + qmllint (M, small LoC, big leverage)

Report §3. Do before the QML-heavy phases so lint catches their mistakes.

1. Register instances created in `main` into the existing module URI:
   `qmlRegisterSingletonInstance("JellyfinWebOS", 1, 0, "App", controller)`,
   same for `Router`, `NativeWindow`, `I18n`; replace the `isWebOS` context
   property with a tiny `Platform` singleton gadget/object. Delete the
   `setContextProperty` block (`src/main.cpp:684-692`).
   Ordering gotcha: registration must happen before
   `window.loadFromModule(...)` (`src/main.cpp:699`).
2. Sweep QML: `appController` → `App`, `router` → `Router`,
   `nativeWindow` → `NativeWindow`, `i18n` → `I18n`, `isWebOS` →
   `Platform.isWebOS`. Mechanical sed + manual review of null-guards
   (`appController ?` guards in `AppShell.qml:13-55` etc. can go —
   singletons are never null; keep guards only where teardown-order
   warnings were the motivation, see `src/main.cpp:634-663` comment).
3. Turn on lint: `qt_add_qml_module` already generates
   `jellyfin-native_qmllint`; add it to CI and to the local build docs. Fix
   or annotate every warning. Add `pragma ComponentBehavior: Bound` to
   delegates as lint suggests.
4. Acceptance: zero unqualified-lookup lint warnings; app boots on both
   targets; startup smoke passes.

## Phase 4 — Dissolve the AppController façade (M/L, ~−700 LoC)

Report §2. Depends on Phase 3 (QML now references singletons; adding more
exposed controllers is cheap and checkable).

1. New `src/app/LibraryManagementController.{h,cpp}`: move policy flags +
   `playlistTargets`/`collectionTargets` (`AppController.h:50-55,122-127`),
   `loadCurrentUserPolicy` (`AppController.cpp:931-964`), and the mutation
   family (:966-1299). Add one private
   `runMutation(QString busyLabel, QCoro::Task<void>, QString successToast)`
   helper to collapse the seven identical setBusy/run/refresh/error
   scaffolds (:1077-1299). It emits `operationSucceeded` / `errorOccurred`;
   AppController (or Phase 10's status surface) subscribes.
2. Expose `browse`, `home`, `content`, `search`, `management` to QML exactly
   like `player`/`settings`/`session` already are (`AppController.h:71-77`)
   — CONSTANT pointer properties are fine; separate singletons are optional
   polish.
3. Delete the forwarding layer:
   - getters `AppController.cpp:137-339` that merely delegate;
   - signal re-emission wiring in the constructor (:74-135) — QML connects
     to the owning controller;
   - the `play*` wrapper family (:565-676, :861-884): replace with a single
     `Q_INVOKABLE void playFromModel(MovieGridModel *model, int index, bool
     fromStart)` (rename of `playOrOpenFromModel`, :547-563) plus
     `Q_INVOKABLE void openFromModel(...)` if needed. QML passes the model
     pointer it already holds.
4. QML sweep: call sites of removed invokables/properties
   (`grep -rn "App\.play\|App\.currentLibrary\|App\.detail" qml`).
5. Acceptance: `AppController.cpp` ≤ ~900 lines; header ≤ ~150; qmllint
   clean; browse/play/manage flows verified by hand on desktop.

## Phase 5 — One navigation owner (M)

Report §5. Depends on Phase 4 (page-change side effects have obvious homes).

1. `RouterController` becomes the sole route owner. Delete
   `NavigationState` (`src/app/NavigationState.{h,cpp}` + its test) and the
   `page` property/`setPage` (`AppController.cpp:1453-1464`). Side effects
   currently keyed on page changes (discovery start/stop, :1460-1463) move
   to `SessionController` state transitions.
2. Details/person routes carry payload in `router.args`
   (`RouteStack.args` already plumbs it — `qml/shell/RouteStack.qml:8`):
   `{itemId, itemType, source, returnRoute, focusIndex}` replaces the
   shell-held `detailsModel/detailsIndex/detailsSource/detailsRoute`
   blob (`qml/shell/AppShell.qml:14-27,177-213`). Pages resolve their model
   from `source` via a small lookup in `RoutePolicy.js` (which shrinks to
   pure helpers).
3. Rewrite `AppShell.back()` (:243-297) as: overlays (in z-order) → player →
   `routeStack.handleBack()` → `Router.pop(fallback)`. The route-name
   special cases disappear because return routes ride in args.
4. Decide the Back-at-Home behavior (currently `switchUser()`,
   `AppShell.qml:287-289`): recommended — first press shows a "Press back
   again to exit" toast, second press within 2.5 s quits; move switch-user
   to the TopBar/user menu only.
5. Acceptance: `tests/app/RouterControllerTest.cpp` extended for
   args-carrying push/replace/pop; back-stack manual test matrix: home ↔
   grid ↔ details ↔ person ↔ search ↔ settings ↔ player.

## Phase 6 — QML input primitives + shell diet (M, ~−1,000 LoC QML)

Report §4 + §6. Can start any time after Phase 3; ideally after 5 so dialog
focus interacts with the final back logic.

1. New `qml/primitives/MenuListView.qml`: NavList
   (`qml/primitives/NavList.qml`) + focus/highlight visuals + `section`-row
   skipping + `accepted(index)` / `dismissed()` + optional edge-escape to a
   named button. New `qml/primitives/OverlayDialog.qml`: scrim + `Surface` +
   focus capture/restore + Back/click-outside dismissal.
2. Port, one component per commit:
   - `LibraryGridPage.qml` library/sort/filter menus (state machines at
     :487-580 plus their three list views) — the largest single win;
   - the shell management overlay → new `qml/shell/ManagementDialog.qml`
     (state `AppShell.qml:37-44`, logic :322-461, UI :778-904);
   - `SyncPlayMenu.qml:97`, `PlayerOverlayMenu.qml`, `ItemContextMenu.qml`,
     `SettingsPage.qml:434` key handlers.
3. After porting, `AppShell.qml` should be ≤ ~600 lines: key routing,
   overlay loaders, toast/error surfaces only.
4. Acceptance: every menu behaves identically for Up/Down/Accept/Back/
   click-outside; `grep -rn "function handleKey" qml | wc -l` drops from
   10+ to ≤ 4 (pages that genuinely own grids keep theirs);
   D-pad walkthrough on device or desktop keyboard.

## Phase 7 — Player notify split (M)

Report §7. Independent; schedule around any active player work.

1. `src/player/PlayerController.h:28-60`: regroup NOTIFY signals —
   `positionChanged` (positionSeconds, durationSeconds),
   `playbackStateChanged` (paused, buffering, bufferingPercent, seeking,
   statusText, backAllowed), `tracksChanged` (subtitle/audio lists +
   indices + subtitlesEnabled), `segmentsChanged` (activeSegmentType,
   activeSegmentEndSeconds), `trickplayChanged` (trickplayAvailable,
   trickplaySheetUrls), keep `chaptersChanged`/`visibleChanged`/etc.
   Replace each `emit stateChanged()` in `PlayerController.cpp` with the
   narrow signal(s); the 4 Hz UI tick (:191-200) must emit **only**
   `positionChanged`.
2. Cache `trickplaySheetUrls` in a member rebuilt when the timeline/session
   changes (getter currently rebuilds a QStringList per evaluation,
   :1322-1333).
3. Small cleanups while in file: pass argv (`QByteArrayList`) into
   `mpvCommand` and delete the whitespace re-tokenizer (:947-965); move
   `rotateLogFile` to a shared helper used by `src/main.cpp:100-111` too.
4. Acceptance: playback overlay updates correctly (position, pause,
   buffering, tracks, skip-intro card, trickplay scrub); QML profiler (or
   simple binding counters) confirms non-position bindings no longer fire
   on ticks.

## Phase 8 — Facade security + hygiene (S/M)

Report §8. Independent.

1. **Token out of URLs**: drop `api_key` from `buildImageUrl`
   (`src/api/JellyfinApiFacade.cpp:531`) and `trickplayTileUrl` (:1367).
   `ArtworkService`'s fetch worker attaches
   `Authorization: MediaBrowser …, Token="…"` instead (token provided via a
   setter from the session). Ensure **all** remote images go through
   `image://artwork` — sweep QML for direct `http` Image sources; trickplay
   tiles (`PlayerTrickplayPreview.qml`) must switch to the provider.
   Cache keys become token-free automatically (they're URL-based).
2. Header hygiene: single `applyCommonHeaders()` used by the constructor
   (:437-439) and `setAcceptLanguage` (:460-468); switch
   `X-Emby-Authorization` (:1571) to standard `Authorization`.
3. `fetchTrickplay` (:1313-1352): request the `Trickplay` field in the
   negotiation/detail response instead of refetching the item.
4. Acceptance: artwork + trickplay load with the token absent from every
   URL in logs and in `$XDG_CACHE_HOME` network-cache entries; login,
   playback, and reporting still work against a real server.

## Phase 9 — Async consistency: QCoro upward + DB futures (M)

Report §6 + §9. After Phase 4 (fewer owners to convert).

1. Controllers may be coroutines. Convert the two hand-rolled joins first:
   - `AppController::playMediaItem` counter join
     (`AppController.cpp:1381-1404`) → `co_await QCoro::whenAll(
     fetchMediaSegments(id), fetchTrickplay(id, msid))`;
   - `HomeModelController`'s `PendingHomeRefresh.remaining` machinery
     (`HomeModelController.h:62-69` + .cpp) → one coroutine awaiting
     `whenAll` over resume/next-up/latest fetches.
   Guard pattern after every `co_await`: `QPointer self{this}; … if (!self)
   co_return;` and generation check `if (!m_gen.isCurrent(token)) co_return;`.
   `AsyncTask.h` stays for leaf fire-and-forget calls.
2. `DatabaseManager`: replace `BlockingQueuedConnection` reads
   (`src/cache/DatabaseManager.cpp:325-339`) with `QFuture`-returning
   methods (worker fulfills a `QPromise`), consumed as
   `co_await database.loadAuthSession()` (QCoro awaits QFuture natively).
   Keep at most two documented synchronous calls for pre-window startup
   (device id / auth session) — or none, which also delivers
   PERFORMANCE_PLAN §5's "sqlite off the critical path" as a side effect
   (coordinate; don't double-implement).
3. Acceptance: `ctest` green (Home/Browse controller tests updated to pump
   the loop for coroutines); no `BlockingQueuedConnection` remains in
   `DatabaseManager.cpp`; cold start still applies the warm home cache.

## Phase 10 — Feedback & status UX (S/M)

Report §11. After Phase 4 (signals have final owners).

1. Wire toasts: `ToastLayer` (`qml/shell/AppShell.qml:993-997`) gets shown —
   connect management `operationSucceeded` (and future success events) to
   `toast.show(...)`. Support 2–3 queued messages instead of the single
   `message` slot (`qml/shell/ToastLayer.qml`).
2. Error routing: background/refresh failures (e.g. warm-cache refresh at
   `AppController.cpp:507-517`) go to the toast; the modal error surface
   (`AppShell.qml:999-1023`) is reserved for actionable/blocking errors.
3. Busy scrim: stop calling `setBusy(true)` for ordinary page loads
   (`AppController.cpp:498`, :827, :1563 …). Pages show their own inline
   loading state (grid: dim + spinner in the toolbar area, or the
   skeleton-first approach if PERFORMANCE_PLAN §6 has landed — coordinate).
   Keep the modal for auth/quick-connect only.
4. Acceptance: add-to-playlist/rename/delete show a toast; killing the
   network mid-browse produces a toast, not a full-screen modal; login
   still shows the modal busy state.

## Phase 11 — Cleanup & docs (S)

1. Dedup leftovers: secret-redaction regex (`JellyfinTypes.cpp:625-631` vs
   `Diagnostics.cpp`), any remaining `queryStringList`-style triplication
   once QVariantMap queries are gone from hot paths.
2. Delete dead code found along the way (Phase 4/5 will strand helpers —
   `showCurrentItems`, `NavigationState`, `RoutePolicy` leftovers).
3. Update `DESIGN.md` (architecture changed: singletons, controllers,
   router) and fold the completed sections of
   `docs/refactor-report-2026-07.md`; mark superseded entries in
   `docs/codebase-audit.md` as done.
4. Re-run the LoC count and record it here.

---

## Dependency graph / suggested schedule

```
P0 ──► P1 ──► P2 ──► P3 ──► P4 ──► P5
                      │      │
                      ├──► P6 (after P3, best after P5)
                      │      └──► P9, P10 (after P4)
                      └──► P7, P8 (anytime after P0, independent)
```

Two-agent split, if parallelized: Agent A takes P0→P1→P2→P3→P4→P5 (the
spine); Agent B takes P7 and P8 (independent files), then joins for P6.

## Expected LoC deltas (rough)

| Phase | C++ | QML | Notes |
|-------|-----|-----|-------|
| P0 | −220 (CMake) | — | plus large test-build-time win |
| P1 | −900 | — | JellyfinTypes.cpp mostly gone |
| P2 | −450 | ~−200 net | delegate churn, snapshots gone |
| P3 | −10 | ~0 | leverage, not lines |
| P4 | −700 | −50 | façade dissolved |
| P5 | −150 | −150 | NavigationState + shell route blob |
| P6 | — | −900 | menus + shell diet |
| P7–P11 | −250 | −100 | hygiene, dedup |
| **Total** | **≈ −2,700** | **≈ −1,400** | ≈ 30.3k → ≈ 26k; further QML shrink comes out of P2/P6 follow-through in pages |

## Verification appendix

- Desktop foreground smoke only for this completion pass: run the native app
  through `nix develop .#native -c ...`, make it exit or kill it after about
  10 seconds, and inspect stdout/stderr for startup/homepage/QML regressions.
  The user will perform full webOS/manual acceptance after the plan is done.
- QML: `cmake --build <dir> --target jellyfin-native_qmllint` is useful after
  Phase 3, but during this pass the required runtime check remains the
  foreground nix app run above.
- Greps that must trend to zero: `setContextProperty` (P3),
  `movieFromSnapshot|QVariantMap &item` in `AppController` (P2),
  `function handleKey` count > 4 in `qml/` (P6), `api_key` in `src/api`
  (P8), `BlockingQueuedConnection` in `src/cache` (P9).
