# EVENTS.md

# KeyKeeper2 Event System Specification

Version: 2.0.0

Status: Draft

---

# 1. Purpose

Настоящий документ определяет архитектуру событийной системы проекта **KeyKeeper2**.

Документ описывает:

- принципы Event-Driven Architecture;
- устройство EventBus;
- структуру событий;
- взаимодействие компонентов;
- правила публикации и обработки событий.

EVENTS.md является дополнением к документам:

- ARCHITECTURE.md
- SOFTWARE.md
- SECURITY.md
- AI_PROMPT.md

---

# 2. Event-Driven Architecture

Все независимые компоненты проекта взаимодействуют через событийную модель.

Прямые зависимости между компонентами должны быть сведены к минимуму.

EventBus обеспечивает слабую связанность (Loose Coupling) между компонентами и упрощает расширение системы.

---

## Goals

Основные цели использования EventBus:

- разделение компонентов;
- уменьшение количества прямых зависимостей;
- повышение модульности;
- упрощение тестирования;
- поддержка асинхронной обработки событий.

---

## Communication Model

Типичная схема взаимодействия:

```text
Publisher

↓

EventBus

↓

Subscriber
```

Компонент, публикующий событие, не знает, кто его обработает.

Компонент, принимающий событие, не знает, кто является его источником.

---

# 3. Design Principles

Подсистема событий строится на следующих принципах:

- Loose Coupling;
- Publish / Subscribe;
- Component Isolation;
- Single Responsibility;
- Thread Safety;
- Documentation First.

---

## Component Independence

Компоненты не должны напрямую вызывать методы друг друга, если взаимодействие может быть реализовано через EventBus.

Например:

```text
GUI

↓

EventBus

↓

VaultService
```

вместо

```text
GUI

↓

VaultService
```

---

## Event Ownership

Каждое событие имеет одного источника (Publisher).

Количество подписчиков не ограничивается.

---

# 4. EventBus Overview

EventBus является центральным механизмом обмена событиями между компонентами проекта.

Он не содержит бизнес-логики и не принимает решений.

---

## Responsibilities

EventBus отвечает за:

- регистрацию подписчиков;
- публикацию событий;
- доставку событий;
- управление очередью сообщений;
- безопасную обработку событий.

---

## Non-Responsibilities

EventBus не отвечает за:

- хранение данных;
- обработку событий;
- проверку безопасности;
- выполнение бизнес-логики.

Все решения принимаются подписчиками.

---

# 5. Event Flow

Каждое событие проходит одинаковый жизненный цикл.

```text
Component

↓

Create Event

↓

Publish

↓

EventBus Queue

↓

Dispatch

↓

Subscriber

↓

Process
```

---

## Event Lifecycle

Типичный жизненный цикл события:

1. Создание события.
2. Заполнение структуры.
3. Публикация в EventBus.
4. Постановка в очередь.
5. Доставка подписчикам.
6. Обработка.
7. Удаление события.

---

# 6. Event Categories

Для удобства сопровождения события разделяются на категории.

---

## GUI Events

Используются для взаимодействия пользовательского интерфейса.

Примеры:

- ScreenChanged;
- ButtonPressed;
- EncoderRotated;
- MenuSelected.

---

## Vault Events

Используются при работе с пользовательскими данными.

Примеры:

- VaultOpened;
- VaultClosed;
- AccountCreated;
- AccountUpdated;
- AccountDeleted;
- SearchCompleted.

---

## Security Events

Используются подсистемой безопасности.

Примеры:

- SessionUnlocked;
- SessionLocked;
- PinVerified;
- PinFailed;
- AutoLockActivated.

---

## Storage Events

Используются подсистемой хранения.

Примеры:

- DatabaseLoaded;
- DatabaseSaved;
- BackupStarted;
- BackupCompleted;
- RestoreCompleted.

---

## USB Events

Используются компонентом USB.

Примеры:

- UsbConnected;
- UsbDisconnected;
- PasswordPrinted;
- UrlPrinted;
- OtpPrinted.

---

## System Events

Используются инфраструктурными сервисами.

Примеры:

- StartupCompleted;
- ShutdownRequested;
- LowMemory;
- ConfigurationChanged;
- LanguageChanged.

---

# 7. Event Structure

Все события используют единый формат.

Каждое событие должно быть компактным, понятным и независимым.

---

## Event Fields

Минимальный набор полей:

```cpp
struct Event
{
    EventType type;
    uint32_t timestamp;
};
```

При необходимости событие может содержать дополнительные данные.

---

## Event Payload

Полезная нагрузка должна содержать только данные, необходимые подписчику.

Не допускается передача крупных объектов без необходимости.

---

## Immutable Events

После публикации событие считается неизменяемым.

Подписчики не должны изменять содержимое полученного события.

---

# 8. Publishers

Любой компонент проекта может публиковать события через EventBus.

---

## Typical Publishers

Основные источники событий:

- GUI;
- VaultService;
- StorageService;
- SecurityService;
- USBService;
- WiFiService;
- WebService;
- SettingsService;
- SystemService.

---

## Publishing Rules

При публикации события компонент должен:

- создать структуру события;
- заполнить необходимые поля;
- передать событие в EventBus.

После публикации ответственность за доставку полностью переходит к EventBus.

---

## Event Frequency

Не рекомендуется публиковать большое количество однотипных событий без необходимости.

Если состояние не изменилось, повторная публикация события не требуется.

---

# 9. Subscribers

Подписчики регистрируются в EventBus и получают только интересующие их события.

---

## Subscription

Компонент может подписаться:

- на одно событие;
- на группу событий;
- на все события определённой категории.

---

## Multiple Subscribers

Одно событие может иметь несколько подписчиков.

Например:

```text
SessionUnlocked

↓

GUI

↓

USB

↓

Logger
```

Все подписчики получают одинаковое событие.

---

## Independent Processing

Каждый подписчик самостоятельно принимает решение о дальнейшей обработке.

Обработка одним подписчиком не должна влиять на остальных.

---

# 10. Event Processing

Все события должны обрабатываться максимально быстро.

---

## Processing Rules

Обработчик не должен:

- выполнять длительные операции;
- обращаться к сети;
- блокировать EventBus;
- ожидать завершения других задач.

Если требуется длительная обработка, обработчик должен инициировать отдельную задачу FreeRTOS.

---

## Event Ordering

События обрабатываются в порядке их поступления в очередь.

Приоритетная обработка допускается только при наличии отдельной очереди соответствующего уровня.

---

## Event Loss

Потеря событий недопустима для критически важных операций.

При переполнении очереди EventBus должен зарегистрировать ошибку и уведомить систему.

---

# 11. Thread Safety

EventBus должен быть безопасен при использовании из нескольких задач FreeRTOS.

---

## Synchronization

Все внутренние структуры EventBus должны быть защищены средствами синхронизации.

Допускается использование:

- очередей FreeRTOS;
- мьютексов;
- других стандартных механизмов ESP-IDF.

---

## Shared Data

События не должны содержать ссылки на изменяемые объекты с неопределённым временем жизни.

При необходимости данные должны копироваться.

---

# 12. Best Practices

При разработке новых компонентов рекомендуется придерживаться следующих правил.

---

## Use Events

Следует использовать события:

- для изменения состояния системы;
- для уведомления других компонентов;
- для завершения длительных операций;
- для взаимодействия независимых модулей.

---

## Avoid Direct Calls

Не рекомендуется напрямую вызывать методы другого компонента, если взаимодействие может быть реализовано через EventBus.

Это уменьшает связанность компонентов и облегчает сопровождение проекта.

---

## Event Naming

Названия событий должны быть:

- короткими;
- понятными;
- описывающими произошедшее действие.

Например:

- AccountCreated;
- BackupCompleted;
- SessionLocked.

Следует избегать неоднозначных названий.

---

# 13. Standard Event List

Ниже приведён перечень основных событий проекта.

---

## GUI

- ScreenChanged;
- ButtonPressed;
- EncoderRotated;
- MenuSelected;
- DialogOpened;
- DialogClosed.

---

## Vault

- VaultOpened;
- VaultClosed;
- AccountCreated;
- AccountUpdated;
- AccountDeleted;
- CategoryCreated;
- FavoriteChanged;
- SearchCompleted.

---

## Security

- SessionUnlocked;
- SessionLocked;
- PinVerified;
- PinFailed;
- AutoLockActivated;
- PermissionDenied.

---

## Storage

- DatabaseLoaded;
- DatabaseSaved;
- BackupStarted;
- BackupCompleted;
- RestoreStarted;
- RestoreCompleted;
- ImportCompleted;
- ExportCompleted.

---

## USB

- UsbConnected;
- UsbDisconnected;
- UrlPrinted;
- UsernamePrinted;
- PasswordPrinted;
- OtpPrinted.

---

## Wi-Fi / Web

- WifiStarted;
- WifiStopped;
- ClientConnected;
- ClientDisconnected;
- WebServerStarted;
- WebServerStopped;
- ApiRequestReceived.

---

## System

- StartupCompleted;
- ShutdownRequested;
- LowMemory;
- ConfigurationChanged;
- LanguageChanged;
- ThemeChanged;
- FirmwareUpdated.

---

# 14. Future Extensions

Архитектура EventBus должна предусматривать возможность дальнейшего развития.

Планируемые улучшения:

- приоритетные очереди событий;
- фильтрация по категориям;
- отложенная публикация;
- периодические события;
- журналирование событий;
- трассировка EventBus;
- удалённый мониторинг через Web UI.

Все новые возможности должны быть реализованы без нарушения существующего публичного API.

---

# 15. Summary

Подсистема **EventBus** является центральным механизмом взаимодействия компонентов проекта **KeyKeeper2** и реализует архитектурный подход **Event-Driven Architecture**.

Использование модели **Publish / Subscribe** обеспечивает слабую связанность компонентов, упрощает расширение функциональности и позволяет независимо развивать отдельные подсистемы проекта.

Все компоненты взаимодействуют через события, сохраняя чёткое разделение ответственности и следуя принципам **Component Isolation**, **Single Responsibility** и **Documentation First**, что обеспечивает масштабируемость, надёжность и удобство сопровождения программного обеспечения.