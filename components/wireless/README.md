# wireless

The vault's side of the radio-dongle link: typing through the user's own USB dongle instead of the vault's
USB cable.

```
Print action ──> usb::Service ──> usb::TypeEngine ──> OutputSink
                                                        ├─ cable   (default)
                                                        └─ DongleSink ──> wireless::Link ──UART/BLE──> dongle ──USB HID──> PC
```

* `wireless.hpp` -- the API used by the UI (`Settings -> USB -> Wireless typing`).
* `src/wireless.cpp` -- built with `CONFIG_KEYKEEPER_KKPROTO`; `src/wireless_off.cpp` is a stub otherwise.
* State lives in NVS namespace `kklink`: `sk` (this device's X25519 key, plain until flash encryption is on),
  `peer` (the paired dongle's key), `en` (the switch, default off).

## Threads

One task (`kk_link`, 10 KB stack) owns the kkproto `Endpoint` and the UART: it reads, ticks, applies commands,
sends requests. Everything else talks to it through a mutex-protected command word and status snapshot.
The typing task (`usb_type`) calls `DongleChannel::type_keys()`: it queues the batch, wakes on a semaphore when
the link task has the dongle's Result (or the link is lost), or gives up after the batch time + 3 s.

## Wire

UART1, GPIO10 (TX) / GPIO11 (RX) of the expansion header, 460800 baud, framing `A5 5A | LEN | PAYLOAD | CRC16`
(`tools/uart_link_test/main/link_frame.hpp`, shared with the dongle). Curve25519 is slow on this chip, so the
vault waits 4 s before it retries a handshake step (see `Params` in `Link::init`).

## Tested

Run on a PC against the real dongle application (two processes over pipes, FreeRTOS replaced by pthreads):
pairing with code confirmation, typing a Cyrillic text, off / on, forget, reconnect after a restart,
the dongle disappearing in the middle of a text ("Dongle did not answer"), typing with the switch off.
The firmware build itself is verified on the device only.
