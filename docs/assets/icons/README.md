# KeyKeeper2 Icon Set

Version: 1.0.0

Status: APPROVED

---

# Overview

KeyKeeper2 использует собственный монохромный набор пиктограмм.

Использование сторонних библиотек (Material Icons, Font Awesome, Emoji) не допускается.

Причины:

- единый визуальный стиль;
- высокая читаемость на дисплее 172×320;
- минимальный размер Flash;
- простая перекраска средствами LVGL;
- независимость от сторонних лицензий.

---

# Design Rules

Все пиктограммы имеют единый стиль.

Размер мастер-иконок:

16 × 16 px

Толщина линий:

2 px

Тип:

Monochrome

Все элементы рисуются только заливкой.

Градиенты не используются.

Тени не используются.

---

# Colors

Цвет задаётся средствами LVGL.

Иконка всегда хранится как монохромная.

Рекомендуемые цвета:

Normal      White

Accent      Green

Disabled    Gray

Warning     Yellow

Error       Red

---

# Naming

Все иконки имеют префикс IC_.

Пример:

IC_USB

IC_LOCK

IC_WIFI

IC_PASSWORD

---

# Current Icon Set

System

IC_USB

IC_LOCK

IC_UNLOCK

IC_WIFI

IC_AP

IC_SD

IC_FLASH

Account

IC_URL

IC_USER

IC_PASSWORD

IC_OTP

IC_STAR

IC_FOLDER

Actions

IC_ADD

IC_EDIT

IC_DELETE

IC_SAVE

IC_BACKUP

IC_IMPORT

IC_EXPORT

Interface

IC_SETTINGS

IC_SEARCH

IC_OK

IC_BACK

IC_WARNING

IC_ERROR

---

# Usage

Все иконки используются только через ThemeManager.

GUI не должен напрямую обращаться к файлам ресурсов.

---

# Future

В дальнейшем допускается добавление новых пиктограмм.

При этом существующие идентификаторы изменяться не должны.

Это гарантирует совместимость между версиями.