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
    "Recovery Codes": "Коды восстановления",
    "Import from file": "Импорт из файла",
    "Save": "Сохранить",
    "Cancel": "Отмена",
    "Set / Replace": "Задать / заменить",
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
    "Access Point": "Точка доступа",
    "STA SSID": "SSID станции",
    "STA Password": "Пароль станции",
    "AP SSID": "SSID точки доступа",
    "AP Password": "Пароль точки доступа",
    "Save Wi-Fi": "Сохранить Wi-Fi",
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
    "No PIN configured yet -- set one up on the device first": "PIN ещё не настроен — сначала задайте его на устройстве",
    "Too many failed attempts -- WiFi disabled": "Слишком много неудачных попыток — Wi-Fi отключён",
    "Missing 'pin' field": "Отсутствует поле PIN",
    "Invalid JSON": "Недопустимый JSON",
    "Failed to save": "Не удалось сохранить",
    "Saved": "Сохранено",
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
    "Unauthorized": "Не авторизован"
  };

  const ATTRS = ["placeholder", "title", "aria-label"];
  let observer = null;
  let retryTimer = null;

  function translateTextNode(node) {
    const text = node.nodeValue;
    if (!text || !text.trim()) return;
    const trimmed = text.trim();
    if (RU[trimmed]) {
      node.nodeValue = text.replace(trimmed, RU[trimmed]);
      return;
    }

    let out = text;
    out = out.replace(/^Firmware:\s*/, RU["Firmware"] + ": ");
    out = out.replace(/^Device:\s*/, RU["Device"] + ": ");
    out = out.replace(/^Flash Encryption:\s*/, RU["Flash Encryption"] + ": ");
    out = out.replace(/^Free heap:\s*/, RU["Free heap"] + ": ");
    out = out.replace(/^Internal storage:\s*/, RU["Internal storage"] + ": ");
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
        document.documentElement.lang = "ru";
        translate(document.body);
        startObserver();
        return true;
      }
      // Settings were read successfully and the language is not Russian.
      // Stop polling; there is no reason to keep hitting the API.
      return true;
    } catch (_) {
      // Authentication/network failure: retry later.
    }
    return false;
  }

  function startLocalization() {
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
