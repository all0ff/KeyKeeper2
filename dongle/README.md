# Донгл KeyKeeper2 (этап 3c)

Проект для плат Waveshare ESP32-S3-LCD-1.47B: постоянный ключ в NVS, экран, сопряжение (Noise XX, код на обоих экранах), переподключение после перезагрузки (Noise IK). Связь — UART (GPIO10 TX, GPIO11 RX, перекрёстно, общий GND, 460800).

Две одинаковые платы: одна — донгл, вторая — «симулятор хранилища» (отдельная сборка).

## Сборка (ESP-IDF PowerShell, папка `dongle`)

Донгл:
    cd dongle
    idf.py set-target esp32s3
    idf.py -p COMx flash monitor

Симулятор хранилища (другая плата, отдельная папка сборки):
    idf.py -B build_vault -D SDKCONFIG=sdkconfig_vault -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.vaultsim" set-target esp32s3
    idf.py -B build_vault -D SDKCONFIG=sdkconfig_vault -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.vaultsim" -p COMy flash monitor

(если после прошивки плата в режиме DOWNLOAD — переткните USB.)

## Кнопка BOOT

| Состояние | Короткое нажатие | Долгое (3 с) |
|---|---|---|
| не сопряжён | донгл: открыть окно сопряжения 60 с; симулятор: начать сопряжение | — |
| окно открыто / идёт сопряжение | отмена | — |
| показан код | «да, коды совпали» | «нет» |
| сопряжён | сопрячь заново | забыть сопряжение |

## Ввод через донгл (этап 4a)
Когда связь есть («Connected»), короткое нажатие BOOT на **симуляторе хранилища** набирает тестовую строку
`Test 123 Проверка!` через донгл (с переключением раскладки Alt+Shift вокруг русских букв). Донгл пока не
клавиатура: он пишет в лог каждую «нажатую» клавишу (`KEY mods=.. usage=.. hold=..`), отвечает `Result`, на
экране — «Typed N keys». Симулятор показывает «Typed 18 characters» или причину ошибки. Реальный USB HID
на донгле — следующий этап.

## Проверка
1. Донгл: BOOT (окно 60 с). Симулятор: BOOT. На обоих экранах одинаковый код.
2. BOOT на обоих — «Paired!», затем «Connected».
3. Сбросьте любую плату — после загрузки снова «Connected» без повторного сопряжения.
4. Долгое BOOT на донгле — забыть; симулятор перестанет подключаться (нужно забыть и там).

Логика проверена на ПК (host-тесты `test_host/`, склейка с заглушками ESP-IDF), на железе — ещё нет. После первой сборки закоммитьте `dependencies.lock`.

## Донгл как настоящая USB-клавиатура (этап 4c)

Сборка с `sdkconfig.hid`: USB-порт платы отдаётся TinyUSB, ПК видит составное устройство
«клавиатура + COM-порт». Лог при этом идёт в этот COM-порт (и дублируется на UART0, GPIO43/44).
Последние 8 КБ лога хранятся в памяти и выводятся, когда терминал подключится.

```
idf.py -B build_hid -D SDKCONFIG=sdkconfig_hid -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.hid" set-target esp32s3
idf.py -B build_hid -D SDKCONFIG=sdkconfig_hid -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.hid" -p COMx flash
```

Прошивка по USB дальше: зажать BOOT, воткнуть USB (режим загрузчика), прошить, переткнуть.
Обычная сборка (без `sdkconfig.hid`) остаётся прежней: клавиши только пишутся в лог.
На экране донгла в состоянии «Connected»: `USB: ready` (ПК увидел) или `USB: no PC`.


## BLE instead of the UART cable (stage 5b)

Build with the extra overlay `sdkconfig.ble` (it switches `CONFIG_DONGLE_TRANSPORT_BLE` and NimBLE on). With the USB
keyboard build:

    idf.py -B build_ble -D SDKCONFIG=sdkconfig_ble -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.hid;sdkconfig.ble" set-target esp32s3
    idf.py -B build_ble -D SDKCONFIG=sdkconfig_ble -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.hid;sdkconfig.ble" -p COMx flash

The dongle advertises by itself; BOOT opens the pairing window as before (the window flag is part of the advertising,
so the vault in pairing mode finds exactly this dongle). The UART pins are not used. The vault simulator
(`sdkconfig.vaultsim`) still talks UART only.
