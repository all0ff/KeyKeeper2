#pragma once

namespace web {

// Client-side account reordering. This is injected after APP_PAGE, so the
// existing list renderer and API helpers are already defined. It deliberately
// keeps the actual vault data untouched: only the Web UI presentation order is
// persisted through /api/v1/account-order.
constexpr char ACCOUNT_REORDER_SCRIPT[] = R"JS(<script>
(function () {
  const originalRenderEntryList = window.renderEntryList;
  const originalLoadList = window.loadList;
  if (typeof originalRenderEntryList !== 'function' || typeof originalLoadList !== 'function') return;

  let order = [];
  let orderReady = false;
  const selected = new Set();
  let dragging = false;
  let dragIds = [];
  let dragSourceId = null;
  // Auto-scroll while dragging near a screen edge (confirmed as a
  // real, reported gap: without this, a drag can only reorder within
  // whatever's already visible on screen -- there was no way to drag
  // an account from the top of a long list down past the fold at
  // all). The page itself scrolls (there's no separate scrollable
  // container around #entry-list -- window.scrollBy is the right
  // target, not some inner element's scrollTop), and a drag target
  // held still near an edge still needs to keep scrolling even
  // though no further pointermove event is firing -- hence a
  // requestAnimationFrame loop driven by the last known pointer
  // position, not just the move handler alone.
  const EDGE_ZONE_PX = 60;
  const MAX_SCROLL_PX_PER_FRAME = 16;
  let lastPointerX = 0;
  let lastPointerY = 0;

  function updateDropMarker(clientX, clientY) {
    const target = document.elementFromPoint(clientX, clientY)?.closest('.kk-entry-shell');
    clearDropMarkers();
    if (!target) return;
    const targetId = Number(target.dataset.id);
    if (dragIds.includes(targetId)) return;
    const rect = target.getBoundingClientRect();
    target.classList.add(clientY < rect.top + rect.height / 2 ? 'kk-drop-before' : 'kk-drop-after');
  }

  function autoScrollStep() {
    if (!dragging) return;
    const viewportHeight = window.innerHeight;
    let delta = 0;
    if (lastPointerY < EDGE_ZONE_PX) {
      const intensity = (EDGE_ZONE_PX - lastPointerY) / EDGE_ZONE_PX;
      delta = -Math.ceil(intensity * MAX_SCROLL_PX_PER_FRAME);
    } else if (lastPointerY > viewportHeight - EDGE_ZONE_PX) {
      const intensity = (lastPointerY - (viewportHeight - EDGE_ZONE_PX)) / EDGE_ZONE_PX;
      delta = Math.ceil(intensity * MAX_SCROLL_PX_PER_FRAME);
    }
    if (delta !== 0) {
      window.scrollBy(0, delta);
      // The page just moved under a pointer that didn't -- no new
      // pointermove event is coming on its own to refresh the drop
      // marker, so this loop does it itself using the pointer's last
      // known viewport position (still valid: scrolling moves page
      // CONTENT, not the pointer's own clientX/clientY).
      updateDropMarker(lastPointerX, lastPointerY);
    }
    requestAnimationFrame(autoScrollStep);
  }

  function tr(s) {
    const translated = (window.krTranslate && window.krTranslate(s));
    if (translated && translated !== s) return translated;
    let russian = false;
    try { russian = localStorage.getItem('kr_lang') === 'russian'; } catch (_) {}
    if (russian) {
      const extra = {
        'Selected: ': 'Выбрано: ',
        'Clear selection': 'Снять выделение',
        'Drag to reorder': 'Перетащить для изменения порядка',
        'Failed to save order': 'Не удалось сохранить порядок'
      };
      return extra[s] || s;
    }
    return s;
  }

  function ensureStyle() {
    if (document.getElementById('kk-reorder-style')) return;
    const style = document.createElement('style');
    style.id = 'kk-reorder-style';
    style.textContent = `
      .kk-entry-shell { display:flex; align-items:stretch; gap:6px; margin:8px 0; }
      .kk-entry-shell .entry-item { flex:1; margin:0; }
      .kk-entry-tools { width:42px; display:flex; flex-direction:column; align-items:center; justify-content:center; gap:8px; }
      .kk-drag-handle { width:30px; height:42px; border:1px solid #cbd5e1; border-radius:6px; background:transparent; display:grid; grid-template-columns:repeat(2,4px); grid-auto-rows:4px; justify-content:center; align-content:center; gap:3px; padding:0; cursor:grab; touch-action:none; }
      .kk-drag-handle span { display:block; width:4px; height:4px; border:1px solid #64748b; border-radius:1px; }
      .kk-drag-handle:active { cursor:grabbing; }
      .kk-entry-shell.kk-selected .entry-item { outline:2px solid #2563eb; }
      .kk-entry-shell.kk-dragging { opacity:.45; }
      .kk-drop-before { border-top:3px solid #2563eb !important; }
      .kk-drop-after { border-bottom:3px solid #2563eb !important; }
      .kk-reorder-bar { display:flex; align-items:center; gap:8px; margin:4px 0 8px; }
      .kk-reorder-count { flex:1; color:#6b7280; font-size:.85rem; }
      .kk-reorder-bar button { width:auto; }
    `;
    document.head.appendChild(style);
  }

  async function api(path, options) {
    const res = await fetch(path, options);
    let body = {};
    try { body = await res.json(); } catch (_) {}
    return { ok: res.ok, status: res.status, body };
  }

  async function loadOrder() {
    const result = await api('api/v1/account-order');
    if (result.ok && result.body && result.body.data && Array.isArray(result.body.data.ids)) {
      order = result.body.data.ids.map(Number);
    }
    orderReady = true;
  }

  function normalizeOrder(list) {
    const ids = list.map(e => Number(e.id));
    const valid = new Set(ids);
    const seen = new Set();
    const result = [];
    for (const id of order) {
      if (valid.has(id) && !seen.has(id)) {
        seen.add(id);
        result.push(id);
      }
    }
    for (const id of ids) {
      if (!seen.has(id)) {
        seen.add(id);
        result.push(id);
      }
    }
    return result;
  }

  function sortEntries(list) {
    if (!orderReady) return list;
    const position = new Map(order.map((id, index) => [id, index]));
    return [...list].sort((a, b) =>
      (position.get(Number(a.id)) ?? 999999) - (position.get(Number(b.id)) ?? 999999));
  }

  async function saveOrder() {
    const result = await api('api/v1/account-order', {
      method:'PUT',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ids:order})
    });
    if (!result.ok) {
      const message = result.body && result.body.message ? result.body.message : 'Failed to save order';
      alert(tr(message));
      return false;
    }
    return true;
  }

  function updateSelectionBar() {
    const list = document.getElementById('entry-list');
    if (!list) return;
    let bar = document.getElementById('kk-reorder-bar');
    if (!bar) {
      bar = document.createElement('div');
      bar.id = 'kk-reorder-bar';
      bar.className = 'kk-reorder-bar';
      list.parentNode.insertBefore(bar, list);
    }
    bar.innerHTML = '';
    if (selected.size === 0) {
      bar.style.display = 'none';
      return;
    }
    bar.style.display = 'flex';
    const count = document.createElement('span');
    count.className = 'kk-reorder-count';
    count.textContent = tr('Selected: ') + selected.size;
    const clear = document.createElement('button');
    clear.className = 'small secondary';
    clear.textContent = tr('Clear selection');
    clear.onclick = () => {
      selected.clear();
      document.querySelectorAll('.kk-select').forEach(cb => cb.checked = false);
      document.querySelectorAll('.kk-entry-shell').forEach(el => el.classList.remove('kk-selected'));
      updateSelectionBar();
    };
    bar.append(count, clear);
  }

  function clearDropMarkers() {
    document.querySelectorAll('.kk-drop-before,.kk-drop-after,.kk-dragging').forEach(el => {
      el.classList.remove('kk-drop-before','kk-drop-after','kk-dragging');
    });
  }

  async function finishDrag(targetId, after) {
    clearDropMarkers();
    if (!dragging || targetId == null) return;

    const moving = dragIds.length ? dragIds : [dragSourceId];
    const current = order.filter(id => !moving.includes(id));
    let targetIndex = current.indexOf(targetId);
    if (targetIndex < 0) {
      order = current.concat(moving);
    } else {
      if (after) targetIndex++;
      current.splice(targetIndex, 0, ...moving);
      order = current;
    }

    dragging = false;
    dragIds = [];
    dragSourceId = null;

    if (await saveOrder()) {
      // `entries` (bare identifier, NOT window.entries) -- the main
      // app script declares it via `let entries = [];` at its own
      // top level (web_app_html.hpp). Top-level let/const bindings
      // are never exposed as window properties, unlike var/function
      // declarations -- window.entries was always undefined here,
      // which is why decorateList() (below) silently never built any
      // reorder UI despite the account list itself rendering fine
      // (confirmed on real hardware: list visible, no drag handles,
      // no console errors -- exactly this failure mode). Classic
      // (non-module) <script> tags on the same page share one global
      // lexical environment, so the bare name resolves correctly
      // even across separate <script> blocks like this one and the
      // main app's.
      const currentEntries = entries || [];
      originalRenderEntryList(sortEntries(currentEntries), tr('No accounts yet'));
      decorateList();
    }
  }

  function startDrag(event, shell, id) {
    if (event.pointerType === 'mouse' && event.button !== 0) return;
    event.preventDefault();
    dragging = true;
    dragSourceId = id;
    dragIds = selected.has(id) ? [...selected] : [id];
    shell.classList.add('kk-dragging');
    lastPointerX = event.clientX;
    lastPointerY = event.clientY;
    requestAnimationFrame(autoScrollStep);

    const move = ev => {
      if (!dragging) return;
      lastPointerX = ev.clientX;
      lastPointerY = ev.clientY;
      updateDropMarker(ev.clientX, ev.clientY);
    };

    const up = async ev => {
      document.removeEventListener('pointermove', move);
      document.removeEventListener('pointerup', up);
      if (!dragging) return;
      const target = document.elementFromPoint(ev.clientX, ev.clientY)?.closest('.kk-entry-shell');
      if (!target) {
        dragging = false;
        dragIds = [];
        dragSourceId = null;
        clearDropMarkers();
        return;
      }
      const targetId = Number(target.dataset.id);
      if (dragIds.includes(targetId)) {
        dragging = false;
        dragIds = [];
        dragSourceId = null;
        clearDropMarkers();
        return;
      }
      const rect = target.getBoundingClientRect();
      await finishDrag(targetId, ev.clientY >= rect.top + rect.height / 2);
    };

    document.addEventListener('pointermove', move);
    document.addEventListener('pointerup', up, { once:true });
  }

  function decorateList() {
    ensureStyle();
    const list = document.getElementById('entry-list');
    if (!list) return;

    // The base renderer has just recreated plain .entry-item nodes.
    const items = [...list.querySelectorAll(':scope > .entry-item')];
    if (!items.length) {
      updateSelectionBar();
      return;
    }

    // See finishDrag()'s own comment above on why this is the bare
    // `entries` identifier, not window.entries.
    const currentEntries = entries || [];
    const visibleIds = new Set(currentEntries.map(e => Number(e.id)));
    for (const id of [...selected]) {
      if (!visibleIds.has(id)) selected.delete(id);
    }

    items.forEach((item, index) => {
      const entry = currentEntries[index];
      if (!entry) return;
      const id = Number(entry.id);
      const shell = document.createElement('div');
      shell.className = 'kk-entry-shell';
      shell.dataset.id = id;

      const tools = document.createElement('div');
      tools.className = 'kk-entry-tools';

      const checkbox = document.createElement('input');
      checkbox.type = 'checkbox';
      checkbox.className = 'kk-select';
      checkbox.checked = selected.has(id);
      checkbox.onclick = event => event.stopPropagation();
      checkbox.onchange = () => {
        if (checkbox.checked) selected.add(id); else selected.delete(id);
        shell.classList.toggle('kk-selected', checkbox.checked);
        updateSelectionBar();
      };

      const handle = document.createElement('button');
      handle.type = 'button';
      handle.className = 'kk-drag-handle';
      handle.title = tr('Drag to reorder');
      for (let i = 0; i < 6; ++i) handle.appendChild(document.createElement('span'));
      handle.onpointerdown = event => startDrag(event, shell, id);
      handle.onclick = event => event.stopPropagation();

      tools.append(checkbox, handle);
      shell.append(item, tools);
      list.appendChild(shell);
      shell.classList.toggle('kk-selected', selected.has(id));
    });

    updateSelectionBar();
  }

  window.renderEntryList = function (listData, emptyMessage) {
    const sorted = sortEntries(listData);
    order = normalizeOrder(sorted);
    originalRenderEntryList(sorted, emptyMessage);
    decorateList();
  };

  window.loadList = async function () {
    await loadOrder();
    return originalLoadList();
  };

  loadOrder().catch(() => { orderReady = true; });
})();
</script>)JS";

} // namespace web
