# ble_link_test — проверка BLE-связи между двумя платами (этап 5a)

Отдельный проект ESP-IDF, НЕ часть прошивки KeyKeeper2. Одна прошивка, две роли:

* **peripheral** — играет донгл: рекламируется, принимает сообщения и отправляет их обратно (эхо);
* **central** — играет хранилище: сканирует, подключается, согласует MTU, подписывается, шлёт сообщения
  длиной 8…250 байт (с фрагментацией), проверяет эхо, печатает задержку; при разрыве переподключается сам.

Безопасность на уровне BLE-канала не используется (без bonding): шифрование даёт наш Noise поверх.

## Сборка (две платы — две сборки, из папки tools/ble_link_test)

Плата A (периферия, COM-порт A):

    idf.py -B build_p -D SDKCONFIG=sdkconfig_p -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.peripheral" set-target esp32s3
    idf.py -B build_p -D SDKCONFIG=sdkconfig_p -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.peripheral" -p COMA flash monitor

Плата B (центральная, COM-порт B):

    idf.py -B build_c -D SDKCONFIG=sdkconfig_c -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.central" set-target esp32s3
    idf.py -B build_c -D SDKCONFIG=sdkconfig_c -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.central" -p COMB flash monitor

## Что смотреть

Центральная каждые 40 сообщений пишет: `sent ok bad lost rtt min/avg/max mtu`.
Хорошо: `bad=0 lost=0`, `mtu 247`, RTT порядка 30–100 мс (зависит от интервала соединения).
Проверка переподключения: сбросить (RESET) любую из плат — через несколько секунд в логе `LINK UP`, счётчики идут дальше.

## Хост-тест фрагментатора

    cd test_host && g++ -std=c++17 -Wall -Wextra -I../main test_frag.cpp -o test_frag && ./test_frag
