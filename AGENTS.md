# AGENTS.md

## Project

- This is the current Jellyfin Native / jellyfin-webOS app.
- The `mpv/` directory is a submodule; commit mpv-port changes inside `mpv/`, then commit the submodule pointer in this repo.
- Make small, separate commits as work is completed, using conventional commit style.
- Do not push unless the user explicitly asks for a push. Prefer local verification before spending GitHub Actions minutes.

## Environment

- Use `nix develop -c ...` from this repo root for normal build and packaging workflows.
- Use `nix-shell` only for one-off tools that are not already available through the dev shell.
- For Linux CI/AppImage work, reproduce the workflow locally first:
  - `nix develop -c bash tools/build-linux-release.sh`
  - `nix develop -c bash tools/package-appimage.sh`
- For script changes, always run relevant syntax checks, for example:
  - `bash -n tools/build-linux-release.sh tools/package-appimage.sh tools/build-macos.sh tools/package-macos-dmg.sh`
- Use `git diff --check` before committing.

## webOS TV

- Primary TV target: `root@192.168.0.200`
- Always use `ssh -tt` when running `luna-send` or other LS2 CLI tools on the TV. Without a forced TTY, those commands can hang or produce no output.
- Do not write directly to TV partitions or runtime state over SSH.
- Do not edit files, remove files/directories, patch opkg metadata, modify `/tmp/appinstalld`, or restart system services manually.
- Do not wipe app/user data as a cleanup shortcut. Removing data that forces login/setup again is disrespectful and wastes time; if the TV is low on space, assume logs/generated diagnostics are the problem first and clean only app-specific logs or generated diagnostics unless the user explicitly approves more.
- Use supported webOS CLIs/services only (`ares-install`, `ares-launch`, `ApplicationInstallerUtility`, `luna-send`) and stop if those cannot do the operation.
- Reading logs over SSH is allowed. Prefer current native app logs:
  - `/tmp/com.codex.jellyfinnative-mpv.log`
  - `/tmp/com.sachk.tern.log`

## Build And Deploy

- Do not run `./webos-mpv-demo-cycle.sh` unless the user explicitly asks for that old demo workflow.
- Do not use the old demo cycle as a default compile check.
- Do not run expensive TV/webOS compile or deploy loops unless the user asks for that verification.
- For runtime webOS bugs, inspect logs first, then make targeted source changes; avoid speculative rebuild/deploy loops.
