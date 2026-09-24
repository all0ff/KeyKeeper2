#pragma once

// Client-side localization for the embedded Web UI.
// The API and DOM structure remain unchanged; this only translates
// user-visible labels, buttons, placeholders and common status text.

namespace web {

constexpr char RUSSIAN_LOCALIZATION_SCRIPT[] = R"JS(<script>
(function () {
  const RU = {
    "Unlock": "Разблокировать",
    "Accounts": "Учётные записи",
    "Help": "Помощь",
    "Settings": "Настройки",
    "+ New": "+ Новая",
    "Back": "Назад",
    "← Back": "← Назад",
    "Recovery Codes": "Коды восстановления",
    "Import from file": "Импорт из файла",
    "Save": "Сохранить",
    "Cancel": "Отмена",
    "← Cancel": "← Отмена",
    "Set / Replace": "Задать / заменить",
    "Set": "Задать",
    "Copy unused": "Копировать неиспользованные",
    "Reveal": "Показать",
    "Copy": "Копировать",
    "Clear": "Очистить",
    "Edit": "Изменить",
    "Delete": "Удалить",
    "Login": "Логин",
    "Password": "Пароль",
    "URL": "URL",
    "Notes": "Заметки",
    "TOTP Secret": "Секрет TOTP",
    "Category": "Категория",
    "Favorite": "Избранное",
    "Seed Phrase": "Сид-фраза",
    "Search login, URL, notes...": "Поиск по логину, URL, заметкам...",
    "No matches": "Совпадений не найдено",
    "No set": "Не задано",
    "Backup": "Резервная копия",
    "Restore": "Восстановление",
    "Backup / Restore": "Резервное копирование / восстановление",
    "Create Backup": "Создать резервную копию",
    "Create backup": "Создать резервную копию",
    "Download": "Скачать",
    "Upload": "Загрузить",
    "Choose File": "Выбрать файл",
    "Choose file": "Выбрать файл",
    "Restore Backup": "Восстановить резервную копию",
    "Restore backup": "Восстановить резервную копию",
    "Restore Selected": "Восстановить выбранную",
    "No backups found": "Резервные копии не найдены",
    "No backup files found": "Файлы резервных копий не найдены",
    "Backup created": "Резервная копия создана",
    "Backup deleted": "Резервная копия удалена",
    "Backup failed": "Не удалось создать резервную копию",
    "Restore successful": "Восстановление выполнено",
    "Restore failed": "Не удалось выполнить восстановление",
    "Are you sure?": "Вы уверены?",
    "Select a backup file": "Выберите файл резервной копии",
    "Invalid backup file": "Недопустимый файл резервной копии",
    "File selected": "Файл выбран",
    "Tap again to confirm": "Нажмите ещё раз для подтверждения",
    "Tap again to confirm restore": "Нажмите ещё раз для подтверждения восстановления",
    "English": "Английский",
    "Russian": "Русский",
    "General": "Общие",
    "Language": "Язык",
    "Display Brightness (0-100)": "Яркость дисплея (0–100)",
    "Screen Timeout (seconds, 0 = off)": "Тайм-аут экрана (секунды, 0 = выкл.)",
    "Save General": "Сохранить общие",
    "USB": "USB",
    "Quick Password (no PIN required)": "Быстрый пароль (PIN не требуется)",
    "Auto-switch keyboard layout for Cyrillic (best-effort, Alt+Shift)": "Автопереключение раскладки для кириллицы (по возможности, Alt+Shift)",
    "Save USB": "Сохранить USB",
    "Security": "Безопасность",
    "Auto Lock enabled": "Автоблокировка включена",
    "Auto Lock timeout": "Тайм-аут автоблокировки",
    "Off": "Выкл.",
    "1 min": "1 мин",
    "3 min": "3 мин",
    "5 min": "5 мин",
    "10 min": "10 мин",
    "15 min": "15 мин",
    "20 min": "20 мин",
    "25 min": "25 мин",
    "30 min": "30 мин",
    "35 min": "35 мин",
    "40 min": "40 мин",
    "45 min": "45 мин",
    "50 min": "50 мин",
    "55 min": "55 мин",
    "60 min": "60 мин",
    "Web UI permissions": "Разрешения Web UI",
    "View accounts": "Просмотр учётных записей",
    "Edit accounts": "Изменение учётных записей",
    "Export data": "Экспорт данных",
    "Change settings": "Изменение настроек",
    "Save Security": "Сохранить безопасность",
    "Wi-Fi": "Wi-Fi",
    "Mode": "Режим",
    "Disabled": "Отключён",
    "Station": "Станция",
    "Station SSID": "SSID клиента",
    "Station Password": "Пароль клиента",
    "Access Point": "Точка доступа",
    "STA SSID": "SSID станции",
    "STA Password": "Пароль станции",
    "AP SSID": "SSID точки доступа",
    "AP Password": "Пароль точки доступа",
    "Save WiFi": "Сохранить WiFi",
    "Captive Portal (Access Point mode only)": "Captive Portal (только режим точки доступа)",
    "Password Generator": "Генератор паролей",
    "Password Gen": "Генератор паролей",
    "Generate": "Сгенерировать",
    "Length": "Длина",
    "Include uppercase": "Заглавные буквы",
    "Include lowercase": "Строчные буквы",
    "Include numbers": "Цифры",
    "Include symbols": "Символы",
    "System": "Система",
    "About": "О программе",
    "Firmware": "Прошивка",
    "Device": "Устройство",
    "Flash Encryption": "Шифрование Flash",
    "On": "Вкл.",
    "Free heap": "Свободная память",
    "Internal storage": "Внутреннее хранилище",
    "microSD": "microSD",
    "unavailable": "недоступно",
    "present, usage unavailable": "установлена, использование недоступно",
    "not detected": "не обнаружена",
    "Help & keyboard": "Помощь и клавиатура",
    "Keyboard": "Клавиатура",
    "Encoder": "Энкодер",
    "OK": "OK",
    "BACK": "НАЗАД",
    "Left": "Влево",
    "Right": "Вправо",
    "Press": "Нажатие",
    "Short press": "Короткое нажатие",
    "Long press": "Долгое нажатие",
    "Rotate": "Поворот",
    "Select": "Выбрать",
    "Confirm": "Подтвердить",
    "Open": "Открыть",
    "Close": "Закрыть",
    "Enter": "Ввод",
    "Exit": "Выход",
    "Save failed": "Ошибка сохранения",
    "Wrong PIN": "Неверный PIN",
    "Locked out, try again later": "Ввод заблокирован, попробуйте позже",
    "Not authenticated": "Не выполнена аутентификация",
    "Not authenticated.": "Не выполнена аутентификация.",
    "No PIN configured yet -- set one up on the device first": "PIN ещё не настроен — сначала задайте его на устройстве",
    "Too many failed attempts -- WiFi disabled": "Слишком много неудачных попыток — Wi-Fi отключён",
    "Missing 'pin' field": "Отсутствует поле PIN",
    "Invalid JSON": "Недопустимый JSON",
    "Failed to save": "Не удалось сохранить",
    "Saved.": "Сохранено.",
    "Loading...": "Загрузка...",
    "Saving...": "Сохранение...",
    "Error": "Ошибка",
    "Success": "Успешно",
    "Failed": "Не удалось",
    "Not found": "Не найдено",
    "No data": "Нет данных",
    "Required": "Обязательно",
    "Invalid value": "Недопустимое значение",
    "Connection failed": "Ошибка подключения",
    "Permission denied": "Доступ запрещён",
    "Unauthorized": "Не авторизован",
    "Not set.": "Не задано.",
    "Checking...": "Проверка...",
    "Mark used": "Отметить использованным",
    "Mark unused": "Снять отметку",
    "Replace": "Заменить",
    "Show": "Показать",
    "Hide": "Скрыть",
    "Yes": "Да",
    "Restoring -- device will restart...": "Восстановление -- устройство перезагрузится...",
    "Restoring. The device is restarting -- reconnect in a few seconds and log in again.":
      "Восстановление. Устройство перезагружается -- переподключитесь через несколько секунд и войдите снова.",
    "A full, exact copy of the device's own internal vault file, on the microSD card. Only ever readable by another KeyKeeper2 device, not other password managers or spreadsheet apps -- for that, use Export Vault (CSV) on the device itself instead, though that CSV export leaves out recovery codes and seed phrases (only login/password/url/notes/TOTP secret/category/favorite) -- a Backup here is the only copy that includes everything.":
      "Полная, точная копия внутреннего файла хранилища устройства, на microSD-карте. Читается только другим устройством KeyKeeper2, не другими менеджерами паролей или табличными редакторами -- для этого используйте Export Vault (CSV) на самом устройстве, хотя такой CSV-экспорт не включает коды восстановления и seed-фразы (только login/password/url/notes/TOTP secret/category/favorite) -- Backup здесь -- единственная копия, включающая всё.",
    "Failed to load": "Не удалось загрузить",
    "Search failed": "Ошибка поиска",
    "Failed to save recovery codes": "Не удалось сохранить коды восстановления",
    "Failed to update code": "Не удалось обновить код",
    "Failed to save seed phrase": "Не удалось сохранить seed-фразу",
    "Failed to clear seed phrase": "Не удалось очистить seed-фразу",
    "Failed to load backups": "Не удалось загрузить список резервных копий",
    "Failed to create backup": "Не удалось создать резервную копию",
    "Failed to delete backup": "Не удалось удалить резервную копию",
    "Failed to load entry": "Не удалось загрузить запись",
    "Delete failed": "Ошибка удаления",
    "Failed to load settings": "Не удалось загрузить настройки",
    "These come FROM the service the account belongs to (its own 2FA or account-recovery settings page) -- paste or import the ones it gave you. This device has no way to create codes that service would actually accept.":
      "Эти коды выдаёт сам сервис, которому принадлежит запись (его собственная страница настроек 2FA или восстановления доступа) -- вставьте или импортируйте именно те, что он выдал. Устройство не может само создать коды, которые сервис реально примет.",
    "Anyone who has this phrase has full, irreversible control of the wallet it belongs to -- treat it with at least the same care as the wallet itself. This device does not encrypt its storage (see Help).":
      "У кого есть эта фраза, у того полный и необратимый контроль над кошельком, которому она принадлежит -- обращайтесь с ней не менее бережно, чем с самим кошельком. Устройство не шифрует своё хранилище (см. Помощь)."
  };

  // For code that shows a message via alert() (a native browser
  // dialog, not a DOM node) -- translate() below only ever walks the
  // PAGE's own DOM, so it can't reach text inside a native alert()
  // popup no matter what's in RU above. A confirmed real case: the
  // three alert(body.message || '...') call sites in the main app
  // script stayed in English even once "Not authenticated" (a common
  // body.message when a session lapses) was already a correct RU
  // entry above -- the dictionary was right, alert() just doesn't go
  // through translate() at all. Exposed globally so the main app
  // script can wrap exactly those call sites; returns the original
  // string unchanged (never throws, never returns undefined) if RU
  // has no entry for it or this script hasn't set language to
  // Russian, so callers never need to check that themselves first.
  window.krTranslate = function (s) {
    return RU[s] || s;
  };

  const ATTRS = ["placeholder", "title", "aria-label"];
  let observer = null;
  let retryTimer = null;

  function normalizeWhitespace(s) {
    // HTML source line-wrapping and indentation (newlines, runs of
    // spaces) are NOT collapsed in node.nodeValue -- that collapsing
    // is CSS's doing, for the visual render only; the DOM text node
    // itself keeps the raw source whitespace verbatim. Confirmed as
    // the real cause of a real bug: every dictionary key here for a
    // paragraph spanning more than one source line was written as
    // plain, single-spaced text (how the text reads, not how the
    // source happens to be wrapped) and so could never exact-match
    // node.nodeValue's own literal "word\n        word" runs no
    // matter how many times the key itself was checked byte-for-byte
    // correct against the reconstructed rendered text -- the
    // reconstruction was the bug, not the key. Single-line strings
    // (most of this dictionary) were never affected, which is why
    // this went unnoticed through several rounds of fixing individual
    // entries instead of the lookup itself.
    return s.replace(/\s+/g, " ").trim();
  }

  function translateTextNode(node) {
    const text = node.nodeValue;
    if (!text || !text.trim()) return;
    const trimmed = text.trim();
    if (RU[trimmed]) {
      node.nodeValue = text.replace(trimmed, RU[trimmed]);
      return;
    }

    // Whitespace-normalized fallback for exactly the multi-source-line
    // case above -- replaces the WHOLE node value (not a substring
    // splice like the exact-match branch just above), since once
    // translated the original's internal line-wrapping no longer
    // means anything.
    const normalized = normalizeWhitespace(text);
    if (normalized !== trimmed && RU[normalized]) {
      node.nodeValue = RU[normalized];
      return;
    }

    let out = text;
    out = out.replace(/^Firmware:\s*/, RU["Firmware"] + ": ");
    out = out.replace(/^Device:\s*/, RU["Device"] + ": ");
    out = out.replace(/^Flash Encryption:\s*/, RU["Flash Encryption"] + ": ");
    out = out.replace(/^Free heap:\s*/, RU["Free heap"] + ": ");
    out = out.replace(/^Internal storage:\s*/, RU["Internal storage"] + ": ");
    out = out.replace(/←\s*Back/g, "← Назад");
    out = out.replace(/←\s*Cancel/g, "← Отмена");
    out = out.replace(/Tap again to confirm restore/g, "Нажмите ещё раз для подтверждения восстановления");
    out = out.replace(/Tap again to confirm/g, "Нажмите ещё раз для подтверждения");
    if (out !== text) node.nodeValue = out;
  }

  function translate(root) {
    if (!root) return;
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    nodes.forEach(translateTextNode);

    if (root.nodeType === Node.ELEMENT_NODE) {
      ATTRS.forEach(function (attr) {
        if (root.hasAttribute && root.hasAttribute(attr)) {
          const value = root.getAttribute(attr);
          if (RU[value]) root.setAttribute(attr, RU[value]);
        }
      });
      if (root.querySelectorAll) {
        root.querySelectorAll("[placeholder],[title],[aria-label]").forEach(function (el) {
          ATTRS.forEach(function (attr) {
            if (el.hasAttribute(attr)) {
              const value = el.getAttribute(attr);
              if (RU[value]) el.setAttribute(attr, RU[value]);
            }
          });
        });
      }
    }
  }

  function startObserver() {
    if (observer || !document.body) return;
    observer = new MutationObserver(function (mutations) {
      mutations.forEach(function (m) {
        m.addedNodes.forEach(function (node) {
          if (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.TEXT_NODE) {
            translate(node);
          }
        });
      });
    });
    observer.observe(document.body, { childList: true, subtree: true });
  }

  async function applyRussianIfSelected() {
    try {
      const response = await fetch("api/v1/settings", { cache: "no-store" });
      if (!response.ok) return false;
      const data = await response.json();
      if (data && data.data && data.data.general && data.data.general.language === "russian") {
        try {
          localStorage.setItem("kr_lang", "russian");
        } catch (_) {
          // Private browsing / storage disabled -- fine, this cache is
          // purely a convenience for the pre-login screen (see
          // startLocalization()'s own comment); the authenticated
          // check right above already applied the real translation
          // for this visit regardless.
        }
        document.documentElement.lang = "ru";
        translate(document.body);
        startObserver();
        // Help's own EN/RU toggle (setHelpLang(), a separate,
        // pre-existing mechanism from this dictionary translator --
        // see this script's own file comment) defaults to English
        // regardless of this setting, since it's just a manual button
        // click with no memory of its own. Syncing it here once,
        // right when the rest of the page goes Russian, means opening
        // Help for the first time already shows the matching content
        // instead of defaulting back to English independently of
        // everything else already translated on the page -- the
        // person can still click EN inside Help afterward if they
        // specifically want that section in English.
        if (typeof setHelpLang === "function") {
          setHelpLang("ru");
        }
        return true;
      }
      // Settings were read successfully and the language is not Russian
      // -- clear any stale cached guess from an earlier visit when it
      // WAS Russian, so a language change back to English is reflected
      // on the pre-login screen too, not just everywhere past login.
      try {
        localStorage.removeItem("kr_lang");
      } catch (_) {
        // See the setItem try/catch above.
      }
      // Stop polling; there is no reason to keep hitting the API.
      return true;
    } catch (_) {
      // Authentication/network failure: retry later.
    }
    return false;
  }

  function startLocalization() {
    // The authenticated check in applyRussianIfSelected() can only
    // ever succeed AFTER unlocking -- GET /api/v1/settings requires a
    // session (require_unlocked() server-side), which the login
    // screen itself obviously doesn't have yet. Confirmed on real
    // hardware: that made the WHOLE pre-login screen (the Unlock
    // button, "Checking...", "Wrong PIN") stay English forever,
    // regardless of what's in RU above -- the retry loop below kept
    // firing every 500ms but could never succeed before login, and
    // by the time it finally could, the screen it needed to translate
    // was already gone. A cached guess from a PREVIOUS successful
    // unlock (see the two localStorage lines in
    // applyRussianIfSelected() above) sidesteps that entirely: applied
    // synchronously, before the first paint, no server round-trip
    // needed. Only ever wrong on the very first visit ever (nothing
    // cached yet) or right after switching the language on some OTHER
    // session -- both self-correct the moment the authenticated check
    // below succeeds post-login.
    try {
      if (localStorage.getItem("kr_lang") === "russian") {
        document.documentElement.lang = "ru";
        translate(document.body);
        startObserver();
      }
    } catch (_) {
      // Private browsing / storage disabled -- falls through to the
      // authenticated-only path below, same as before this existed.
    }

    retryTimer = setInterval(async function () {
      if (await applyRussianIfSelected()) {
        clearInterval(retryTimer);
        retryTimer = null;
      }
    }, 500);

    // Also try immediately when the page is already authenticated.
    applyRussianIfSelected();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", startLocalization);
  } else {
    startLocalization();
  }
})();
</script>)JS";

} // namespace web
