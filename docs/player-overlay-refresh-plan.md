# Player Overlay Refresh — Implementation Plan

Scope: continue polishing `qml/pages/PlayerOverlayPage.qml` (player HUD, submenus,
trickplay, skip card) and pay down the structural debt that has accumulated in
this 1458-line file.

## Already done (in working tree, not yet committed)
- Removed the 3-stop horizontal gradient on the timeline progress fill → solid
  brand color (`#48D5FF` while buffering).
- Removed the stray purple (`#8A5CFF`) scrubber-thumb accent.
- Refreshed the subtitles/audio/settings menu and the audio-sync panel
  (rounded panels, hairline borders, uppercase section labels, accent-bar focus,
  Material `check` for the active track, cleaner step chips/±buttons).

The rest of this document is the remaining "do everything" work.

---

## 1. Adopt `Theme` + a scaling helper (foundation — do first)

**Why:** the overlay is the only major QML file that ignores the shared
`Theme`/`Metrics` singletons. It hardcodes **32 distinct hex literals** and
**98 `Math.round(N * uiScale)`** calls. Two concrete consequences:
- The accent-color preference (`Theme.accentIndex` → blue/purple/indigo,
  `qml/theme/Theme.qml:20`) never reaches the player — it stays blue.
- Every dimension is hand-scaled, which is noisy and error-prone.

**Changes to the page root (`FocusScope`):**
- Add `import "../theme"` (for `Theme`) and `import "../primitives"` (for
  `MaterialIcon`). Keep `import JellyfinWebOS` for the C++ `MpvVideoItem`.
- Add a scaling helper: `function dp(n) { return Math.round(n * uiScale) }`.
- Add a centralized palette as `readonly property color` on the root, so the
  extracted components can read `overlay.colX` and the whole overlay shares one
  source of truth:

  | property | value | replaces |
  |---|---|---|
  | `accent` | `Theme.accent` | `#00A4DC` (solid) |
  | `accentBright` | `Qt.lighter(Theme.accent, 1.35)` | `#EAF8FF`, `#5AD0F5`, `#48D5FF` focus/buffer |
  | `accentPurple` | `Theme.accentPurple` | `#AA5CC3` (negative audio delay) |
  | `colText` | `#F4F8FA` | title / primary over-video text |
  | `colTextDim` | `#B8C4CA` | secondary labels |
  | `colTextMuted` | `#8FA0A9` | uppercase section labels |
  | `colTextSubtle` | `#BCC6CB` | inactive list rows |
  | `colPanelBg` | `#F0121519` | panel backgrounds |
  | `colHairline` | `#26FFFFFF` | panel borders |
  | `colHairlineSoft` | `#1AFFFFFF` | dividers |
  | `colFillSubtle` | `#14FFFFFF` | inactive chips/buttons |

  > Over-video text stays slightly brighter than `Theme.textPrimary` (`#E8E8E8`)
  > on purpose — legibility over video differs from in-app surfaces. Panel
  > backgrounds stay translucent literals (Theme only has opaque surface
  > colors). Only the **accent** is unified to `Theme`.

- Translucent accent fills use `Qt.alpha(overlay.accent, 0.2)` instead of
  pre-baked ARGB hex (the accent is now dynamic).

**Mechanical pass:** replace `Math.round(N * overlay.uiScale)` and
`Math.round(N * uiScale)` → `dp(N)` everywhere (`dp` resolves unqualified inside
the page and inside Repeater delegates since they share the document scope).

---

## 2. Split the file into focused components

The page is an input/state machine **plus** five independent visuals. Extract
the visuals; keep all key-dispatch / state logic in the page. Each component
takes `property var overlay` and reads state through it (e.g.
`overlay.menuIndex`, `overlay.mode`, `overlay.player`). Each gets its own local
`uiScale`/`dp(n)`.

| New file (`qml/pages/`) | Extracted from | Notes |
|---|---|---|
| `PlayerTrickplayPreview.qml` | `trickplayPreview` item | Fully self-contained. Modernize frame: `radius dp(8)`, `border colHairline`. |
| `PlayerSkipSegmentCard.qml` | `skipSegmentCard` | Modernize: `Qt.alpha(accent, 0.9)` fill, `accentBright` border, `radius dp(12)`. |
| `PlayerAudioSyncPanel.qml` | `audioSyncPanel` | Instantiated with `id: audioSyncPanel` so the `states` `PropertyChanges` target still resolves. |
| `PlayerOverlayMenu.qml` | `menuPanel` | Instantiated with `id: menuPanel`. Expose `function positionAtTop()`. |

**Page wiring after extraction:**
- The three `open*()` functions call `menuList.positionViewAtBeginning()` today.
  Change to `menuPanel.positionAtTop()` (the only reach-in to extracted internals).
- `actionCenterX()` / `setMenuAnchor()` reference `hud` + `actionRow` geometry —
  the **HUD/timeline stays in the page**, so these keep working untouched.
- Preserve declaration order so z-order is unchanged: scrims → backButton →
  trickplay → skip card → hud → audioSyncPanel → menuPanel.

**Imports per component:** `QtQuick` (+ `QtQuick.Layouts` where used), plus
`"../theme"` (Theme) and `"../primitives"` (MaterialIcon) for those that need them.

**Build registration (required):** add all four files to the `QML_FILES` list in
`CMakeLists.txt` (alongside `qml/pages/PlayerOverlayPage.qml`, ~line 185).
Without this the module won't resolve the new types.

**Dead code to drop while here:**
- `previewSeek()` (a one-line alias for `seekBy()`).
- The two near-identical ± buttons in the audio-sync panel → one inline
  `component DelayButton: Rectangle { property int dir; ... }` (Qt 6.3+).

---

## 3. Visual / UX improvements

### 3a. Restore real scrim gradients (legibility)
`topScrim`/`bottomScrim` are currently `color: "transparent"` — they animate
opacity but render nothing, so the title/timeline float on raw video. Give them
vertical gradients (different beast from the slider gradient that was removed):
- `bottomScrim`: `transparent` (top) → `#CC000000` (bottom).
- `topScrim`: `#99000000` (top) → `transparent` (bottom).
Drop the now-unused `color` property. Opacity is still driven by the state machine.

### 3b. Active-segment marker on the timeline
The player exposes `activeSegmentEndSeconds` (`PlayerController.h:47`). Draw a
thin vertical tick on the track at `activeSegmentEndSeconds / durationSeconds`,
visible while `activeSegmentType.length > 0`, so the Skip Intro/Outro boundary is
visible on the scrubber.
> Limitation: only the *active* segment is exposed (no full chapter/segment
> list), so full chapter markers would need a new `PlayerController` property.

### 3c. Unify focus styling
The action row + back button still use the old ring (`#3300A4DC` fill + icy
`#EAF8FF` 2px border), which now clashes with the menus' accent-bar/fill style.
Re-tint to the accent: focused fill `Qt.alpha(accent, 0.22)`, border `accentBright`.
Keep the ring shape (correct for circular icon targets) — unify the *palette*, not the form.

### 3d. Panel entrance motion (elevation cue)
Add to the menu + audio-sync panels, matching the `ImageCard` idiom:
```qml
scale: opacity > 0.5 ? 1 : 0.97
Behavior on scale { enabled: !Theme.reducedMotion; NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
transform: Translate { y: opacity > 0.5 ? 0 : dp(14); Behavior on y { enabled: !Theme.reducedMotion; NumberAnimation { duration: 160; easing.type: Easing.OutCubic } } }
```

### 3e. Modernize stragglers
- Trickplay thumb frame: `radius dp(8)`, softer `colHairline` border (done as
  part of the extraction in §2).
- Skip-segment card: accent fill/border, `radius dp(12)` (also in §2).

### 3f. Empty states for track menus
When the subtitle/audio list is empty, show a muted placeholder row
("No subtitles available" / "No audio tracks") instead of a blank panel
(`menuList.count === 0`). Debug menu always has entries.

---

## 4. Deferred — needs a build-system decision

**True drop-shadow / blur elevation.** `QtQuick.Effects` (`MultiEffect`) is **not
currently linked** — the static webOS build only links the basic Quick/Controls2
plugins (`CMakeLists.txt:302-308`), and nothing in `qml/` imports the effects
module. Adding it requires `find_package(... Effects)` + linking the effects
QML plugin into the static target, which can't be verified without a full
webOS/AppImage build. §3d (entrance motion) is the safe stand-in. **Decision
needed** before wiring `MultiEffect`: confirm we're willing to add the effects
plugin to the static link list and run a target build.

---

## Suggested commit sequence (small, conventional commits per AGENTS.md)
1. `refactor(player): adopt Theme + dp() helper in overlay` (§1)
2. `refactor(player): extract trickplay/skip/audiosync/menu components` (§2 + CMake)
3. `feat(player): restore scrim gradients + active-segment marker` (§3a, §3b)
4. `style(player): unify focus styling and panel entrance motion` (§3c, §3d)
5. `feat(player): empty states for subtitle/audio menus` (§3f)

## Verification (no `qmllint`/effects build available locally)
- Brace/paren balance check on each file.
- Grep the page for stale references to extracted ids (`menuList`,
  `audioSyncPanel` internals, `thumbContainer`, etc.).
- Confirm `states` `PropertyChanges` targets (`menuPanel`, `audioSyncPanel`,
  `backButton`, `topScrim`, `bottomScrim`, `hud`) all still resolve.
- Confirm CMake `QML_FILES` lists all four new components.
- If a TV/AppImage build is authorized later, do a real launch to confirm focus
  navigation and submenu open/close are unchanged.

## Risk notes
- The component split is the most invasive step; do it one component at a time
  and keep the input/state machine in the page. The only page→component reach-in
  is `menuList.positionViewAtBeginning()` → `menuPanel.positionAtTop()`.
- Keyboard/remote navigation logic is **not** changed by any of this — all edits
  are visual/structural.
