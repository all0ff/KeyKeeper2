# ARCHITECTURE.md

# KeyKeeper2 Architecture Specification

Version: 2.1.0

Status: Draft

---

# Purpose

Данный документ определяет архитектуру проекта **KeyKeeper2**.

Он является основным архитектурным документом проекта и описывает:

- общую структуру системы;
- взаимодействие компонентов;
- программные слои;
- основные сервисы;
- архитектурные принципы;
- правила разработки новых компонентов.

Настоящий документ является обязательным для всех участников проекта и служит основой для реализации программного обеспечения.

---

# Design Philosophy

Архитектура проекта построена на следующих принципах:

- Documentation First;
- Architecture Before Code;
- Component-Based Architecture;
- Event-Driven Architecture;
- Clean Architecture;
- SOLID;
- DRY;
- KISS;
- Security First.

Любые изменения архитектуры должны сопровождаться обновлением документации и соответствующего ADR.

---

# Design Goals

Архитектура должна обеспечивать:

- модульность;
- независимость компонентов;
- расширяемость;
- тестируемость;
- повторное использование кода;
- минимальную связанность;
- простоту сопровождения;
- переносимость;
- безопасность;
- высокую производительность.

---

# System Overview

KeyKeeper2 представляет собой автономный менеджер паролей на базе ESP32-S3.

Основные возможности системы:

- безопасное хранение учетных записей;
- хранение TOTP/HOTP;
- USB HID Keyboard;
- локальный Web UI;
- Wi-Fi Access Point;
- Captive Portal;
- резервное копирование;
- импорт и экспорт базы данных;
- современный графический интерфейс LVGL 9;
- поддержка нескольких языков.

---

# High-Level Architecture

Архитектура разделена на независимые уровни.

```text
Application
        │
        ▼
GUI Layer
        │
        ▼
Component Layer (security:: / vault:: / storage:: / usb:: / wifi::
                  / web:: / settings:: -- see Component Architecture
                  below; each is the service boundary on its own,
                  there is no separate wrapper layer above them)
        │
        ▼
Hardware Abstraction Layer (BSP)
        │
        ▼
ESP-IDF
```

Earlier revisions of this document showed "Service Layer" and
"Security Layer" as two separate layers here, with Security
positioned *below* a Service Layer that wrapped it in classes like
`SecurityService`/`VaultService`. That wrapper layer was never
actually built -- every component (`security::`, `vault::`,
`storage::`, `usb::`, `wifi::`, `web::`, `settings::`) exposes its own
public API as free functions directly, and GUI calls those
directly (confirmed via `components/ui`'s own `CMakeLists.txt`
`REQUIRES`, which lists these components, not any `*_service`
wrapper). This is the actual, current architecture, not a
simplification pending future work -- see the **Service Layer**
section further below for the full reasoning.

Каждый уровень взаимодействует только с соседним уровнем.

Обращение через несколько уровней запрещается.

---

# Component Architecture

Проект разделён на независимые компоненты ESP-IDF.

```text
components/

app_system/
bsp/
display/
event_bus/
imu/
input/
interfaces/
password_gen/
power/
rtc_time/
security/
settings/
storage/
totp/
ui/
usb/
vault/
web/
wifi/
```

Каждый компонент имеет собственную область ответственности и публичный API.

**Note:** this list used to show `app/`, `gui/`, `system/`, and was
missing half the project's real components (`ui`, `event_bus`,
`power`, `rtc_time`, `totp`, `display`, `input`, `interfaces`, `imu`,
`password_gen`) entirely. `app/` and `system/` were early names for
what's now the single `app_system` component; `gui/` is a leftover,
unbuilt skeleton (excluded from the build -- see its own
`components/gui/README.md` for why, and why `ui/` is the component
actually doing this work). This list is now the real
`components/` directory, not yet the detailed per-component write-ups
below it (`# Component Overview`), which still describe the old list
-- a larger pass, not done as part of this one-line fix.

---

# Component Overview

## app

Назначение:

Точка входа приложения.

Отвечает за:

- инициализацию системы;
- запуск сервисов;
- регистрацию компонентов;
- последовательность запуска.

---

## bsp

Board Support Package.

Отвечает за работу оборудования:

- дисплей;
- подсветка;
- энкодер;
- кнопки;
- USB;
- SPI;
- I²C;
- microSD;
- Flash.

GUI не взаимодействует с BSP напрямую.

---

## gui

Отвечает за пользовательский интерфейс.

Использует:

- LVGL 9;
- ThemeManager;
- ScreenManager;
- Localization.

GUI содержит только отображение данных.

Бизнес-логика в GUI запрещена.

---

## storage

Отвечает исключительно за хранение данных.

Поддерживает:

- LittleFS;
- microSD;
- Backup;
- Restore;
- Import;
- Export.

Не содержит бизнес-логики.

---

## vault

Подсистема хранения учетных записей.

Отвечает за:

- чтение;
- запись;
- поиск;
- категории;
- избранное;
- OTP;
- работу с метаданными.

Vault не отвечает за безопасность.

Все проверки доступа выполняются через Security.

---

## security

Централизованная подсистема безопасности проекта.

Компонент отвечает за:

- проверку PIN-кода;
- блокировку устройства;
- разблокировку устройства;
- Auto Lock;
- управление пользовательской сессией;
- проверку разрешений;
- публикацию событий безопасности;
- подготовку архитектуры к AES-256;
- подготовку к Secure Element.

Все остальные компоненты используют Security только через security::.

---

## usb

Подсистема USB HID.

Отвечает за:

- печать URL;
- печать логина;
- печать пароля;
- печать OTP;
- очередь передачи;
- обработку задержек;
- поддержку TinyUSB.

---

## wifi

Подсистема Wi-Fi.

Отвечает за:

- Access Point;
- Station;
- Captive Portal;
- сетевые настройки;
- управление подключениями.

---

## web

Локальный Web UI.

Отвечает за:

- REST API;
- Web Interface;
- управление настройками;
- резервное копирование;
- импорт;
- экспорт.

---

## settings

Хранение настроек приложения.

Использует NVS.

Настройки доступны только через settings::.

---

## system

Инфраструктурный компонент.

Содержит:

- EventBus;
- Logger;
- Time;
- Utility Classes;
- Common Types.

System не содержит бизнес-логики приложения.

---

# Service Layer

**Note on terminology:** earlier revisions of this document described
a separate "Service Layer" of wrapper classes (`StorageService`,
`VaultService`, `SecurityService`, ...) sitting between GUI and each
component's own implementation. That layer was never actually built.
What exists instead, and what this section now describes, is simpler:
each component's own public namespace (declared in its `include/`
headers) *is* the service boundary -- there is no separate wrapper
class above it. `components/ui`'s own `CMakeLists.txt` confirms this
directly: its `REQUIRES` lists `security`, `vault`, `storage`, `usb`,
`wifi`, `web`, `settings` by name, and GUI code calls their free
functions (`vault::repository::load(...)`,
`security::permission::check(...)`, and so on) straight from screen
code, with no intermediate object to construct or own.

Each component's public API is its own service:

```text
storage::

vault::

security::

usb::

wifi::

web::

settings::
```

Theming and localization (what earlier revisions called
`ThemeService`/`LanguageService`) are not separate components --
`ui::theme` and `ui::i18n` are part of the `ui` component itself,
since both exist purely to serve GUI rendering and have no reason to
be reachable from, say, `web::` or `usb::`.

Components communicate with each other either through EventBus
(`event_bus::publish()`/`subscribe()` -- for loosely-coupled
notifications, e.g. `security::lock` publishing `DeviceLocked` without
needing to know who's listening) or by calling another component's
public API directly where a direct dependency already makes sense
(e.g. `usb::print_field()` calling `security::permission::check()`
before typing a password -- see the Security Architecture section
below). There is no rule against a component depending on another
component's public header; the thing actually enforced is narrower
and more useful: a component may only reach another component through
that component's own declared public API (its `include/` headers),
never by reaching past it into internals that aren't exported there.

---

# Security Layer

Security является самостоятельным архитектурным уровнем.

```text
GUI

↓

security::

↓

Permission Check

↓

Vault / USB / Web
```

Любая операция, связанная с конфиденциальными данными, проходит проверку через security::.

К защищённым операциям относятся:

- печать пароля;
- печать OTP;
- экспорт базы;
- изменение PIN;
- изменение настроек безопасности;
- доступ к Web UI.

---

# Component Dependencies

Зависимости между компонентами построены по принципу минимальной связанности.

```text
                 GUI
                  │
      ┌───────────┼────────────┐
      ▼           ▼            ▼
  Security      Vault        Web
      │            │            │
      │            ▼            │
      │        Storage          │
      │                         │
      ├────────► USB            │
      │                         │
      ├────────► Settings       │
      │                         │
      └────────► EventBus ◄─────┘
```

Компоненты взаимодействуют только через публичные интерфейсы или EventBus.

---

# Internal Layers

Внутри каждого компонента рекомендуется использовать одинаковую структуру.

```text
Public API (free functions in the component's own namespace,
            declared in its include/ headers)

↓

Manager (one per sub-area of responsibility -- e.g. security::'s own
         pin_manager.cpp / lock_manager.cpp / session_manager.cpp /
         permission_manager.cpp, each exposing its own nested
         namespace: security::pin::, security::lock::, and so on)

↓

ESP-IDF
```

**Note on "Service" here:** this used to show a separate `Service`
layer between `Public API` and `Manager`. Dropped -- see the *Service
Layer* section above for the full reasoning; within one component,
the "Public API" row above already *is* that component's own service
boundary, not a distinct layer sitting above a `Manager` layer.
`security::lock::unlock()`, for instance, is simultaneously the public
API call a caller makes *and* a thin function inside
`lock_manager.cpp` doing the actual work -- there's no separate
wrapper for it to pass through first.

Каждый слой имеет единственную область ответственности.

---

# Event-Driven Architecture

Взаимодействие компонентов строится на основе событий.

```text
Publisher

↓

EventBus

↓

Subscribers
```

Преимущества данного подхода:

- слабая связанность компонентов;
- независимое развитие подсистем;
- простое добавление новых возможностей;
- минимальное количество прямых зависимостей.

Описание EventBus приведено в документе **EVENTS.md**.

---

# Application Startup

Последовательность запуска системы должна быть фиксированной.

```text
Power On

↓

Bootloader

↓

ESP-IDF

↓

Board Support Package

↓

Settings

↓

Storage

↓

Security

↓

USB

↓

Wi-Fi

↓

GUI

↓

Application
```

Каждый следующий этап запускается только после успешной инициализации предыдущего.

---

# Shutdown Sequence

При завершении работы выполняется обратная последовательность.

```text
Application

↓

GUI

↓

USB

↓

Wi-Fi

↓

Storage

↓

Settings
```

Перед выключением должны быть сохранены все изменённые данные.

---

# GUI Architecture

```text
LVGL

↓

ui::UiManager (screen stack, navigation, the recursive LVGL mutex
               every lv_* call goes through)

↓

Screen classes (LockScreen, AccountViewScreen, ... -- one per
                screen; each calls the component namespaces below
                directly from its own on_input()/render() -- there is
                no separate Presenter object in between)

↓

security:: / vault:: / settings:: / usb:: / wifi:: / web:: / ...

↓

EventBus (for the loosely-coupled notifications -- see the Service
          Layer section above)
```

**Note:** this used to show a separate `Presenters` layer between
`ScreenManager` and `Services` ("GUI полностью отделён от
бизнес-логики"). Dropped for the same reason as the Service Layer
above -- every concrete screen class calls the relevant component's
own public API directly from its own code, with no separate presenter
object wrapping that call. The 21 screens under `components/ui/src/
screens/` are both the presentation AND the thing that invokes
business logic, just like `security::lock::unlock()` is both the
public API and the implementation one layer down (see Internal
Layers above).

GUI отвечает только за:

- отображение информации;
- обработку пользовательского ввода;
- генерацию событий интерфейса.

---

# Storage Architecture

Подсистема хранения разделена на два уровня.

```text
vault::

↓

StorageProvider

↓

LittleFS / microSD
```

StorageProvider инкапсулирует работу с файловой системой и позволяет в будущем заменить способ хранения данных без изменения остальной архитектуры.

Подробное описание приведено в документе **STORAGE.md**.

---

# Security Architecture

Подсистема безопасности является отдельным компонентом проекта.

Внутренняя структура:

```text
security::

├── security::pin

├── security::lock

├── security::session

└── security::permission
```

---

### security::

Центральная точка доступа ко всей подсистеме безопасности.

Отвечает за:

- авторизацию;
- управление состоянием устройства;
- координацию внутренних менеджеров;
- публикацию событий безопасности.

---

### security::pin

Отвечает за:

- хранение PIN;
- проверку PIN;
- изменение PIN;
- контроль неверных попыток.

---

### security::lock

Отвечает за:

- блокировку устройства;
- разблокировку;
- Auto Lock;
- состояние Locked/Unlocked.

---

### security::session

Управляет пользовательской сессией.

В дальнейшем используется:

- Web UI;
- OTA;
- многопользовательским режимом.

---

### security::permission

Определяет возможность выполнения защищённых операций.

Например:

- Print Password;
- Print OTP;
- Export Vault;
- Change PIN;
- Web Login.

---

# USB Architecture

Подсистема USB построена поверх TinyUSB.

```text
usb::

↓

Print Queue

↓

TinyUSB

↓

USB HID
```

Все запросы помещаются в очередь.

Перед выполнением защищённых действий usb:: обращается к security::.

---

# Wi-Fi Architecture

Wi-Fi полностью изолирован от GUI.

```text
wifi::

↓

ESP Wi-Fi

↓

TCP/IP

↓

WebServer
```

Все сетевые события публикуются через EventBus.

---

# Web Architecture

Локальный Web UI вызывает нужные компоненты напрямую, как и GUI.

```text
Browser

↓

REST API

↓

web::

↓

vault::

↓

Storage
```

Перед выполнением защищённых операций производится проверка через security::.

---

# Settings Architecture

Все настройки приложения централизованы.

```text
settings::

↓

NVS
```

Прямой доступ к NVS из других компонентов запрещён.

Настройки изменяются только через settings::.

---

# Localization

Все текстовые строки приложения хранятся централизованно.

Поддерживаемые языки:

- English;
- Русский.

Добавление новых языков не требует изменения GUI.

---

# Theme System

Используется единая система тем оформления.

Основная тема проекта:

- Dark Theme;
- зелёные акценты;
- шрифты Inter;
- шрифты Montserrat;
- единый набор монохромных пиктограмм.

GUI не использует фиксированные цвета напрямую.

Все цвета предоставляются ThemeManager.

---

# Logging

Журналирование используется исключительно для диагностики.

Запрещается выводить:

- PIN;
- пароли;
- OTP;
- секретные ключи;
- содержимое Vault.

Настройки уровня журналирования определяются конфигурацией сборки.

---

# Design Rules

При разработке новых компонентов необходимо соблюдать следующие правила.

## Rule 1

GUI не содержит бизнес-логики.

---

## Rule 2

GUI не обращается напрямую к ESP-IDF.

---

## Rule 3

GUI не работает напрямую с файловой системой.

---

## Rule 4

Все операции хранения выполняются через storage::.

---

## Rule 5

Все операции с учетными записями выполняются через vault::.

---

## Rule 6

Все операции, связанные с безопасностью, выполняются через security::.

---

## Rule 7

USB не имеет доступа к данным Vault без проверки security::.

---

## Rule 8

Web UI не имеет доступа к защищённым данным без проверки security::.

---

## Rule 9

Все изменения настроек выполняются через settings::.

---

## Rule 10

Компоненты взаимодействуют между собой через публичные интерфейсы или EventBus.

---

## Rule 11

Компоненты не должны иметь циклических зависимостей.

---

## Rule 12

Каждый компонент отвечает только за одну область ответственности.

---

# Component Lifecycle

Каждый компонент проходит одинаковый жизненный цикл.

```text
Create

↓

Initialize

↓

Start

↓

Run

↓

Stop

↓

Destroy
```

Инициализация должна быть идемпотентной.

Повторный вызов Initialize не должен приводить к ошибкам.

---

# Error Handling

Каждый компонент обязан:

- корректно обрабатывать ошибки;
- не завершать работу системы аварийно;
- возвращать информативный статус выполнения;
- публиковать события об ошибках через EventBus при необходимости.

Критические ошибки должны фиксироваться в журнале.

---

# Thread Safety

Все сервисы должны учитывать многозадачность FreeRTOS.

Рекомендуется:

- избегать совместного изменения данных;
- минимизировать использование блокировок;
- применять очереди сообщений для обмена между задачами;
- использовать EventBus для уведомлений.

Потокобезопасность должна учитываться при проектировании публичного API.

---

# Memory Management

Проект ориентирован на работу на ESP32-S3.

Поэтому необходимо:

- минимизировать динамические выделения памяти;
- использовать RAII;
- применять `std::unique_ptr`;
- избегать копирования крупных объектов;
- переиспользовать буферы при длительной работе.

Все операции должны учитывать ограничения Flash и RAM.

---

# Extensibility

Архитектура должна позволять добавление новых функций без изменения существующих компонентов.

В дальнейшем планируется поддержка:

- AES-256 Vault Encryption;
- Secure Element;
- NFC;
- Bluetooth;
- OTA;
- пользовательских USB-макросов;
- нескольких Vault;
- многопользовательского режима;
- дополнительных языков;
- новых тем оформления.

Расширение проекта должно выполняться путём добавления новых компонентов или сервисов, а не изменения существующих архитектурных уровней.

---

# Documentation Rules

Любое изменение архитектуры должно сопровождаться обновлением документации.

При необходимости обновляются:

- REQUIREMENTS.md;
- ARCHITECTURE.md;
- SOFTWARE.md;
- GUI.md;
- STORAGE.md;
- SECURITY.md;
- USB.md;
- WEB.md;
- EVENTS.md;
- BUILD.md;
- ROADMAP.md;
- соответствующий ADR.

Документация является частью исходного кода проекта.

---

# Related Documents

Архитектура проекта тесно связана со следующими документами:

- README.md
- REQUIREMENTS.md
- SOFTWARE.md
- GUI.md
- STORAGE.md
- SECURITY.md
- USB.md
- WEB.md
- EVENTS.md
- BUILD.md
- CODING_STYLE.md
- ROADMAP.md
- AI_PROMPT.md
- ADR

---

# Summary

Архитектура **KeyKeeper2** построена на принципах **Documentation First**, **Component-Based Architecture**, **Event-Driven Architecture** и **Security First**. Система разделена на независимые компоненты с чётко определёнными зонами ответственности, взаимодействующими через публичные интерфейсы и EventBus.

Выделение компонента **Security** в самостоятельную подсистему обеспечивает централизованную проверку доступа, управление блокировкой устройства и единый механизм защиты данных, не нарушая принцип единой ответственности (SRP).

Такая архитектура обеспечивает:

- простоту сопровождения;
- минимальную связанность компонентов;
- высокую тестируемость;
- возможность масштабирования;
- готовность к внедрению новых функций без изменения существующей структуры проекта.

Документ **ARCHITECTURE.md** является основой для реализации программного обеспечения и должен оставаться согласованным со всеми архитектурными документами проекта.