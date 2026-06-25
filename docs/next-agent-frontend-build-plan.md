# Next Agent Plan: Frontend Foundation, QCoro, And Build Architecture

Status: handoff plan.
Created: 2026-06-09.
Scope: application repo outside the `mpv/` fork, except where build scripts need
to build or package the `mpv` submodule.

This plan combines three related starts:

- Use Qt Quick Controls/Templates properly for the app's own UI system.
- Replace the local fake QCoro shim with proper upstream QCoro.
- Rework build/package/CI architecture so Linux, macOS, AppImage, and webOS
  static-Qt builds are reproducible and cacheable.
- Split platform mpv usage into separate `mpv` and `mpv_webos` submodules.
- Add guardrails so Nix flake inputs stay in sync with submodule revisions.

Do not treat this as one giant patch. Treat it as a set of small conventional
commits with explicit verification after each phase.

## Constraints

- Follow `AGENTS.md`.
- Do not push.
- Do not use the old `webos-mpv-demo-cycle.sh` flow.
- Do not deploy to the TV unless the user explicitly asks.
- Keep `mpv/` changes inside the submodule if any become necessary.
- Prefer `nix develop -c ...` for normal build/package work.
- Keep build changes reproducible on GitHub Actions, not dependent on local
  sibling directories or hidden machine state.

## Branch Setup

The primary branch for this repo is currently `master` (`origin/HEAD` points to
`origin/master`). Start from an up-to-date primary branch before doing this work:

```sh
git fetch origin
git switch master
git pull --ff-only origin master
```

For implementation, create a new topic branch from current `master` unless the
user explicitly asks for direct commits on `master`:

```sh
git switch -c refactor/frontend-build-foundation
```

Do not base this work on the old `webos/lifecycle-cursor-vkb` branch. That
branch has already been merged into `master`.

## First Principle

The most important runtime foundation is still typed navigation state with stable
item identity. Qt Quick Controls/Templates will make the UI cleaner, but it will
not fix stale `model + index` selection by itself.

For frontend work, do this order:

1. Establish shared input/control primitives.
2. Introduce typed route/item identity.
3. Move pages onto the cleaner primitives and route model.

For build work, do this order:

1. Make existing build paths reproducible and testable.
2. Split scripts into explicit phases.
3. Add static Qt cache/artifact workflows.
4. Expand platform matrix.

## Workstream A: Qt Quick Controls/Templates

### A1. Define UI Technology Policy

Use Qt Quick Controls and Qt Quick Templates as the base UI framework.

Do not adopt Kirigami for webOS TV in this phase. Kirigami may be useful later
for a desktop/mobile shell, but webOS needs a remote-first 10-foot UI and a very
controlled static Qt package.

Policy:

- WebOS TV UI uses custom controls backed by `QtQuick.Templates`.
- Desktop/Linux/macOS can use the same components first; platform-native style
  can be evaluated later.
- Avoid adding KDE Frameworks dependencies until the build system is stable.
- Keep QML import lists static-build friendly.

### A2. Add Shared Input Classification

Current QML files duplicate accept/back/key handling. Add a shared singleton or
small attached helper for:

- Accept keys: Return, Enter, Select, Space.
- Back keys: Back, Escape, Backspace, BrowserBack, webOS scan-code variants.
- Direction keys.
- Media/color keys.

Candidate file:

- `qml/primitives/InputKeys.qml`, or
- `qml/input/InputKeys.qml` if introducing a new module.

Then migrate page-level checks gradually. Do not change behavior and component
structure in the same commit.

Verification:

- Run QML import scanning/build configure after each import/module change.
- Manually inspect static QML plugin list generation or scanner output.

### A3. Rebuild Primitives On Templates

Start with the primitives that are already custom controls:

- `ActionButton.qml`
- `IconButton.qml`
- `TextFieldRow.qml`
- `SelectRow.qml`
- `ToggleRow.qml`
- `SliderRow.qml`
- `SettingRow.qml`
- `MediaItemActions.qml`

Target:

- Separate control logic from visual skin.
- Use `QtQuick.Templates as T` for control behavior where appropriate.
- Keep TV focus rings and D-pad behavior explicit.
- Use `Action`/command objects where useful, especially for settings and player
  actions.
- Keep visual tokens in `Theme.qml` and size tokens in `Metrics.qml`.

Do not rewrite all pages at once. Convert one primitive, update its direct uses,
then verify.

### A4. Add Route-Safe Page APIs

Once shared input primitives exist, start the navigation refactor:

- Replace `shell.openDetails(model, index, source, returnRoute)` with
  item-id based route requests.
- Keep old index state only as focus-restoration hints.
- Add invalid route handling for missing item ids.
- Route object should include stable item id and return route.

Expected C++/QML shape:

- `Router` or `NavigationState` QObject exposed to QML.
- `ItemDetailsRoute { itemId, itemType, returnRoute }`.
- Page activation by `itemId`, not by source string plus index.

Verification:

- Add a small test or debug assertion for details route creation without an id.
- Exercise: open from library, search, latest, resume, person, similar; start
  playback; stop playback; confirm details still has the selected item.

## Workstream B: Replace Local QCoro Shim With Upstream QCoro

### B1. Inventory Current Usage

Run:

```sh
rg -n "QCoro|runDetached|Task<|co_await" src third_party CMakeLists.txt flake.nix
```

Current local shim lives under `third_party/qcoro/` and pretends to be QCoro. It
is intentionally minimal. Replace it with real upstream QCoro rather than
growing the shim.

### B2. Add Proper Dependency

Use packaged upstream QCoro where available:

- Nix dev shell: add the Qt 6 QCoro package from nixpkgs.
- CMake: use upstream `find_package(...)` and imported targets. Verify exact
  target names from the installed package; common Qt 6 packages expose
  `QCoro6`/`QCoro6::Core`/`QCoro6::Network` style targets, while the current shim
  exposes `QCoro::Core` and `QCoro::Network`.
- GitHub Actions: ensure Linux and macOS jobs install/enter the same Nix
  environment.
- webOS: decide whether QCoro is host/header-only enough for the target or
  should be built as part of the webOS third-party manifest. Do not assume the
  host package is usable for cross builds.

Recommended transition:

1. Add a build option such as `JELLYFIN_USE_BUNDLED_QCORO=OFF`.
2. Wire upstream QCoro first.
3. Keep the shim only as a temporary fallback if webOS cross-build cannot use
   upstream immediately.
4. Remove `third_party/qcoro` once all target builds pass.

### B3. Improve Async Patterns While Migrating

Do not simply replace include paths. Use the migration to reduce repeated
generation-token boilerplate:

- Add a small helper for route/session-scoped async work.
- Centralize error handling for detached tasks.
- Avoid silent failure callbacks for playback reporting and SyncPlay commands.

Verification:

- Linux configure/build.
- AppImage build.
- webOS configure/build, at least without deployment.
- macOS workflow path on macOS runner if available.

## Workstream C: Build Architecture Refactor

### C1. Define Build Matrix

Write the build axes down in code/config, not just docs:

- Host OS: Linux, macOS, future Windows.
- Target OS: Linux desktop, macOS, webOS, future Windows, Android, Android TV,
  iOS, tvOS.
- Qt source: nixpkgs, source-built Qt, prebuilt cached Qt archive.
- Qt linkage: shared, static.
- Media backend: desktop libmpv render, webOS Starfish/libmpv, future mobile
  backend.
- Package format: AppImage, DMG, IPK, future installers/APK/AAB/IPA.

Candidate files:

- `CMakePresets.json`
- `tools/build-targets/*.env`
- `tools/manifests/*.json`

### C2. Create Source Manifests

Add manifests for external source dependencies. Every download must have a
version/revision and checksum.

Candidate manifests:

- `tools/manifests/qt-webos-6.11.json`
- `tools/manifests/webos-third-party.json`
- `tools/manifests/linuxdeploy.json`

Include:

- Qt version.
- Module list.
- Source URLs.
- SHA256 hashes.
- Patch list.
- Configure flags.
- Toolchain/sdk identity.
- `mpv` and `mpv_webos` submodule commits.

Cache keys in CI should be based on these manifests plus patch/script hashes.

### C3. Split webOS Build Into Phases

Current `build-ipk.sh` does too much. Split into phase scripts or a single
orchestrator that calls phase functions:

- Fetch source archives.
- Build Qt host tools.
- Build Qt target, shared or static.
- Build third-party media dependencies.
- Configure/build app.
- Stage runtime libraries/plugins/QML imports.
- Package IPK.

The important design rule: each phase gets explicit inputs and outputs. No phase
should quietly depend on a sibling `../build` path unless that path is a declared
argument.

### C4. Generate Static Qt QML Plugin Handling

The static Qt plugin list in CMake is fragile. Replace hand-maintained plugin
lists with scanner-derived data where possible.

Target:

- qmlimportscanner runs in build/package phase.
- Static plugin requirements are generated or validated.
- CI fails when a new QML import is missing from static builds.

### C5. Unify Native Build Scripts

Use `tools/lib/build-common.sh` for Linux, macOS, AppImage, and webOS where
possible.

Specific cleanups:

- `tools/build-linux-dev.sh` should align with `nix develop`, not old ad hoc
  assumptions.
- `tools/build-linux-release.sh` and `tools/build-macos.sh` should share mpv and
  CMake setup logic.
- `tools/package-appimage.sh` should stop downloading floating `continuous`
  artifacts without hashes.
- `tools/build-macos.sh` should fail if deployment is expected but `macdeployqt`
  or equivalent tooling is unavailable.

### C6. Keep Flake Inputs In Sync With Submodules

Nix builds must not silently drift from checked-out submodule revisions. Add a
tracked hook plus a CI check that verifies flake inputs and submodule commits are
consistent.

Target:

- Add a check script, for example `tools/check-submodule-flake-sync.sh`.
- Add a tracked hook, for example `tools/git-hooks/pre-commit`.
- Add an installer, for example `tools/install-git-hooks.sh`, that sets
  `core.hooksPath=tools/git-hooks`.
- The hook should inspect `.gitmodules`, `mpv`, `mpv_webos`, `flake.nix`, and
  `flake.lock`.
- The hook should fail if a submodule pointer changed without the matching flake
  update, or if the flake references a different mpv revision than the submodule.
- Add the same check to GitHub Actions so CI enforces it even when local hooks
  are not installed.

Implementation detail to decide:

- If Nix should source `mpv` and `mpv_webos` from local submodules during normal
  development, make that explicit in `flake.nix`.
- If Nix should fetch them from `github:sachk/mpv/<rev>`, then `flake.lock` must
  record the same commits as the git submodule pointers.

## Workstream D: GitHub Actions And Caching

### D1. Add Cheap Regular CI

Run on pull requests:

- `git diff --check`.
- Shell syntax checks for every `.sh` script.
- Python syntax checks for tool scripts.
- CMake configure for Linux.
- Nix eval/metadata checks.
- QML import scan if it can run without expensive target builds.

### D2. Linux AppImage Job

Keep or improve:

```sh
nix develop -c bash tools/build-linux-release.sh
nix develop -c bash tools/package-appimage.sh
```

Make all external AppImage tools pinned by manifest/checksum.

### D3. macOS Job

On macOS GitHub runner, verify both Nix and script paths:

```sh
nix run .
nix develop -c bash tools/build-macos.sh
nix develop -c bash tools/package-macos-dmg.sh
```

If `nix run .` launches a GUI and cannot be used headlessly, add a cheap
`--version`/`--diagnostics`/offscreen smoke command or a CTest target instead.

### D4. webOS Static Qt Cache Job

Create a manual workflow for building static Qt once per Qt version/toolchain:

- Inputs: Qt manifest, webOS SDK/toolchain archive, linkage mode.
- Output: compressed Qt host prefix and target prefix, or a cache entry.
- Cache/artifact key includes:
  - Qt version.
  - Qt manifest hash.
  - Patch hashes.
  - webOS SDK archive digest.
  - build script hash.
  - host runner OS.

Important: check GitHub cache size and retention limits before relying on
`actions/cache` alone. If static Qt is too large, use workflow artifacts,
GitHub Releases, or an external artifact store. The webOS IPK job should consume
this artifact/cache and only rebuild Qt when the key changes.

### D5. webOS IPK Job

The IPK job should:

- Restore or download the Qt prefix artifact/cache.
- Build app and bundled dependencies.
- Package IPK.
- Upload IPK artifact.
- Avoid TV deployment.

Run manually or nightly until costs are understood. Add a cheaper PR configure
job separately.

## Workstream E: Verification Matrix

### E1. Always Run Before Committing

```sh
git status --short
git diff --check
bash -n build-ipk.sh tools/*.sh tools/lib/*.sh tools/webos/*.sh tools/webos-native/*.sh
python3 -m py_compile tools/reduce_openapi.py tools/webos/bigalloc-report.py
```

After adding the submodule/flake sync check:

```sh
tools/check-submodule-flake-sync.sh
```

### E2. Linux Local Reproduction

```sh
nix develop -c bash tools/build-linux-release.sh
nix develop -c bash tools/package-appimage.sh
```

If a GUI smoke is needed and supported:

```sh
env JELLYFIN_NO_REBUILD=1 QT_QPA_PLATFORM=offscreen timeout 10s nix run .
```

### E3. webOS Static Qt From Scratch

Use a clean build/output directory, not local leftovers. The exact command names
should be finalized by the build refactor, but the end-to-end test should prove:

- Qt source fetch verifies checksums.
- Host Qt builds.
- Target static Qt builds.
- App configures against target Qt.
- Static QML imports/plugins are included.
- IPK packages.
- No TV install/deploy happens unless explicitly requested.

### E4. macOS

On a macOS runner or host:

```sh
nix run .
nix develop -c bash tools/build-macos.sh
nix develop -c bash tools/package-macos-dmg.sh
```

If `nix run .` cannot be headless, replace with a deterministic smoke target and
make CI call that target.

## Workstream F: Suggested Commit Sequence

1. `build: split mpv submodules by platform`
2. `build: add submodule flake sync check`
3. `build: add platform/source manifests`
4. `build: split webos qt build phases`
5. `ci: add static qt cache workflow`
6. `build: unify linux and macos build helpers`
7. `build: pin appimage tooling downloads`
8. `build: add version single source`
9. `build: use upstream qcoro`
10. `refactor(qml): add shared input key helper`
11. `refactor(qml): convert base controls to templates`
12. `refactor(nav): introduce stable item routes`

Do not combine the QCoro migration, QML control rewrite, and build script
decomposition in one commit. It will be too hard to bisect.

## Workstream G: Split mpv Submodules By Platform

The repo should have two mpv submodules:

- `mpv`: the normal non-webOS branch from `sachk/mpv`, with gpu-next/libmpv
  support for Linux, macOS, and future desktop/mobile platforms.
- `mpv_webos`: the webOS branch from `sachk/mpv`, containing the Starfish/webOS
  backend and webOS-specific patches.

Rationale:

- The webOS Starfish backend and desktop/mobile gpu-next libmpv path have
  different platform constraints.
- Keeping one submodule branch for every platform makes CI/cache behavior and
  branch hygiene harder.
- A split lets build scripts choose the correct media backend by target.

Implementation outline:

1. Confirm the exact branch names that should be used in `sachk/mpv`.
2. Update `.gitmodules` to keep `mpv` for the cross-platform branch.
3. Add `mpv_webos` as a second submodule pointing at the webOS branch.
4. Update CMake/build scripts so:
   - Linux/macOS/AppImage builds use `mpv`.
   - webOS IPK builds use `mpv_webos`.
5. Update `flake.nix` and `flake.lock` to know which mpv source each target
   consumes.
6. Add the submodule/flake sync check before committing the split.
7. Commit submodule pointer changes separately from build script rewrites.

Verification:

```sh
git submodule status --recursive
nix flake metadata
tools/check-submodule-flake-sync.sh
nix develop -c bash tools/build-linux-release.sh
nix develop -c bash tools/package-appimage.sh
```

For webOS, verify configure/build/package against `mpv_webos` without deploying
to the TV.

## Things To Avoid

- Do not start with a Kirigami migration for webOS.
- Do not redesign pages before fixing route identity.
- Do not add unpinned downloads.
- Do not make CI depend on paths outside the checkout.
- Do not remove the QCoro shim until every target has an upstream path.
- Do not deploy to the TV as part of build verification.
- Do not mix `mpv`/`mpv_webos` submodule pointer commits with unrelated QML,
  QCoro, or build-script rewrites.
- Do not let `flake.lock` point at different mpv revisions than the submodules.

## Success Criteria

This kickoff is successful when:

- Linux release build and AppImage build run through `nix develop`.
- macOS build/DMG workflow is reproducible on a macOS runner.
- webOS static Qt can be built once from manifest-pinned sources and restored by
  later webOS builds.
- CI has a cheap PR path plus manual/heavy artifact workflows.
- The app uses upstream QCoro or has a documented temporary fallback only for
  webOS.
- QML has a shared input helper and the first template-based controls without
  behavior regressions.
- The next runtime refactor can safely move details routes from `model + index`
  to stable `itemId`.
