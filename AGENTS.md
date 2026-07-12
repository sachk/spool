# AGENTS.md

## Project

- This is the current Jellyfin Native client for webOS and desktop, with future mobile and TV targets.
- Prefer a smaller, faster, easier-to-extend codebase over compatibility scaffolding or speculative abstractions.
- The app is unreleased. Cache/schema changes use a clean cutover: bump or reset the cache instead of adding legacy migrations or fallback readers.
- The `mpv/` directory is a submodule. Commit mpv changes inside `mpv/`, then commit its pointer here.
- Make small conventional commits as coherent work finishes. Never push unless the user explicitly asks.

## Local Development

- Use `nix develop .#native -c ...` from the repository root.
- Configure and build the native app with:
  - `nix develop .#native -c cmake --preset linux-dev`
  - `nix develop .#native -c cmake --build build/linux-dev/app --target jellyfin-native`
- Foreground-test the real local app with `timeout 10s nix run`. Do not add a smoke-only exit path; a visible launch catches GUI and runtime failures.
- During large refactors, batch coherent edits before rebuilding. Prefer a focused target or touched test over repeated project-wide verification.
- Run touched C++ tests with `nix develop .#native -c ctest --test-dir <build-dir> -R '<tests>' --output-on-failure`.
- Before committing C++/QML, format touched files with `clang-format`/`qmlformat`, then run `git diff --check`.
- For Linux release packaging use:
  - `nix develop .#native -c bash tools/build-linux-release.sh`
  - `nix develop .#native -c bash tools/package-appimage.sh`
- Syntax-check changed shell scripts with `bash -n`.
- Use `nix-shell` only for a one-off tool absent from the dev shell.

## webOS

- Do not build, install, launch, deploy, or test on a TV unless the user explicitly requests the final webOS pass.
- Treat “deploy” as install-only. Never launch or relaunch the app unless the user separately asks to launch it.
- For every requested deployment, create a fresh package from current sources in this order:
  - `nix develop -c bash build-ipk.sh app`
  - `nix develop -c bash build-ipk.sh stage`
  - `nix develop -c bash build-ipk.sh package`
  - Verify that the executable inside the IPK exactly matches `build/webos/stage/app/bin/jellyfin-native`.
  - Install with `nix develop -c bash tools/webos/verify-device.sh --no-launch <ipk>`.
- After installation, terminate any stale `jellyfin-native` process without launching a replacement, then verify that the installed executable SHA-256 matches the packaged executable.
- Do not use `ares-*`, `luna-send`, ApplicationInstallerUtility, or TV SSH as routine development checks.
- Never run `webos-mpv-demo-cycle.sh` unless explicitly requested.
- If webOS diagnosis is requested, read current logs first and make one targeted source change. Do not run speculative deploy loops.
- Never write to TV partitions, patch package metadata, modify installer internals, restart system services, or wipe app/user data.
