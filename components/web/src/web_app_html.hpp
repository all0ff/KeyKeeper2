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

</div>

<script>
let entries = [];
let currentEntryId = null;
let editingId = null;
let deleteConfirmPending = false;

function showView(id) {
  ['list-view', 'detail-view', 'edit-view', 'settings-view'].forEach(v => {
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

  document.getElementById('delete-btn').textContent = 'Delete';
  showView('detail-view');
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
      default_password: document.getElementById('set-default-password').value
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
