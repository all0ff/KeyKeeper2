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
    <div class="topbar">
      <h2>Accounts</h2>
      <div style="display:flex; gap:8px">
        <button class="small secondary" onclick="showView('help-view')">Help</button>
        <button class="small secondary" onclick="openSettings()">Settings</button>
        <button class="small" onclick="openEdit(null)">+ New</button>
      </div>
    </div>
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
        wallet it belongs to -- treat it with at least the same care as the wallet itself. This device does not
        encrypt its storage (see Help).</div>
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
    <div id="set-wifi-msg"></div>
    <button onclick="saveSettings('wifi')" style="margin-top:8px">Save WiFi</button>
  </div>

  <div id="help-view" class="hidden">
    <div class="topbar">
      <h2>Help</h2>
      <button class="small secondary" onclick="showView('list-view')">&larr; Back</button>
    </div>

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
          all recognized, not just this device's own exact header names).</li>
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
        <li>The vault file on the device (and in a Backup or CSV export) is <strong>not encrypted</strong> &mdash;
          the PIN protects on-device access, not the data at rest. Treat a backup or export file with the same
          care as the passwords it contains.</li>
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
        <li>The Language setting (General settings) doesn't translate anything yet &mdash; the on-device interface
          is English-only regardless of what it's set to.</li>
        <li>Search and Backup/Restore aren't available from this web page yet, only on the device itself.</li>
      </ul>
    </div>
  </div>

</div>

<script>
let entries = [];
let currentEntryId = null;
let editingId = null;
let deleteConfirmPending = false;
let currentRecoveryCodes = [];
let currentSeedPhrase = [];
let seedRevealed = false;
let seedClearConfirmPending = false;

function showView(id) {
  ['list-view', 'detail-view', 'edit-view', 'settings-view', 'help-view'].forEach(v => {
    document.getElementById(v).classList.toggle('hidden', v !== id);
  });
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
  msg.textContent = 'Checking...';
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
      msg.textContent = body.message || 'Error';
    }
  } catch (e) {
    msg.style.color = '#c00';
    msg.textContent = 'Request failed: ' + e;
  }
}

function showApp() {
  document.getElementById('login-view').classList.add('hidden');
  document.getElementById('app-view').classList.remove('hidden');
  showView('list-view');
  loadList();
}

// ---------- List ----------

async function loadList() {
  const msg = document.getElementById('msg');
  const list = document.getElementById('entry-list');
  msg.textContent = 'Loading...';
  list.innerHTML = '';

  const { ok, body } = await api('api/v1/entries');
  if (!ok) {
    msg.textContent = body.message || 'Failed to load';
    return;
  }

  entries = body.data.entries || [];
  msg.textContent = '';

  if (entries.length === 0) {
    list.innerHTML = '<div class="empty">No accounts yet</div>';
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

function escapeHtml(s) {
  const d = document.createElement('div');
  d.textContent = s == null ? '' : String(s);
  return d.innerHTML;
}

// ---------- Detail ----------

async function openEntry(id) {
  const { ok, body } = await api('api/v1/entry?id=' + encodeURIComponent(id));
  if (!ok) {
    alert(body.message || 'Failed to load entry');
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
      btn.textContent = 'Show';
      btn.onclick = () => {
        const shown = span.textContent !== '\u2022'.repeat(8);
        span.textContent = shown ? '\u2022'.repeat(8) : value;
        btn.textContent = shown ? 'Show' : 'Hide';
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

  document.getElementById('delete-btn').textContent = 'Delete';
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
    empty.textContent = 'Not set.';
    list.appendChild(empty);
    copyBtn.style.display = 'none';
    setBtn.textContent = 'Set';
  } else {
    currentRecoveryCodes.forEach(rc => {
      const row = document.createElement('div');
      row.className = 'recovery-code-row' + (rc.used ? ' used' : '');
      const code = document.createElement('span');
      code.className = 'code';
      code.textContent = rc.code;
      const btn = document.createElement('button');
      btn.className = 'small secondary';
      btn.textContent = rc.used ? 'Mark unused' : 'Mark used';
      btn.onclick = () => toggleRecoveryCode(rc.code, !rc.used);
      row.appendChild(code);
      row.appendChild(btn);
      list.appendChild(row);
    });
    copyBtn.style.display = 'inline-block';
    setBtn.textContent = 'Replace';
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
    document.getElementById('recovery-msg').textContent = 'Could not read that file.';
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
    msg.textContent = 'Paste or import at least one code first.';
    return;
  }

  msg.textContent = 'Saving...';
  const { ok, body } = await api(
    'api/v1/entry/recovery_codes?id=' + encodeURIComponent(currentEntryId),
    { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ codes }) }
  );
  if (!ok) {
    msg.textContent = body.message || 'Failed to save recovery codes';
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
    msg.textContent = body.message || 'Failed to update code';
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
    msg.textContent = 'Copied to clipboard.';
  } catch (e) {
    msg.textContent = 'Could not access clipboard -- select and copy manually.';
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
    empty.textContent = 'Not set.';
    view.appendChild(empty);
    revealBtn.style.display = 'none';
    copyBtn.style.display = 'none';
    clearBtn.style.display = 'none';
    editBtn.textContent = 'Set';
  } else {
    revealBtn.style.display = 'inline-block';
    revealBtn.textContent = seedRevealed ? 'Hide' : 'Reveal';
    copyBtn.style.display = 'inline-block';
    clearBtn.style.display = 'inline-block';
    editBtn.textContent = 'Replace';

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
    document.getElementById('seed-msg').textContent = 'Could not read that file.';
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
    msg.textContent = 'Must be 12, 15, 18, 21 or 24 words -- got ' + words.length + '.';
    return;
  }

  msg.textContent = 'Saving...';
  const { ok, body } = await api(
    'api/v1/entry/seed_phrase?id=' + encodeURIComponent(currentEntryId),
    { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ words }) }
  );
  if (!ok) {
    msg.textContent = body.message || 'Failed to save seed phrase';
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
    msg.textContent = 'Copied to clipboard.';
  } catch (e) {
    msg.textContent = 'Could not access clipboard -- select and copy manually.';
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
    msg.textContent = body.message || 'Failed to clear seed phrase';
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
    btn.textContent = 'Tap again to confirm';
    return;
  }

  const { ok, body } = await api('api/v1/entry?id=' + encodeURIComponent(currentEntryId), { method: 'DELETE' });
  if (!ok) {
    alert(body.message || 'Delete failed');
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
  msg.textContent = 'Saving...';

  const path = editingId ? ('api/v1/entry?id=' + encodeURIComponent(editingId)) : 'api/v1/entry';
  const method = editingId ? 'PUT' : 'POST';

  const { ok, body } = await api(path, {
    method: method,
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });

  if (!ok) {
    msg.style.color = '#c00';
    msg.textContent = body.message || 'Save failed';
    return;
  }

  showView('list-view');
  loadList();
}

// ---------- Settings ----------

async function openSettings() {
  const { ok, body } = await api('api/v1/settings');
  if (!ok) {
    alert(body.message || 'Failed to load settings');
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
      ap_password: document.getElementById('set-ap-password').value
    };
  }

  msg.style.color = '#000';
  msg.textContent = 'Saving...';

  const { ok, body } = await api('api/v1/settings/' + section, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });

  if (!ok) {
    msg.style.color = '#c00';
    msg.textContent = body.message || 'Save failed';
    return;
  }

  msg.style.color = '#080';
  msg.textContent = 'Saved.';
}

checkAuth();
</script>
</body>
</html>
)HTML";

} // namespace web
