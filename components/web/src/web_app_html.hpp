#pragma once

// =============================================================================
// web (internal) -- the single embedded page served at GET / (and,
// with a secret word configured, at GET /<secret_word>/ -- see
// web_json_helpers.hpp's build_prefixed_path()).
//
// A plain single-page app: no framework, no build step, just one HTML
// string with inline CSS/JS, calling the REST endpoints
// web_vault_routes.cpp exposes (GET /api/v1/entries, GET/PUT/DELETE
// /api/v1/entry?id=N, POST /api/v1/entry), web_settings_routes.cpp
// exposes (GET /api/v1/settings, PUT /api/v1/settings/<section>), and
// web_service.cpp's own auth endpoints. All fetch() paths here are
// RELATIVE (e.g. 'api/v1/entries', not '/api/v1/entries') so they
// resolve correctly whether or not a secret-word prefix is active --
// see web_service.cpp's own login page for why that matters.
//
// Flow: on load, checks GET api/v1/auth/status. If not authenticated,
// shows the PIN login form (same as before); on success, switches to
// the vault view in place, no page reload. From there: a flat list of
// entries -> tap one to view its fields (password/TOTP secret masked
// behind a reveal toggle) -> Edit opens the same form used for
// creating a new entry. Delete requires a second confirmation tap,
// matching the on-device UI's own press-twice pattern. A Settings
// button opens General/USB/Security/WiFi sections, each with its own
// Save button (matches web_settings_routes.hpp's per-section PUT
// shape) -- see that file's own comment for which fields are
// deliberately NOT here (PIN length/change, secret word, Factory
// Reset).
//
// NOT here: search, backup/restore over the web (see
// components/web/README.md's "Explicitly NOT here yet" list -- still
// accurate).
// =============================================================================

namespace web {

constexpr char APP_PAGE[] = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KeyKeeper2</title>
<style>
  :root { color-scheme: light dark; }
  * { box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    max-width: 480px; margin: 0 auto; padding: 16px;
    color: #1a1a1a; background: #fafafa;
  }
  h2 { margin: 8px 0 16px; }
  input, select, textarea, button {
    width: 100%; padding: 10px; margin: 6px 0; font-size: 1rem;
    box-sizing: border-box; border: 1px solid #ccc; border-radius: 6px;
    font-family: inherit;
  }
  button {
    background: #2563eb; color: #fff; border: none; cursor: pointer; font-weight: 600;
  }
  button:active { background: #1d4ed8; }
  button.secondary { background: #6b7280; }
  button.danger { background: #dc2626; }
  button.small { width: auto; padding: 6px 12px; font-size: 0.85rem; margin: 0; }
  .row { display: flex; gap: 8px; align-items: center; }
  .row > * { flex: 1; }
  #msg { min-height: 1.4em; font-size: 0.9rem; margin: 4px 0; }
  .topbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
  .topbar h2 { margin: 0; }
  .entry-item {
    background: #fff; border: 1px solid #e5e7eb; border-radius: 8px;
    padding: 12px; margin: 8px 0; cursor: pointer;
  }
  .entry-item .login { font-weight: 600; }
  .entry-item .meta { color: #6b7280; font-size: 0.85rem; margin-top: 2px; }
  .field-label { color: #6b7280; font-size: 0.8rem; margin-top: 10px; }
  .field-value { font-size: 1rem; word-break: break-all; display: flex; align-items: center; gap: 8px; }
  .field-value span { flex: 1; }
  .empty { color: #9ca3af; text-align: center; padding: 24px 0; }
  .hidden { display: none; }
  label.checkbox { display: flex; align-items: center; gap: 8px; font-size: 0.95rem; }
  label.checkbox input { width: auto; margin: 0; }
  .help-section { margin-bottom: 22px; }
  .help-section h3 { margin: 0 0 6px; }
  .help-section p, .help-section li { font-size: 0.92rem; line-height: 1.5; color: #374151; }
  .help-section ul { margin: 4px 0; padding-left: 20px; }
  kbd {
    display: inline-block; padding: 1px 7px; border-radius: 4px; font-size: 0.85em;
    background: #e5e7eb; border: 1px solid #d1d5db; font-family: inherit;
  }
  .note { background: #fffbeb; border: 1px solid #fde68a; border-radius: 6px; padding: 10px 12px; font-size: 0.88rem; }
  .recovery-code-row {
    display: flex; align-items: center; gap: 8px; padding: 6px 0;
    border-bottom: 1px solid #f0f0f0; font-size: 0.95rem;
  }
  .recovery-code-row .code { font-family: ui-monospace, Consolas, monospace; flex: 1; }
  .recovery-code-row.used .code { text-decoration: line-through; color: #9ca3af; }
  .recovery-code-row button { width: auto; padding: 3px 10px; font-size: 0.78rem; margin: 0; }
  .backup-row {
    display: flex; align-items: center; gap: 8px; padding: 8px 0;
    border-bottom: 1px solid #f0f0f0; font-size: 0.92rem;
  }
  .backup-row .name { font-family: ui-monospace, Consolas, monospace; flex: 1; }
  .backup-row .size { color: #6b7280; font-size: 0.85rem; }
  .backup-row button { width: auto; padding: 4px 10px; font-size: 0.8rem; margin: 0; }
</style>
</head>
<body>

<div id="login-view">
  <h2>KeyKeeper2</h2>
  <p id="login-msg"></p>
  <input id="pin" type="password" inputmode="numeric" placeholder="PIN" autofocus>
  <button onclick="login()">Unlock</button>
</div>

<div id="app-view" class="hidden">

  <div id="list-view">
    <div class="topbar" style="flex-direction:column; align-items:stretch; gap:8px">
      <div style="display:flex; gap:8px; justify-content:flex-start; flex-wrap:wrap">
        <button class="small secondary" onclick="showView('help-view')">Help</button>
        <button class="small secondary" onclick="openBackup()">Backup</button>
        <button class="small secondary" onclick="openSettings()">Settings</button>
        <button class="small" onclick="openEdit(null)">+ New</button>
      </div>
      <h2>Accounts</h2>
    </div>
    <input id="search-box" type="text" placeholder="Search login, URL, notes..." oninput="onSearchInput()"
      style="margin-bottom:10px">
    <div id="msg"></div>
    <div id="entry-list"></div>
  </div>

  <div id="detail-view" class="hidden">
    <div class="topbar">
      <button class="small secondary" onclick="showView('list-view'); loadList();">&larr; Back</button>
      <span></span>
    </div>
    <div id="detail-fields"></div>

    <div id="recovery-codes-section" style="margin-top:20px">
      <div class="topbar" style="margin-bottom:6px">
        <h3 style="margin:0; font-size:1rem">Recovery Codes</h3>
      </div>
      <div id="recovery-codes-list"></div>
      <div id="recovery-codes-edit" style="display:none">
        <textarea id="recovery-codes-input" rows="4"
          placeholder="Paste the codes this service gave you, one per line"
          style="width:100%; font-family:ui-monospace,Consolas,monospace; font-size:0.9rem"></textarea>
        <div class="row" style="margin-top:6px">
          <button class="small secondary" onclick="document.getElementById('recovery-codes-file').click()">Import from file</button>
          <input type="file" id="recovery-codes-file" accept=".txt" style="display:none" onchange="handleRecoveryCodesFile(event)">
        </div>
        <div class="row" style="margin-top:6px">
          <button class="small" onclick="saveRecoveryCodes()">Save</button>
          <button class="small secondary" onclick="cancelEditRecoveryCodes()">Cancel</button>
        </div>
      </div>
      <div class="row" id="recovery-codes-actions" style="margin-top:6px">
        <button class="small secondary" id="recovery-set-btn" onclick="startEditRecoveryCodes()">Set / Replace</button>
        <button class="small secondary" id="recovery-copy-btn" onclick="copyRecoveryCodes()" style="display:none">Copy unused</button>
      </div>
      <div class="note" style="margin-top:8px">These come FROM the service the account belongs to (its own 2FA or
        account-recovery settings page) -- paste or import the ones it gave you. This device has no way to create
        codes that service would actually accept.</div>
      <div id="recovery-msg" style="font-size:0.85rem; margin-top:4px"></div>
    </div>

    <div id="seed-phrase-section" style="margin-top:20px">
      <div class="topbar" style="margin-bottom:6px">
        <h3 style="margin:0; font-size:1rem">Seed Phrase</h3>
        <button class="small secondary" id="seed-reveal-btn" onclick="toggleSeedPhraseReveal()" style="display:none">Reveal</button>
      </div>
      <div id="seed-phrase-view"></div>
      <div id="seed-phrase-edit" style="display:none">
        <textarea id="seed-phrase-input" rows="3"
          placeholder="12, 15, 18, 21 or 24 words, space or newline separated"
          style="width:100%; font-family:ui-monospace,Consolas,monospace; font-size:0.9rem"></textarea>
        <div class="row" style="margin-top:6px">
          <button class="small secondary" onclick="document.getElementById('seed-phrase-file').click()">Import from file</button>
          <input type="file" id="seed-phrase-file" accept=".txt" style="display:none" onchange="handleSeedPhraseFile(event)">
        </div>
        <div class="row" style="margin-top:6px">
          <button class="small" onclick="saveSeedPhrase()">Save</button>
          <button class="small secondary" onclick="cancelEditSeedPhrase()">Cancel</button>
        </div>
      </div>
      <div class="row" id="seed-phrase-actions" style="margin-top:6px">
        <button class="small secondary" id="seed-edit-btn" onclick="startEditSeedPhrase()">Set / Replace</button>
        <button class="small secondary" id="seed-copy-btn" onclick="copySeedPhrase()" style="display:none">Copy</button>
        <button class="small danger" id="seed-clear-btn" onclick="clearSeedPhrase()" style="display:none">Clear</button>
      </div>
      <div class="note" style="margin-top:8px">Anyone who has this phrase has full, irreversible control of the
        wallet it belongs to -- treat it with at least the same care as the wallet itself. This device encrypts
        its own storage, but a Backup or CSV export of it is not (see Help).</div>
      <div id="seed-msg" style="font-size:0.85rem; margin-top:4px"></div>
    </div>

    <div class="row" style="margin-top:16px">
      <button onclick="openEdit(currentEntryId)">Edit</button>
      <button class="danger" id="delete-btn" onclick="confirmDelete()">Delete</button>
    </div>
  </div>

  <div id="edit-view" class="hidden">
    <div class="topbar">
      <button class="small secondary" onclick="cancelEdit()">&larr; Cancel</button>
      <span></span>
    </div>
    <div class="field-label">Login</div>
    <input id="edit-login" type="text">
    <div class="field-label">Password</div>
    <input id="edit-password" type="text">
    <div class="field-label">URL</div>
    <input id="edit-url" type="text">
    <div class="field-label">Notes</div>
    <textarea id="edit-notes" rows="3"></textarea>
    <div class="field-label">TOTP Secret</div>
    <input id="edit-totp" type="text">
    <div class="field-label">Category</div>
    <input id="edit-category" type="text">
    <label class="checkbox" style="margin-top:12px">
      <input id="edit-favorite" type="checkbox">
      Favorite
    </label>
    <div id="edit-msg"></div>
    <button onclick="saveEntry()" style="margin-top:12px">Save</button>
  </div>

  <div id="settings-view" class="hidden">
    <div class="topbar">
      <button class="small secondary" onclick="showView('list-view')">&larr; Back</button>
      <span></span>
    </div>
    <h2>Settings</h2>

    <h3>General</h3>
    <div class="field-label">Language</div>
    <select id="set-language">
      <option value="english">English</option>
      <option value="russian">Russian</option>
    </select>
    <div class="field-label">Display Brightness (0-100)</div>
    <input id="set-brightness" type="number" min="0" max="100">
    <div class="field-label">Screen Timeout (seconds, 0 = off)</div>
    <input id="set-screen-timeout" type="number" min="0">
    <div id="set-general-msg"></div>
    <button onclick="saveSettings('general')" style="margin-top:8px">Save General</button>

    <h3 style="margin-top:24px">USB</h3>
    <div class="field-label">Quick Password (no PIN required)</div>
    <input id="set-default-password" type="text">
    <label class="checkbox" style="margin-top:12px">
      <input id="set-cyrillic-auto-switch" type="checkbox">
      Auto-switch keyboard layout for Cyrillic (best-effort, Alt+Shift)
    </label>
    <div id="set-usb-msg"></div>
    <button onclick="saveSettings('usb')" style="margin-top:8px">Save USB</button>

    <h3 style="margin-top:24px">Security</h3>
    <label class="checkbox">
      <input id="set-auto-lock-enabled" type="checkbox">
      Auto Lock enabled
    </label>
    <div class="field-label">Auto Lock timeout</div>
    <select id="set-auto-lock-timeout">
      <option value="0">Off</option>
      <option value="60">1 min</option>
      <option value="180">3 min</option>
      <option value="300">5 min</option>
      <option value="600">10 min</option>
      <option value="900">15 min</option>
      <option value="1200">20 min</option>
      <option value="1500">25 min</option>
      <option value="1800">30 min</option>
      <option value="2100">35 min</option>
      <option value="2400">40 min</option>
      <option value="2700">45 min</option>
      <option value="3000">50 min</option>
      <option value="3300">55 min</option>
      <option value="3600">60 min</option>
      <option value="3900">65 min</option>
      <option value="4200">70 min</option>
      <option value="4500">75 min</option>
      <option value="4800">80 min</option>
      <option value="5100">85 min</option>
      <option value="5400">90 min</option>
      <option value="5700">95 min</option>
      <option value="6000">100 min</option>
    </select>
    <div id="set-security-msg"></div>
    <button onclick="saveSettings('security')" style="margin-top:8px">Save Security</button>

    <h3 style="margin-top:24px">WiFi</h3>
    <div class="field-label">Mode</div>
    <select id="set-wifi-mode">
      <option value="disabled">Disabled</option>
      <option value="station">Station</option>
      <option value="access_point">Access Point</option>
    </select>
    <div class="field-label">Station SSID</div>
    <input id="set-sta-ssid" type="text">
    <div class="field-label">Station Password</div>
    <input id="set-sta-password" type="text">
    <div class="field-label">AP SSID</div>
    <input id="set-ap-ssid" type="text">
    <div class="field-label">AP Password</div>
    <input id="set-ap-password" type="text">
    <label class="checkbox" style="margin-top:12px">
      <input id="set-captive-portal" type="checkbox">
      Captive Portal (Access Point mode only)
    </label>
    <div id="set-wifi-msg"></div>
    <button onclick="saveSettings('wifi')" style="margin-top:8px">Save WiFi</button>
  </div>

  <div id="backup-view" class="hidden">
    <div class="topbar">
      <h2>Backup</h2>
      <button class="small secondary" onclick="showView('list-view'); loadList();">&larr; Back</button>
    </div>
    <p style="font-size:0.88rem; color:#6b7280">A full, exact copy of the device's own internal vault file, on the
      microSD card. Only ever readable by another KeyKeeper2 device, not other password managers or spreadsheet
      apps -- for that, use Export Vault (CSV) on the device itself instead, though that CSV export leaves out
      recovery codes and seed phrases (only login/password/url/notes/TOTP secret/category/favorite) -- a Backup
      here is the only copy that includes everything.</p>
    <button onclick="createBackup()">Create Backup</button>
    <div id="backup-list" style="margin-top:16px"></div>
    <div id="backup-msg" style="font-size:0.85rem; margin-top:8px"></div>
  </div>

  <div id="help-view" class="hidden">
    <div class="topbar">
      <h2>Help</h2>
      <div style="display:flex; gap:8px">
        <button class="small secondary" onclick="setHelpLang('en')">EN</button>
        <button class="small secondary" onclick="setHelpLang('ru')">RU</button>
        <button class="small secondary" onclick="showView('list-view')">&larr; Back</button>
      </div>
    </div>

    <div id="help-content-en">
    <div class="help-section">
      <h3>Device controls</h3>
      <p>The device has one rotary knob (turn / press) and one BACK button, each read as a short or long press:</p>
      <ul>
        <li><kbd>Rotate</kbd> &mdash; move the selection, or spin the current character while typing</li>
        <li><kbd>OK</kbd> (short) &mdash; select / confirm a character / enter a menu</li>
        <li><kbd>Hold OK</kbd> &mdash; finish typing a field, or a "hold" action shown in the on-device footer</li>
        <li><kbd>BACK</kbd> (short) &mdash; go back one screen, or erase the last typed character</li>
        <li><kbd>Hold BACK</kbd> &mdash; while typing, switches between character sets (lowercase, UPPERCASE,
          digits, symbols, кириллица строчная/ПРОПИСНАЯ)</li>
      </ul>
    </div>

    <div class="help-section">
      <h3>Accounts</h3>
      <p>Each entry has Login, Password, URL, Notes, TOTP Secret, Category and a Favorite flag. On the device: hold
        <kbd>OK</kbd> on the account list to create a new entry; open an existing one to view, edit, delete, reveal
        the password, or print a field over USB. This web page can do the same over the network &mdash; open an
        entry to view or edit it, or use <strong>+ New</strong> above.</p>
      <p>Cyrillic text is supported in every field, both typing it on the device and displaying it back &mdash;
        the on-device font and character-set switch (see Device controls above) both handle it.</p>
    </div>

    <div class="help-section">
      <h3>Password generator</h3>
      <p>Settings &rarr; Password Gen sets the length and which character classes to use. From there,
        <strong>Generate &amp; Type</strong> creates a fresh password and types it over USB immediately &mdash;
        useful for a signup form on whatever computer the device is plugged into, without creating an account entry
        at all. Inside an account's Password field, holding <kbd>OK</kbd> generates a new password for that entry
        directly.</p>
    </div>

    <div class="help-section">
      <h3>Recovery codes</h3>
      <p>For the codes a service gives you as a backup way in if you lose access otherwise (GitHub's 2FA recovery
        codes, a bank's, an exchange's, ...). This device does <strong>not</strong> generate these &mdash; only the
        service itself can create codes it will actually accept back. On an entry's page, use
        <strong>Set / Replace</strong> to paste the codes the service gave you (one per line), or
        <strong>Import from file</strong> to load them from a .txt file the service let you download. Mark each one
        used as you spend it; <strong>Copy unused</strong> copies whatever's left to your clipboard.</p>
      <p>On the device itself this is view-and-print only &mdash; entering a whole set via the rotary knob isn't
        practical, so setting or replacing the codes is web-only.</p>
    </div>

    <div class="help-section">
      <h3>Seed phrase</h3>
      <p>For a crypto wallet's own recovery phrase (BIP-39 &mdash; 12, 15, 18, 21 or 24 words). Same idea as recovery
        codes: the wallet software generates this, not this device &mdash; paste or import the exact phrase the
        wallet gave you, and the device checks every word against the real BIP-39 wordlist before accepting it.
        Shown masked by default (like the password field) with a <strong>Reveal</strong> toggle, since this is more
        sensitive than almost anything else stored here &mdash; whoever has it has full, irreversible control of the
        wallet.</p>
      <p>Same as recovery codes, only one phrase per entry, and setting or replacing it is web-only; the device can
        view it (masked, <kbd>OK</kbd> toggles reveal) and print it over USB as one space-separated line, matching
        how wallet software's own "paste your phrase" fields expect it.</p>
    </div>

    <div class="help-section">
      <h3>TOTP (2FA) codes</h3>
      <p>Paste the Base32 secret (the same string a "manual entry" QR code setup gives you, e.g.
        <code>JBSWY3DPEHPK3PXP</code>) into an entry's TOTP Secret field. The device then shows a live, auto-refreshing
        6-digit code on that entry's screen, and Print OTP types the current code over USB.</p>
      <div class="note">This board has no battery-backed clock chip &mdash; the only time source is NTP over WiFi.
        TOTP codes are only available once WiFi (Station mode) has connected and synced time at least once since the
        device was last powered on; a full power loss resets that until the device reconnects again.</div>
    </div>

    <div class="help-section">
      <h3>USB typing</h3>
      <p>Printing a field types it as if from a USB keyboard into whatever computer the device is plugged into.
        Settings &rarr; USB controls the delays between keystrokes and the print order, plus a Quick Password
        shortcut reachable from the lock screen without unlocking (a deliberate convenience/security trade-off).</p>
      <p>Typing Cyrillic text this way needs the receiving computer to actually have a Russian keyboard layout
        available. Settings &rarr; USB &rarr; <strong>Auto-switch layout</strong> (off by default) makes the device
        send Alt+Shift to try switching for you before and after each run of Cyrillic characters &mdash; best-effort,
        since the device can't know what's configured on the computer it's plugged into. With it off, switch the
        layout yourself on the computer before printing.</p>
    </div>

    <div class="help-section">
      <h3>Backup, export &amp; import</h3>
      <p>Two different things, both on a microSD card, both under Backup on the device:</p>
      <ul>
        <li><strong>Create/Restore Backup</strong> &mdash; a full, exact copy of the device's own internal vault
          file. Only ever readable by another KeyKeeper2 device, not other apps.</li>
        <li><strong>Export/Import Vault</strong> &mdash; a plain CSV file, readable by (or editable in) most other
          password managers and spreadsheet apps. Import adds entries rather than replacing what's already there,
          and is lenient about column names (<code>username</code>, <code>site</code>, <code>note</code>, etc. are
          all recognized, not just this device's own exact header names). Only the basic fields
          (login/password/url/notes/TOTP secret/category/favorite) round-trip through CSV -- recovery codes and
          seed phrases are NOT included, by design (CSV is meant to move between different password managers, and
          most others have nowhere to put those anyway). Create/Restore Backup above is the only copy that
          includes everything.</li>
      </ul>
      <p>The device also has a Format SD Card action, useful if a card was shipped pre-formatted as exFAT &mdash;
        common on 32GB+ cards &mdash; which this device can't read.</p>
    </div>

    <div class="help-section">
      <h3>Security</h3>
      <ul>
        <li>Repeated wrong PIN attempts escalate: a temporary lockout, then eventually a full wipe of the vault
          and PIN. This is deliberate and cannot be turned off.</li>
        <li>A separate Duress PIN can be set up, which unlocks normally but silently wipes the vault first &mdash;
          for being made to unlock the device under pressure.</li>
        <li>Auto-Lock (Settings &rarr; Security) locks the device after a period of inactivity you choose.</li>
        <li>Factory Reset erases everything: vault, PIN, all settings, WiFi credentials.</li>
        <li>The vault file on the device itself is encrypted (AES-256-GCM, keyed from the PIN) -- but a Backup or
          CSV export of it is <strong>not</strong>, by design (a Backup needs to work from any KeyKeeper2 device's
          own PIN, and CSV is meant to move between different password managers). Treat a backup or export file
          with the same care as the passwords it contains.</li>
      </ul>
    </div>

    <div class="help-section">
      <h3>WiFi &amp; this web page</h3>
      <p>Settings &rarr; WiFi switches between Station (join an existing network, needed for TOTP's time sync) and
        Access Point (the device hosts its own network). This page is served either way, at the device's IP address
        &mdash; and, if a Secret Word is set (also under WiFi settings), only at <code>/&lt;secret word&gt;/</code>
        rather than the bare address, as a lightweight extra layer against someone stumbling onto it by guessing the
        IP.</p>
    </div>

    <div class="help-section">
      <h3>Known limitations</h3>
      <ul>
        <li>The on-device interface's own translation (General settings &rarr; Language) is a work in progress --
          some screens are already in Russian when selected, others are still English-only regardless of the
          setting.</li>
        <li>Search and Backup/Restore aren't available from this web page yet, only on the device itself.</li>
      </ul>
    </div>
    </div>

    <div id="help-content-ru" style="display:none">
    <div class="help-section">
      <h3>Управление устройством</h3>
      <p>У устройства один поворотный энкодер (вращение / нажатие) и одна кнопка BACK, у каждой есть короткое и
        долгое нажатие:</p>
      <ul>
        <li><kbd>Поворот</kbd> &mdash; перемещение выбора, либо прокрутка текущего символа при вводе текста</li>
        <li><kbd>OK</kbd> (коротко) &mdash; выбрать / подтвердить символ / открыть пункт меню</li>
        <li><kbd>Удержание OK</kbd> &mdash; закончить ввод поля, либо действие "удержание", показанное внизу экрана</li>
        <li><kbd>BACK</kbd> (коротко) &mdash; вернуться на экран назад, либо стереть последний введённый символ</li>
        <li><kbd>Удержание BACK</kbd> &mdash; при вводе текста переключает набор символов (строчные, ЗАГЛАВНЫЕ,
          цифры, символы, кириллица строчная/ПРОПИСНАЯ)</li>
      </ul>
    </div>

    <div class="help-section">
      <h3>Учётные записи</h3>
      <p>У каждой записи есть Логин, Пароль, URL, Заметки, Секрет TOTP, Категория и отметка Избранное. На устройстве:
        удержание <kbd>OK</kbd> в списке записей создаёт новую; открытие существующей позволяет посмотреть,
        отредактировать, удалить, показать пароль или напечатать поле через USB. Эта веб-страница делает то же самое
        по сети &mdash; откройте запись для просмотра/редактирования, либо используйте <strong>+ Новая</strong>
        выше.</p>
      <p>Кириллица поддерживается в любом поле, и при вводе на устройстве, и при отображении &mdash; за это отвечают
        шрифт устройства и переключение набора символов (см. "Управление устройством" выше).</p>
    </div>

    <div class="help-section">
      <h3>Генератор паролей</h3>
      <p>Настройки &rarr; Генератор паролей задаёт длину и какие наборы символов использовать. Там же
        <strong>Сгенерировать и напечатать</strong> создаёт новый пароль и сразу печатает его через USB &mdash; удобно для
        формы регистрации на любом компьютере, куда воткнуто устройство, без создания записи в аккаунтах вообще.
        Внутри поля Пароль у записи удержание <kbd>OK</kbd> генерирует новый пароль прямо для неё.</p>
    </div>

    <div class="help-section">
      <h3>Коды восстановления</h3>
      <p>Это коды, которые сервис даёт вам как запасной способ входа, если обычный способ потерян (2FA-коды GitHub,
        коды банка, биржи и т.д.). Устройство их <strong>не создаёт</strong> само &mdash; только сам сервис может
        выдать коды, которые он потом примет обратно. На экране записи кнопка <strong>Задать / заменить</strong>
        позволяет вставить коды, полученные от сервиса (по одному на строку), либо <strong>Импорт из файла</strong>
        &mdash; загрузить их из .txt-файла, который сервис дал скачать. Отмечайте каждый использованным по мере
        траты; <strong>Копировать неиспользованные</strong> копирует оставшиеся в буфер обмена.</p>
      <p>На самом устройстве это только просмотр и печать &mdash; вводить весь набор кодов через энкодер
        непрактично, поэтому задать или заменить их можно только через веб.</p>
    </div>

    <div class="help-section">
      <h3>Seed-фраза</h3>
      <p>Для фразы восстановления крипто-кошелька (BIP-39 &mdash; 12, 15, 18, 21 или 24 слова). Та же логика, что и
        с кодами восстановления: фразу создаёт само приложение кошелька, не это устройство &mdash; вставьте или
        импортируйте именно ту фразу, что дал кошелёк, и устройство проверит каждое слово по настоящему словарю
        BIP-39 перед сохранением. По умолчанию скрыта (как и поле Пароль), с переключателем
        <strong>Показать</strong> &mdash; это чувствительнее почти всего остального, что тут хранится: у кого есть эта
        фраза, у того полный и необратимый контроль над кошельком.</p>
      <p>Как и с кодами восстановления, только одна фраза на запись, задать или заменить её можно только через веб;
        устройство умеет её просматривать (скрыто, <kbd>OK</kbd> переключает показ) и печатать через USB одной
        строкой через пробел &mdash; именно так большинство кошельков ожидают вставку фразы.</p>
    </div>

    <div class="help-section">
      <h3>Коды TOTP (2FA)</h3>
      <p>Вставьте Base32-секрет (та же строка, что даётся при настройке "вручную" вместо QR-кода, например
        <code>JBSWY3DPEHPK3PXP</code>) в поле Секрет TOTP записи. Устройство покажет живой, обновляющийся 6-значный
        код прямо на экране записи, а Напечатать OTP напечатает текущий код через USB.</p>
      <div class="note">У этой платы нет батарейного чипа часов реального времени &mdash; единственный источник
        времени — NTP через WiFi. Коды TOTP доступны только после того, как WiFi (режим Station) подключился и хотя
        бы раз синхронизировал время с момента включения устройства; после полного отключения питания это сбрасывается
        до следующего подключения.</div>
    </div>

    <div class="help-section">
      <h3>Печать через USB</h3>
      <p>Печать поля — это ввод текста как с USB-клавиатуры на тот компьютер, куда воткнуто устройство. Настройки
        &rarr; USB управляет задержками между нажатиями и порядком печати, плюс быстрый пароль, доступный с экрана
        блокировки без разблокировки (осознанный компромисс удобства и безопасности).</p>
      <p>Для печати кириллицы принимающему компьютеру нужна установленная русская раскладка. Настройки &rarr; USB
        &rarr; <strong>Авто-переключение раскладки</strong> (по умолчанию выключено) заставляет устройство отправлять
        Alt+Shift, пытаясь переключить раскладку самостоятельно до и после каждого кириллического участка &mdash;
        это работает не гарантированно, так как устройство не может знать, что настроено на принимающем компьютере.
        Если выключено — переключайте раскладку сами перед печатью.</p>
    </div>

    <div class="help-section">
      <h3>Резервная копия, экспорт и импорт</h3>
      <p>Две разные вещи, обе на microSD-карте, обе в разделе Резервная копия на устройстве:</p>
      <ul>
        <li><strong>Создать/восстановить резервную копию</strong> &mdash; полная, точная копия внутреннего файла хранилища
          устройства. Читается только другим устройством KeyKeeper2, не сторонними приложениями.</li>
        <li><strong>Экспорт/импорт хранилища</strong> &mdash; обычный CSV-файл, читаемый (или редактируемый) большинством
          других менеджеров паролей и табличных редакторов. Импорт добавляет записи, не заменяя уже существующие, и
          терпимо относится к названиям колонок (<code>username</code>, <code>site</code>, <code>note</code> и т.п.
          распознаются наравне с собственными названиями устройства). Через CSV переносятся только базовые поля
          (логин/пароль/url/заметки/секрет TOTP/категория/избранное) — коды восстановления и seed-фразы туда
          намеренно не попадают (CSV предназначен для переноса между разными менеджерами паролей, а у большинства
          из них и нет места для таких полей). Полную копию со всем содержимым даёт только Создать/восстановить резервную копию
          выше.</li>
      </ul>
      <p>На устройстве также есть действие Форматировать SD-карту — полезно, если карта пришла отформатированной в exFAT
        (обычное дело для карт от 32ГБ), который устройство прочитать не может.</p>
    </div>

    <div class="help-section">
      <h3>Безопасность</h3>
      <ul>
        <li>Повторные неверные попытки PIN усиливают реакцию: временная блокировка, затем полное стирание хранилища
          и PIN. Это намеренно и не отключается.</li>
        <li>Можно настроить отдельный PIN под принуждением — он разблокирует как обычно, но перед этим незаметно
          стирает хранилище — на случай если вас заставляют разблокировать устройство.</li>
        <li>Автоблокировка (Настройки &rarr; Безопасность) блокирует устройство после выбранного периода бездействия.</li>
        <li>Сброс до заводских стирает всё: хранилище, PIN, все настройки, данные WiFi.</li>
        <li>Файл хранилища на самом устройстве зашифрован (AES-256-GCM, ключ выводится из PIN) — а резервная копия
          или CSV-экспорт <strong>нет</strong>, осознанно (резервная копия должна работать с PIN любого устройства
          KeyKeeper2, а CSV предназначен для переноса между разными менеджерами паролей). Обращайтесь с файлом
          резервной копии или экспорта так же бережно, как с паролями внутри него.</li>
      </ul>
    </div>

    <div class="help-section">
      <h3>WiFi и эта веб-страница</h3>
      <p>Настройки &rarr; WiFi переключает между Station (подключение к существующей сети, нужно для синхронизации
        времени TOTP) и Точка доступа (устройство создаёт свою собственную сеть). Эта страница доступна в обоих
        режимах, по IP-адресу устройства &mdash; а если задано Секретное слово (там же, в настройках WiFi), то только по
        адресу <code>/&lt;secret word&gt;/</code>, а не по голому адресу — как лёгкий дополнительный барьер против
        случайного попадания на страницу перебором IP.</p>
    </div>

    <div class="help-section">
      <h3>Известные ограничения</h3>
      <ul>
        <li>Перевод самого интерфейса устройства (Общие настройки &rarr; Язык) ещё в процессе — часть экранов
          уже показывается по-русски при выборе языка, часть пока остаётся на английском независимо от настройки.</li>
        <li>Поиск и Backup/Restore пока недоступны с этой веб-страницы, только на самом устройстве.</li>
      </ul>
    </div>
    </div>
  </div>

</div>

<script>
// Set by web_russian_localization.hpp's own script (a separate,
// dictionary-based DOM translator -- see that file's own comment) --
// used here to translate a handful of dynamically-assigned button/
// status strings DIRECTLY at the moment they're set, rather than
// relying on that script's MutationObserver to catch the change
// after the fact. Confirmed on real hardware as the correct fix for
// several strings that stayed English despite already being correct,
// present dictionary entries (Set/Replace/Delete and others): plain
// .textContent assignment on an EXISTING element is a real DOM
// mutation and the observer's own childList config does pick it up
// in most cases, but exactly which of these specific call sites ran
// before the async, fetch-based language check had first resolved
// (a real timing gap right after unlock, not something worth trying
// to close from the observer side) varied in a way that wasn't worth
// chasing further. Always defined (returns the input unchanged) even
// before that script decides the language, so no caller needs its
// own existence check.
function t(s) {
  return (window.krTranslate && window.krTranslate(s)) || s;
}

let entries = [];
let currentEntryId = null;
let editingId = null;
let deleteConfirmPending = false;
let currentRecoveryCodes = [];
let currentSeedPhrase = [];
let seedRevealed = false;
let seedClearConfirmPending = false;

function showView(id) {
  ['list-view', 'detail-view', 'edit-view', 'settings-view', 'backup-view', 'help-view'].forEach(v => {
    document.getElementById(v).classList.toggle('hidden', v !== id);
  });
}

function setHelpLang(lang) {
  document.getElementById('help-content-en').style.display = lang === 'en' ? 'block' : 'none';
  document.getElementById('help-content-ru').style.display = lang === 'ru' ? 'block' : 'none';
}

async function api(path, options) {
  const res = await fetch(path, options);
  let body = {};
  try { body = await res.json(); } catch (e) {}
  return { ok: res.ok, status: res.status, body };
}

// ---------- Auth ----------

async function checkAuth() {
  const { ok, body } = await api('api/v1/auth/status');
  if (ok && body.data && body.data.authenticated) {
    showApp();
  }
}

async function login() {
  const pin = document.getElementById('pin').value;
  const msg = document.getElementById('login-msg');
  msg.style.color = '#000';
  msg.textContent = t('Checking...');
  try {
    const { ok, body } = await api('api/v1/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ pin: pin })
    });
    if (ok) {
      msg.textContent = '';
      showApp();
    } else {
      msg.style.color = '#c00';
      msg.textContent = t(body.message || 'Error');
    }
  } catch (e) {
    msg.style.color = '#c00';
    msg.textContent = t('Request failed: ') + e;
  }
}

function showApp() {
  document.getElementById('login-view').classList.add('hidden');
  document.getElementById('app-view').classList.remove('hidden');
  showView('list-view');
  loadList();
}

// ---------- List ----------

function renderEntryList(list_data, empty_message) {
  entries = list_data;
  const list = document.getElementById('entry-list');
  list.innerHTML = '';

  if (entries.length === 0) {
    list.innerHTML = '<div class="empty">' + empty_message + '</div>';
    return;
  }

  entries.forEach(e => {
    const div = document.createElement('div');
    div.className = 'entry-item';
    div.onclick = () => openEntry(e.id);
    const star = e.favorite ? '\u2605 ' : '';
    const otp = e.has_otp ? ' \u00b7 OTP' : '';
    const cat = e.category ? ' \u00b7 ' + escapeHtml(e.category) : '';
    div.innerHTML =
      '<div class="login">' + star + escapeHtml(e.login || '(no login)') + '</div>' +
      '<div class="meta">' + escapeHtml(e.url || '') + cat + otp + '</div>';
    list.appendChild(div);
  });
}

async function loadList() {
  const msg = document.getElementById('msg');
  msg.textContent = t('Loading...');
  const searchBox = document.getElementById('search-box');
  if (searchBox) searchBox.value = '';

  const { ok, body } = await api('api/v1/entries');
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to load');
    return;
  }

  msg.textContent = '';
  renderEntryList(body.data.entries || [], 'No accounts yet');
}

let searchDebounceTimer = null;

function onSearchInput() {
  clearTimeout(searchDebounceTimer);
  searchDebounceTimer = setTimeout(runSearch, 250);
}

async function runSearch() {
  const q = document.getElementById('search-box').value.trim();
  const msg = document.getElementById('msg');

  if (!q) {
    loadList();
    return;
  }

  msg.textContent = t('Searching...');
  const { ok, body } = await api('api/v1/search?q=' + encodeURIComponent(q));
  if (!ok) {
    msg.textContent = t(body.message || 'Search failed');
    return;
  }

  msg.textContent = '';
  renderEntryList(body.data.entries || [], 'No matches');
}

function escapeHtml(s) {
  const d = document.createElement('div');
  d.textContent = s == null ? '' : String(s);
  return d.innerHTML;
}

// ---------- Detail ----------

async function openEntry(id) {
  const { ok, body } = await api('api/v1/entry?id=' + encodeURIComponent(id));
  if (!ok) {
    alert(window.krTranslate ? window.krTranslate(body.message || 'Failed to load entry') : (body.message || 'Failed to load entry'));
    return;
  }

  currentEntryId = id;
  deleteConfirmPending = false;
  const e = body.data;
  const fields = document.getElementById('detail-fields');
  fields.innerHTML = '';

  const addField = (label, value, masked) => {
    if (!value) return;
    const wrap = document.createElement('div');
    const l = document.createElement('div');
    l.className = 'field-label';
    l.textContent = label;
    const v = document.createElement('div');
    v.className = 'field-value';
    const span = document.createElement('span');
    span.textContent = masked ? '\u2022'.repeat(8) : value;
    v.appendChild(span);
    if (masked) {
      const btn = document.createElement('button');
      btn.className = 'small secondary';
      btn.textContent = t('Show');
      btn.onclick = () => {
        const shown = span.textContent !== '\u2022'.repeat(8);
        span.textContent = shown ? '\u2022'.repeat(8) : value;
        btn.textContent = shown ? t('Show') : t('Hide');
      };
      v.appendChild(btn);
    }
    wrap.appendChild(l);
    wrap.appendChild(v);
    fields.appendChild(wrap);
  };

  addField('Login', e.login, false);
  addField('Password', e.password, true);
  addField('URL', e.url, false);
  addField('Notes', e.notes, false);
  addField('TOTP Secret', e.totp_secret, true);
  addField('Category', e.category, false);
  if (e.favorite) addField('Favorite', 'Yes', false);

  currentRecoveryCodes = e.recovery_codes || [];
  document.getElementById('recovery-codes-edit').style.display = 'none';
  renderRecoveryCodes();

  currentSeedPhrase = e.seed_phrase || [];
  seedRevealed = false;
  seedClearConfirmPending = false;
  document.getElementById('seed-phrase-edit').style.display = 'none';
  renderSeedPhrase();

  document.getElementById('delete-btn').textContent = t('Delete');
  showView('detail-view');
}

function renderRecoveryCodes() {
  const list = document.getElementById('recovery-codes-list');
  const actions = document.getElementById('recovery-codes-actions');
  const setBtn = document.getElementById('recovery-set-btn');
  const copyBtn = document.getElementById('recovery-copy-btn');
  list.innerHTML = '';

  if (currentRecoveryCodes.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'empty';
    empty.style.padding = '10px 0';
    empty.textContent = t('Not set.');
    list.appendChild(empty);
    copyBtn.style.display = 'none';
    setBtn.textContent = t('Set');
  } else {
    currentRecoveryCodes.forEach(rc => {
      const row = document.createElement('div');
      row.className = 'recovery-code-row' + (rc.used ? ' used' : '');
      const code = document.createElement('span');
      code.className = 'code';
      code.textContent = rc.code;
      const btn = document.createElement('button');
      btn.className = 'small secondary';
      btn.textContent = rc.used ? t('Mark unused') : t('Mark used');
      btn.onclick = () => toggleRecoveryCode(rc.code, !rc.used);
      row.appendChild(code);
      row.appendChild(btn);
      list.appendChild(row);
    });
    copyBtn.style.display = 'inline-block';
    setBtn.textContent = t('Replace');
  }
  actions.style.display = 'flex';
}

function startEditRecoveryCodes() {
  document.getElementById('recovery-codes-input').value = currentRecoveryCodes.map(rc => rc.code).join('\n');
  document.getElementById('recovery-codes-edit').style.display = 'block';
  document.getElementById('recovery-codes-actions').style.display = 'none';
}

function cancelEditRecoveryCodes() {
  document.getElementById('recovery-codes-edit').style.display = 'none';
  document.getElementById('recovery-codes-actions').style.display = 'flex';
  document.getElementById('recovery-msg').textContent = '';
}

function handleRecoveryCodesFile(event) {
  const file = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    document.getElementById('recovery-codes-input').value = reader.result;
  };
  reader.onerror = () => {
    document.getElementById('recovery-msg').textContent = t('Could not read that file.');
  };
  reader.readAsText(file);
  event.target.value = ''; // allow re-selecting the same file later
}

async function saveRecoveryCodes() {
  const raw = document.getElementById('recovery-codes-input').value;
  // One code per line -- whatever the service's own export/display
  // used (this device doesn't assume any particular format, since
  // every service's codes look different -- see this section's own
  // "note" in the HTML).
  const codes = raw.split(/\r?\n/).map(s => s.trim()).filter(s => s.length > 0);
  const msg = document.getElementById('recovery-msg');

  if (codes.length === 0) {
    msg.textContent = t('Paste or import at least one code first.');
    return;
  }

  msg.textContent = t('Saving...');
  const { ok, body } = await api(
    'api/v1/entry/recovery_codes?id=' + encodeURIComponent(currentEntryId),
    { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ codes }) }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to save recovery codes');
    return;
  }

  currentRecoveryCodes = body.data.recovery_codes;
  document.getElementById('recovery-codes-edit').style.display = 'none';
  document.getElementById('recovery-codes-actions').style.display = 'flex';
  msg.textContent = '';
  renderRecoveryCodes();
}

async function toggleRecoveryCode(code, used) {
  const msg = document.getElementById('recovery-msg');
  msg.textContent = '';
  const { ok, body } = await api(
    'api/v1/entry/recovery_codes/mark?id=' + encodeURIComponent(currentEntryId),
    { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ code, used }) }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to update code');
    return;
  }
  currentRecoveryCodes = body.data.recovery_codes;
  renderRecoveryCodes();
}

async function copyRecoveryCodes() {
  const unused = currentRecoveryCodes.filter(rc => !rc.used).map(rc => rc.code).join('\n');
  const msg = document.getElementById('recovery-msg');
  try {
    await navigator.clipboard.writeText(unused);
    msg.textContent = t('Copied to clipboard.');
  } catch (e) {
    msg.textContent = t('Could not access clipboard -- select and copy manually.');
  }
}

// ---------- Seed phrase ----------

const VALID_SEED_LENGTHS = [12, 15, 18, 21, 24];

function renderSeedPhrase() {
  const view = document.getElementById('seed-phrase-view');
  const revealBtn = document.getElementById('seed-reveal-btn');
  const editBtn = document.getElementById('seed-edit-btn');
  const copyBtn = document.getElementById('seed-copy-btn');
  const clearBtn = document.getElementById('seed-clear-btn');
  view.innerHTML = '';

  if (currentSeedPhrase.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'empty';
    empty.style.padding = '6px 0';
    empty.textContent = t('Not set.');
    view.appendChild(empty);
    revealBtn.style.display = 'none';
    copyBtn.style.display = 'none';
    clearBtn.style.display = 'none';
    editBtn.textContent = t('Set');
  } else {
    revealBtn.style.display = 'inline-block';
    revealBtn.textContent = seedRevealed ? t('Hide') : t('Reveal');
    copyBtn.style.display = 'inline-block';
    clearBtn.style.display = 'inline-block';
    editBtn.textContent = t('Replace');

    const list = document.createElement('div');
    list.style.fontFamily = 'ui-monospace, Consolas, monospace';
    list.style.fontSize = '0.92rem';
    list.style.lineHeight = '1.6';
    if (seedRevealed) {
      list.textContent = currentSeedPhrase.map((w, i) => (i + 1) + '. ' + w).join('   ');
    } else {
      list.textContent = currentSeedPhrase.map((_, i) => (i + 1) + '. \u2022\u2022\u2022\u2022').join('   ');
    }
    view.appendChild(list);
  }

  clearBtn.textContent = seedClearConfirmPending ? 'Tap again to confirm' : 'Clear';
}

function toggleSeedPhraseReveal() {
  seedRevealed = !seedRevealed;
  renderSeedPhrase();
}

function startEditSeedPhrase() {
  document.getElementById('seed-phrase-input').value = currentSeedPhrase.join(' ');
  document.getElementById('seed-phrase-edit').style.display = 'block';
  document.getElementById('seed-phrase-actions').style.display = 'none';
}

function cancelEditSeedPhrase() {
  document.getElementById('seed-phrase-edit').style.display = 'none';
  document.getElementById('seed-phrase-actions').style.display = 'flex';
  document.getElementById('seed-msg').textContent = '';
}

function handleSeedPhraseFile(event) {
  const file = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    document.getElementById('seed-phrase-input').value = reader.result;
  };
  reader.onerror = () => {
    document.getElementById('seed-msg').textContent = t('Could not read that file.');
  };
  reader.readAsText(file);
  event.target.value = '';
}

async function saveSeedPhrase() {
  const raw = document.getElementById('seed-phrase-input').value;
  const words = raw.trim().split(/\s+/).filter(w => w.length > 0).map(w => w.toLowerCase());
  const msg = document.getElementById('seed-msg');

  // Quick client-side length check only -- catches the most common
  // mistake (pasted the wrong thing, missed a word) instantly. The
  // real check (every word actually in the BIP-39 wordlist) runs
  // server-side, which has the 2048-word list to check against and
  // is the one that actually decides -- see
  // web_vault_routes.cpp's handle_set_seed_phrase().
  if (!VALID_SEED_LENGTHS.includes(words.length)) {
    msg.textContent = t('Must be 12, 15, 18, 21 or 24 words -- got ') + words.length + '.';
    return;
  }

  msg.textContent = t('Saving...');
  const { ok, body } = await api(
    'api/v1/entry/seed_phrase?id=' + encodeURIComponent(currentEntryId),
    { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ words }) }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to save seed phrase');
    return;
  }

  currentSeedPhrase = body.data.seed_phrase;
  seedRevealed = false;
  document.getElementById('seed-phrase-edit').style.display = 'none';
  document.getElementById('seed-phrase-actions').style.display = 'flex';
  msg.textContent = '';
  renderSeedPhrase();
}

async function copySeedPhrase() {
  const msg = document.getElementById('seed-msg');
  try {
    await navigator.clipboard.writeText(currentSeedPhrase.join(' '));
    msg.textContent = t('Copied to clipboard.');
  } catch (e) {
    msg.textContent = t('Could not access clipboard -- select and copy manually.');
  }
}

async function clearSeedPhrase() {
  if (!seedClearConfirmPending) {
    seedClearConfirmPending = true;
    renderSeedPhrase();
    return;
  }
  seedClearConfirmPending = false;

  const msg = document.getElementById('seed-msg');
  const { ok, body } = await api(
    'api/v1/entry/seed_phrase?id=' + encodeURIComponent(currentEntryId),
    { method: 'DELETE' }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to clear seed phrase');
    return;
  }

  currentSeedPhrase = [];
  seedRevealed = false;
  msg.textContent = '';
  renderSeedPhrase();
}

async function confirmDelete() {
  const btn = document.getElementById('delete-btn');
  if (!deleteConfirmPending) {
    deleteConfirmPending = true;
    btn.textContent = t('Tap again to confirm');
    return;
  }

  const { ok, body } = await api('api/v1/entry?id=' + encodeURIComponent(currentEntryId), { method: 'DELETE' });
  if (!ok) {
    alert(window.krTranslate ? window.krTranslate(body.message || 'Delete failed') : (body.message || 'Delete failed'));
    return;
  }
  showView('list-view');
  loadList();
}

// ---------- Edit / Create ----------

function openEdit(id) {
  editingId = id;

  // The list only carries summary fields -- if editing an existing
  // entry, fetch the full record first so Password/Notes/TOTP aren't
  // blanked out by mistake.
  if (id) {
    api('api/v1/entry?id=' + encodeURIComponent(id)).then(({ ok, body }) => {
      fillEditForm(ok ? body.data : {});
    });
  } else {
    fillEditForm({});
  }

  document.getElementById('edit-msg').textContent = '';
  showView('edit-view');
}

function fillEditForm(e) {
  document.getElementById('edit-login').value = e.login || '';
  document.getElementById('edit-password').value = e.password || '';
  document.getElementById('edit-url').value = e.url || '';
  document.getElementById('edit-notes').value = e.notes || '';
  document.getElementById('edit-totp').value = e.totp_secret || '';
  document.getElementById('edit-category').value = e.category || '';
  document.getElementById('edit-favorite').checked = !!e.favorite;
}

function cancelEdit() {
  if (editingId) {
    openEntry(editingId);
  } else {
    showView('list-view');
  }
}

async function saveEntry() {
  const msg = document.getElementById('edit-msg');
  const payload = {
    login: document.getElementById('edit-login').value,
    password: document.getElementById('edit-password').value,
    url: document.getElementById('edit-url').value,
    notes: document.getElementById('edit-notes').value,
    totp_secret: document.getElementById('edit-totp').value,
    category: document.getElementById('edit-category').value,
    favorite: document.getElementById('edit-favorite').checked
  };

  if (!payload.login) {
    msg.style.color = '#c00';
    msg.textContent = "'Login' must not be empty";
    return;
  }

  msg.style.color = '#000';
  msg.textContent = t('Saving...');

  const path = editingId ? ('api/v1/entry?id=' + encodeURIComponent(editingId)) : 'api/v1/entry';
  const method = editingId ? 'PUT' : 'POST';

  const { ok, body } = await api(path, {
    method: method,
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });

  if (!ok) {
    msg.style.color = '#c00';
    msg.textContent = t(body.message || 'Save failed');
    return;
  }

  showView('list-view');
  loadList();
}

// ---------- Settings ----------

async function openSettings() {
  const { ok, body } = await api('api/v1/settings');
  if (!ok) {
    alert(window.krTranslate ? window.krTranslate(body.message || 'Failed to load settings') : (body.message || 'Failed to load settings'));
    return;
  }

  const d = body.data;
  document.getElementById('set-language').value = d.general.language;
  document.getElementById('set-brightness').value = d.general.display_brightness;
  document.getElementById('set-screen-timeout').value = d.general.display_off_timeout_s;

  document.getElementById('set-default-password').value = d.usb.default_password;
  document.getElementById('set-cyrillic-auto-switch').checked = !!d.usb.cyrillic_auto_switch_layout;

  document.getElementById('set-auto-lock-enabled').checked = !!d.security.auto_lock_enabled;
  document.getElementById('set-auto-lock-timeout').value = d.security.auto_lock_timeout_s;

  document.getElementById('set-wifi-mode').value = d.wifi.mode;
  document.getElementById('set-sta-ssid').value = d.wifi.sta_ssid;
  document.getElementById('set-sta-password').value = d.wifi.sta_password;
  document.getElementById('set-ap-ssid').value = d.wifi.ap_ssid;
  document.getElementById('set-ap-password').value = d.wifi.ap_password;
  document.getElementById('set-captive-portal').checked = !!d.wifi.captive_portal_enabled;

  ['general', 'usb', 'security', 'wifi'].forEach(s => {
    document.getElementById('set-' + s + '-msg').textContent = '';
  });

  showView('settings-view');
}

async function saveSettings(section) {
  const msg = document.getElementById('set-' + section + '-msg');
  let payload = {};

  if (section === 'general') {
    payload = {
      language: document.getElementById('set-language').value,
      display_brightness: parseInt(document.getElementById('set-brightness').value, 10) || 0,
      display_off_timeout_s: parseInt(document.getElementById('set-screen-timeout').value, 10) || 0
    };
  } else if (section === 'usb') {
    payload = {
      default_password: document.getElementById('set-default-password').value,
      cyrillic_auto_switch_layout: document.getElementById('set-cyrillic-auto-switch').checked
    };
  } else if (section === 'security') {
    payload = {
      auto_lock_enabled: document.getElementById('set-auto-lock-enabled').checked,
      auto_lock_timeout_s: parseInt(document.getElementById('set-auto-lock-timeout').value, 10) || 0
    };
  } else if (section === 'wifi') {
    payload = {
      mode: document.getElementById('set-wifi-mode').value,
      sta_ssid: document.getElementById('set-sta-ssid').value,
      sta_password: document.getElementById('set-sta-password').value,
      ap_ssid: document.getElementById('set-ap-ssid').value,
      ap_password: document.getElementById('set-ap-password').value,
      captive_portal_enabled: document.getElementById('set-captive-portal').checked
    };
  }

  msg.style.color = '#000';
  msg.textContent = t('Saving...');

  const { ok, body } = await api('api/v1/settings/' + section, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });

  if (!ok) {
    msg.style.color = '#c00';
    msg.textContent = t(body.message || 'Save failed');
    return;
  }

  msg.style.color = '#080';
  msg.textContent = t('Saved.');
}

// ---------- Backup ----------

let backupRestoreConfirmPending = null; // filename currently pending a second confirm tap, or null

function openBackup() {
  backupRestoreConfirmPending = null;
  showView('backup-view');
  loadBackups();
}

async function loadBackups() {
  const list = document.getElementById('backup-list');
  const msg = document.getElementById('backup-msg');
  list.innerHTML = t('Loading...');

  const { ok, body } = await api('api/v1/backups');
  if (!ok) {
    list.innerHTML = '';
    msg.textContent = t(body.message || 'Failed to load backups');
    return;
  }

  const backups = body.data.backups || [];
  list.innerHTML = '';

  if (backups.length === 0) {
    list.innerHTML = '<div class="empty">No backups yet.</div>';
    return;
  }

  backups.forEach(b => {
    const row = document.createElement('div');
    row.className = 'backup-row';

    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = b.filename;

    const size = document.createElement('span');
    size.className = 'size';
    size.textContent = Math.round(b.size_bytes / 1024) + 'KB';

    const restoreBtn = document.createElement('button');
    restoreBtn.className = 'small secondary';
    restoreBtn.textContent = (backupRestoreConfirmPending === b.filename) ? 'Tap again to confirm' : 'Restore';
    restoreBtn.onclick = () => restoreBackup(b.filename);

    const deleteBtn = document.createElement('button');
    deleteBtn.className = 'small danger';
    deleteBtn.textContent = t('Delete');
    deleteBtn.onclick = () => deleteBackup(b.filename);

    row.appendChild(name);
    row.appendChild(size);
    row.appendChild(restoreBtn);
    row.appendChild(deleteBtn);
    list.appendChild(row);
  });
}

async function createBackup() {
  const msg = document.getElementById('backup-msg');
  msg.textContent = t('Creating...');
  const { ok, body } = await api('api/v1/backups', { method: 'POST' });
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to create backup');
    return;
  }
  msg.textContent = t('Created: ') + body.data.filename;
  loadBackups();
}

async function restoreBackup(filename) {
  if (backupRestoreConfirmPending !== filename) {
    // Overwrites the WHOLE vault and restarts the device -- same
    // "tap again to confirm" pattern as Delete elsewhere in this app,
    // not a silent one-tap action.
    backupRestoreConfirmPending = filename;
    loadBackups();
    return;
  }
  backupRestoreConfirmPending = null;

  const msg = document.getElementById('backup-msg');
  msg.textContent = t('Restoring -- device will restart...');
  const { ok, body } = await api(
    'api/v1/backups/restore?filename=' + encodeURIComponent(filename),
    { method: 'POST' }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Restore failed');
    return;
  }
  msg.textContent = t('Restoring. The device is restarting -- reconnect in a few seconds and log in again.');
}

async function deleteBackup(filename) {
  const msg = document.getElementById('backup-msg');
  const { ok, body } = await api(
    'api/v1/backups?filename=' + encodeURIComponent(filename),
    { method: 'DELETE' }
  );
  if (!ok) {
    msg.textContent = t(body.message || 'Failed to delete backup');
    return;
  }
  msg.textContent = '';
  loadBackups();
}

checkAuth();
</script>
</body>
</html>
)HTML";

} // namespace web
