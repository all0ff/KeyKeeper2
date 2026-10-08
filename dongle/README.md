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

## Проверка
1. Донгл: BOOT (окно 60 с). Симулятор: BOOT. На обоих экранах одинаковый код.
2. BOOT на обоих — «Paired!», затем «Connected».
3. Сбросьте любую плату — после загрузки снова «Connected» без повторного сопряжения.
4. Долгое BOOT на донгле — забыть; симулятор перестанет подключаться (нужно забыть и там).

Логика проверена на ПК (host-тесты `test_host/`, склейка с заглушками ESP-IDF), на железе — ещё нет. После первой сборки закоммитьте `dependencies.lock`.
