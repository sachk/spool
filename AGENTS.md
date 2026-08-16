# AGENTS.md

## Project

- Spool for Jellyfin client for webOS and desktop.
- Prefer a smaller, faster, easier-to-extend codebase over compatibility scaffolding or speculative abstractions.
- The app is prerelease software: bump/reset caches on schema changes instead of adding migrations or fallback readers.
- `mpv/` is a submodule. Commit mpv changes inside `mpv/`, then commit its pointer here.
- Make small conventional commits as coherent work finishes. Never push unless the user explicitly asks.

## Module Seam

The goal is one core that several backends can sit on (spool-jellyfin,
spool-plex, spool-stremio). `CMakeLists.txt` groups sources into
`SPOOL_PLATFORM_SOURCES`, `SPOOL_PLAYER_SOURCES`, `SPOOL_SHELL_SOURCES` and
`SPOOL_JELLYFIN_SOURCES` to mark where that cut goes. They still build as one
library; the grouping exists so the split stays mechanical.

- Do not add a `src/api/` or `src/discovery/` include to the platform, player
  or shell groups. Four such dependencies already exist and are the work a
  split has to undo first: `PlayerController`, `PlaybackReporter`,
  `PlayQueueController` and `SettingsController` each take a
  `JellyfinApiFacade`, and `configurePlatformPlaybackCapabilities()` is
  handed one.
- `qml/primitives` and `qml/theme` reach exactly two singletons, `Art.url` and
  `Settings.uiScalePercent`. Keep it that way; page- and shell-level QML is
  where backend-shaped data belongs.
- Keep backend branding out of shared code. Settings copy, theme colour names
  and network user agents should read as the product, not as Jellyfin.

## Local Development

- Launch the native release app with `nix run`. Clean Git revisions substitute the immutable package from Cachix; dirty tracked changes use the incremental checkout build.
- Launch the already-built native release app without rebuilding with `nix run .#run`. It fails with a build-command hint when the binary is missing.
- Build without launching with `nix run .#build`.
- For a visible smoke test after a successful local build, use `timeout 10s nix run .#run`. Never wrap the default `nix run` in the launch timeout because it may substitute or build before launching.
- Use `nix develop .#native -c ...` for targeted development commands (e.g. `cmake --preset linux-dev`, then `cmake --build build/linux-dev/app --target jellyfin-native`).
- The image-diagnostics equivalents remain `nix run .#image-debug-build` followed by `nix run .#image-debug`.
- Batch coherent edits, then run one build and one `qmlformat`/`clang-format` invocation over all touched files; don't build or format file-by-file.

## webOS

- Do not build, install, launch, deploy, or test on a TV unless the user explicitly requests it. "Deploy" means install-only; never launch unless separately asked.
- For a requested deployment, use this exact build-and-install command from the repository root:
  `TV_HOST=root@tv.local; ./build-ipk.sh && nix develop -c bash tools/webos/verify-device.sh --no-launch --host "$TV_HOST" ./build/com.sachk.spool_0.3.0_arm.ipk`
  `build-ipk.sh` defaults to the complete fresh `all` pipeline. Never invoke the `app`, `stage`, or `package` phases separately.
- The packaged IPK is the only supported way to update the TV app. Never copy binaries or libraries to the TV manually, mutate the installed application tree, or substitute hand-written CMake/staging/install steps.
- If webOS diagnosis is requested, read current logs first and make one targeted change — no speculative deploy loops.
- Never write to TV partitions, patch package metadata, restart system services, or wipe app/user data.
