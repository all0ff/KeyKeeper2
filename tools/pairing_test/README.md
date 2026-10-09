# Тест сопряжения по UART (этап 3b)

Одна и та же прошивка на обе платы (провода как в `tools/uart_link_test`). Нажмите BOOT на ОДНОЙ плате:
она проведёт с другой Noise XX сопряжение, обе покажут одинаковый 6-значный код и обменяются
зашифрованными HELLO / HELLO_ACK.

Заодно проверяется то, что нельзя проверить на ПК:
- собирается ли `components/kkproto` с mbedTLS/PSA вашего ESP-IDF и проходит ли `kk::crypto::selftest()`;
- сколько занимают X25519 и всё сопряжение на ESP32-S3.

## Сборка и прошивка (на каждой плате)

    cd tools/pairing_test
    idf.py set-target esp32s3
    idf.py build flash monitor


## Что должно быть в логе

    kkproto self-test: PASS (... ms)
    X25519: key generation ... ms, shared secret ... ms
    This board's key starts ...
    Ready. Press BOOT on ONE board ...
    (после BOOT)
    PAIRING CODE:  123 456     <- одинаковый на обеих платах
    Session works (encrypted HELLO/HELLO_ACK exchanged). Other board's key starts ...
    Pairing took ... ms

Ключ «Other board's key» на одной плате должен совпасть с «This board's key» на другой.

## Тесты на ПК

    bash tools/pairing_test/test_host/run_host_tests.sh
    bash tools/uart_link_test/test_host/run_host_tests.sh

Тест не трогает раздел хранилища; после него прошейте основную прошивку: `idf.py flash` в корне репозитория.
