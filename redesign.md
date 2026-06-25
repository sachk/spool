# Login redesign — differences from `sachk/qtfin`

A running log of how the design system / mocks **diverge from the current
`sachk/qtfin` source**, for the coding agent that will implement them in
Qt/QML. Everything not listed here matches the repo (tokens, components and the
other UI-kit screens are faithful recreations).

Reference mock: `ui_kits/jellyfin-tv/LoginRedesign.html`.
Source being replaced: `qml/pages/LoginPage.qml` (+ login flow in
`src/app/AppController.cpp`, `QuickConnectController`, `SessionController`).

---

## 1. Login becomes two distinct screens (was one)

The repo has a single `LoginPage.qml` (server picker + credentials + "Login to
this server next time" toggle, side by side). Replace with:

### A. Profile select — launch screen (NEW)
- Shown on app launch when ≥1 saved account/server pair exists **and** there is
  no default (a default skips straight to sign-in — see §4).
- Netflix-style: account/server **pairs as tiles, left-to-right in order of
  addition**, then a trailing **`+` Add** tile.
- Tile = large square avatar (user initial on a per-pair deterministic muted
  tint), **username** below, then **server name** and **server address**
  underneath. The default pair shows a small `star` DEFAULT badge.
- TV-first: D-pad Left/Right moves focus, OK selects, focus = 3px accent ring +
  scale-up + accent-purple under-bar. Selecting a tile → straight to that
  account's password entry (or auto-auth if a token is stored). Selecting `+` →
  the Add screen.

### B. Add account — sequential two-step screen (REPLACES old form)
One decision on screen at a time; **the two steps never render together** (the
old page showed server + account simultaneously).
- **Step 1 — Server.** `URL` field at the **top**, then the list of detected
  servers below it. Typing a URL and pressing OK/Enter **adds a row to the list
  in a `Checking` state** (no separate "Add" button — see §3); once it confirms
  online the row flips to `Online` and is selectable. Selecting any
  **online** server advances to step 2. (Maps to the repo's
  `discoveredServers` model + `setServerUrl` / `chooseDiscoveredServer`, plus a
  new reachability probe for typed URLs.)
- **Step 2 — Account.** A compact chosen-server bar (OK on it returns to step 1
  to change server), then Username + Password, then **Sign In** + **Quick
  Connect**.

---

## 2. "Login to this server next time" toggle — REMOVED
The inline remember-me `ToggleRow` in `LoginPage.qml` is gone. A pair becomes
the default **only** via:
- the post-login **make-default prompt** (see §4), or
- a setting in `SettingsPage.qml`.

Drop `appController.loginSameServer` / `setLoginSameServer` from the login UI;
keep/relocate the persistence behind the prompt + settings.

---

## 3. No "Add" button — OK/Enter on the URL field submits
TV-remote step count drove this. Adding a server by URL should be: focus URL →
OK (keyboard) → type → OK/Enter to submit. A separate "Add" button adds an
extra focus-move + OK. So: **submitting the field adds the server**; the new
row appears in the list in `Checking`, then `Online`.

Common-case path (90% — one detected server): on entering step 1, **default
focus is the first detected server**, so it's a single OK to choose it.

---

## 4. Default set via prompt on the 1st and 3rd successful login
After a successful sign-in, show a small modal: *"Sign in here automatically?"*
with the `user · server` pair, **Yes / Not now**.
- Triggered on the user's **1st and 3rd** successful logins for that
  install/pair. The **3rd** is a deliberate second chance for users who
  dismissed the 1st by accident.
- "Yes" marks that pair default (and stores its token for silent re-auth).
- Also settable later in `SettingsPage.qml`.

Implementation: track a per-install login counter (e.g. in `SettingsController`
/ SQLite) and a `defaultProfileId`; launch flow checks `defaultProfileId` before
showing Profile Select.

---

## 5. Copy stripped back (intention over instruction)
Removed all explanatory/instructional microcopy. Specifically gone:
- "Type a URL and Add — once it confirms online, select it to continue."
- "No 'remember me' here — you'll be asked to set a default after you sign in."
- Quick Connect's "Enter this code on another device" caption.
- On-screen nav hints ("← → to move · Enter to select").
- Field label shortened: "Add a server by URL" → **`URL`**.

Apply the same restraint elsewhere: labels are nouns/commands, not sentences;
status words only where functional (`Checking`, `Online`). The login subtitle
string `login.subtitle` ("Native Qt 6.11 client for LG webOS") is no longer
shown on these screens.

---

## 6. Visual: no gradients on buttons; quieter default marker; icon-only Back; "Auk" name
- **Primary button** no longer uses the blue→purple gradient (`ActionButton`
  `kind="primary"` in the repo). It is now a **restrained solid deep-accent
  fill (`--accent-dim`) with a light label**, brightening to `--accent` on
  hover; focus ring is white (`--text-primary`). The blue→purple gradient is
  **kept for the logo / brand mark only**. System-wide change
  (`components/core/Button.jsx`), applies to every primary button, not just login.
- **Product name** in user-facing chrome is **"Auk"** (was "Jellyfin"). The
  login logo mark is now a user-fillable placeholder (`<image-slot>`), not the
  gradient square — drop in the real Auk logo.
- **Launch screen** has no "Who's watching?" heading — just the profile tiles.
- **Default account marker** on profile tiles is just a **gold star
  (`#F6C544`) top-right** — the "DEFAULT" pill/label was removed.
- **Back control** on the Add screen is an **icon-only circular button** (no
  "Profiles" label text).

## Open questions (not yet decided)
- Default pair on launch: fully silent auto-sign-in vs. Profile Select with the
  default pre-focused. (Mock currently still shows the picker.)
- Remove/edit affordance on profile tiles for managing multiple accounts.
- Whether to retire `LoginScreen.jsx` / the old `index.html` login once this is
  approved.
