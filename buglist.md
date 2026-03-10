# Bug List

## BUG-001: Hard freeze при reactive RGB эффектах + Bluetooth

**Статус:** Найден root cause, есть рабочий фикс, не закоммичен

**Симптомы:**
- Клавиатура полностью зависает (hard freeze, требует перетыкание питания)
- Происходит при использовании reactive RGB эффектов (splash, solid_reactive_multiwide и т.д.)
- Часто совпадает с BT blink анимацией (подключение/отключение/переключение профиля)
- Чем активнее печать — тем выше шанс

**Root cause:** Race condition в `drivers/bluetooth/iton_bt.c`

BT callbacks (`iton_bt_connection_successful`, `iton_bt_disconnected` и т.д.) вызываются из PAL ISR (GPIO прерывание на `ITON_BT_INT_LINE`). Оригинальный `iton_bt_send_ack()` прямо из ISR:
1. Пишет `ITON_BT_IRQ_LINE` HIGH
2. Заполняет общий `iton_bt_buffer[16]`
3. Запускает SPI send через `spiStartSendI`

Одновременно main loop при каждом нажатии клавиши вызывает `iton_bt_send_keyboard()` -> `iton_bt_send()`, который использует тот же `iton_bt_buffer` и тот же SPI. Гонка на буфере и SPI периферии приводит к повреждению состояния SPI драйвера -> IRQ pin застревает HIGH навсегда -> `while (readPin(IRQ))` в main loop крутится бесконечно -> hard freeze.

**Файлы:**
- `drivers/bluetooth/iton_bt.c` — race condition между ISR (`iton_bt_send_ack`) и main loop (`iton_bt_send`/`iton_bt_send2`)
- `drivers/bluetooth/iton_bt.h` — декларация `iton_bt_task()`
- `keyboards/keychron/k3/rgb/v2/optical/optical.c` — вызов `iton_bt_task()` из `housekeeping_task_kb()`
- `keyboards/keychron/k5se/rgb/optical/optical.c` — то же

**Рабочий фикс (проверен на железе):**

1. **ACK через ring buffer очередь** — `iton_bt_send_ack()` не трогает SPI/IRQ/буфер из ISR, а кладет b1, b2 в volatile ring buffer (4 слота). Новая функция `iton_bt_task()` вызывается из `housekeeping_task_kb()` и отправляет все накопленные ACK через обычный `iton_bt_send2()` из main loop контекста. Гонки больше нет.

2. **Timeout на busy-wait (опционально)** — `while(readPin(IRQ))` заменяется на `for (volatile i < 50000)`. Страховка: если IRQ застрял, main loop дропает пакет вместо вечного зависания. При наличии фикса #1 не обязателен, но рекомендуется.

**Нерешенная проблема:**

`batt_wake_mcu` и `batt_unknown` ACK-и тоже уходят через очередь с задержкой. BT модуль при загрузке шлет `wake_mcu` и, вероятно, ожидает `wake_ack` в жестком тайминге. Задержка через main loop может триггерить ресет BT модуля -> сброс запаренных устройств.

**Предложенное решение для wake_ack:** Отправлять `wake_ack` и `unknown_ack` напрямую из ISR (оригинальный код), а `connect_ack` и `disconnect_ack` через очередь. При загрузке нет SPI-трафика от main loop, поэтому гонки при прямой ISR-отправке wake/unknown не возникает.

**Что пробовали и не сработало:**
1. Очередь с 1 слотом — ACK-и терялись при быстром переключении профилей, профили переставали переключаться
2. Отдельный `iton_bt_ack_buf[3]` + `spiStartSendI` из ISR + `chSysLock`/`spiStartSendI` в main loop — ломало BT подключение (возможно из-за замены `spiStartSend` на `spiStartSendI` в main loop)
3. Отдельный буфер без chSysLock — тоже ломало BT

**Влияние фикса на ресурсы:**
- Flash: -236 байт (убрали ISR SPI inline из каждого callback)
- RAM: +10 байт (ring buffer: b1[4] + b2[4] + head + count)
- CPU: ~0 (проверка `count > 0` раз в housekeeping цикл)
