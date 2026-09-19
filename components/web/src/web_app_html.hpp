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