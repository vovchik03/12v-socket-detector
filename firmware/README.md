# Firmware (ESP32 DevKit, PlatformIO + Arduino)

## Збірка і прошивка

```bash
cp include/secrets.example.h include/secrets.h   # заповнити Wi-Fi / MQTT
pio run                 # збірка
pio run -t upload       # прошивка
pio device monitor      # серійний лог, 115200
```

Без `include/secrets.h` прошивка збирається з плейсхолдерами з `secrets.example.h` (буде warning).

## Структура

| Файл | Що робить |
|---|---|
| `include/config.h` | піни, таблиця входів, пороги, інтервали |
| `src/power_monitor.*` | АЦП → фільтр → гістерезис → антидребезг → події входів, визначення джерела |
| `src/output_control.*` | вихід 12V з failsafe (off завжди тимчасовий) |
| `src/mqtt_link.*` | Wi-Fi + MQTT, LWT `offline`, реконект з backoff, підписка на `cmd/#` |
| `src/event_queue.h` | черга подій, поки немає зв'язку |
| `src/main.cpp` | логіка: події, команди, state, heartbeat, LED |

## Піни (ESP32 DevKit)

| GPIO | Призначення |
|---|---|
| 34 (ADC1) | напруга Main, дільник 100k/22k |
| 35 (ADC1) | напруга павера, дільник 100k/22k |
| 25 | керування MOSFET виходу (HIGH = увімкнено) |
| 2 | LED статусу: горить = MQTT, повільно блимає = лише Wi-Fi, швидко = немає Wi-Fi |

## Тест без заліза

Локальний брокер (наприклад, Mosquitto на Pi), у `secrets.h`: `MQTT_PORT 1883`, `MQTT_USE_TLS 0`.

```bash
mosquitto_sub -h <broker> -v -t 'psd/#'
mosquitto_pub -h <broker> -t 'psd/<device_id>/cmd/out' -m 'off:30'
```
