# Питание от батареи и экономия энергии

## Подключение (плата Waveshare ESP32-S3-LCD-1.47B, разъёма аккумулятора на ней нет)

Литий-ионный/Li-Po аккумулятор 1S **со встроенной защитой** → модуль заряда и защиты (TP4056 + DW01 и т.п.) →
выход модуля `OUT+` на пин **5V** платы, `OUT−` на **GND**. На пин **3V3** батарею подключать **нельзя**
(это выход стабилизатора, 4,2 В его убьёт). Заряжать от USB только через модуль, а не через USB платы,
пока выход модуля подключён к пину 5V (иначе два источника питания работают друг на друга), либо поставить диод
Шоттки последовательно в выход модуля.

### Делитель для измерения заряда → GPIO8 (ADC1)

```
батарея (+) ──[220 кОм]──┬── GPIO8
                          │
                       [220 кОм]    + 100 нФ между GPIO8 и GND
                          │
батарея (−) ─────────────┴── GND
```
Коэффициент 2:1; при 4,2 В на пине 2,1 В. Постоянный ток делителя около 10 мкА. Подключать «+» делителя к
батарее (после защиты), не к пину 3V3.

### Вход CHRG модуля TP4056 → GPIO9

Выход с открытым стоком, активный низкий уровень. Внутренняя подтяжка к 3,3 В включена программно.
- **0 В** (LOW) — идёт зарядка;
- **3,3 В** (HIGH, вывод «висит») — зарядка закончена либо зарядное устройство не подключено.
Подтягивать к 5 В нельзя. Вывод STDBY не используется.

## Настройки (Настройки → Экономия энергии)

См. `components/energy/include/energy/energy.hpp`. Для снижения частоты CPU нужен `CONFIG_PM_ENABLE=y`
(есть в `sdkconfig.defaults`; в уже собранном проекте удалите `sdkconfig` или включите в `idf.py menuconfig`:
Component config → Power Management → Support for power management).

## Замеры (измерил пользователь, 11 октября 2026)

| Состояние | Ток |
|---|---|
| экран погашен, радио выключено | 76 мА |
| экран погашен, Wi-Fi клиент | 76–90 мА |
| экран погашен, радио включено | 80 мА |
| Wi-Fi клиент, экран горит | 150–180 мА |
| экран горит, радио выключено | 150–200 мА |

## Light sleep (experimental)

Settings -> Energy saving -> **Sleep (exp.)**, off by default. The MCU light-sleeps (RAM kept, wakes in under a
millisecond) only when ALL of these hold:

- no USB host is mounted (a computer on the cable keeps the device awake as before),
- the screen is off (display timeout),
- Wi-Fi is stopped (its "off after" timer fired, or Wi-Fi mode is off),
- the dongle link is paused (its "off after" timer fired) -- the BLE stack is then shut down completely, because a
  running BLE controller forbids light sleep. The first Print afterwards starts it again (about one second extra).

While asleep the input task polls the knob and buttons every 20 ms (the chip sleeps in between), LVGL runs at 10 Hz
without its tick timer, the energy task ticks once a second. The first button press or knob turn wakes the screen.
After a sleep of more than 3 s without a USB host the firmware detaches and attaches USB once, so a computer that
was plugged in during the sleep enumerates the device.

Build requirements (in `sdkconfig.defaults`): `CONFIG_PM_ENABLE=y`, `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`; delete
`sdkconfig` once so they are picked up. Log lines: `energy: entering light sleep ...`, `light sleep over after N s`,
`blelink: BLE stack shut down`, `wireless: BLE stack is back`.

Measure with the lab supply on the 5V pin and no data cable. Expected: a few mA to a few tens of mA instead of 49 mA.
