# blelink -- BLE transport between the vault and its dongle (stage 5b)

A message pipe over BLE (NimBLE). The kkproto link (`components/kkproto`) runs on top of it unchanged; the only thing
that differs from the UART cable is how a frame travels.

* dongle = GATT **peripheral**: advertises one service (RX: write without response, TX: notify) and a flag
  "pairing window open";
* vault = **central**: scans, connects, negotiates the MTU (247 -> 244 bytes per packet), subscribes to TX;
* a message (one kkproto frame, up to 256 bytes) is cut into ATT-sized fragments (`frag.hpp`, 2-byte header) and
  reassembled on the other side. `frag.hpp` is the same code as `tools/ble_link_test/main/ble_frag.hpp`
  (verified on two boards there: 4000 echoes, no loss).

There is **no BLE pairing / encryption** on purpose: the Noise channel of kkproto is the security. What an eavesdropper
sees is the dongle's radio address, the fact that it is a KeyKeeper dongle, and ciphertext.

## How the vault finds the dongle
* pairing: scans for a dongle that advertises "pairing window open" (BOOT on the dongle);
* afterwards: connects to the stored BLE address of that dongle (NVS key `baddr` in namespace `kklink`);
* a dongle paired before addresses were stored (UART days) is found as "any KeyKeeper dongle" once, and its address is
  stored when the Noise session comes up.

The dongle accepts one connection; if somebody connects and does not start a handshake within 15 s, the dongle drops it.

## sdkconfig (both projects)
    CONFIG_BT_ENABLED=y
    CONFIG_BT_BLUEDROID_ENABLED=n
    CONFIG_BT_NIMBLE_ENABLED=y
    CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=247
    CONFIG_BT_NIMBLE_LOG_LEVEL_WARNING=y

Without `CONFIG_BT_NIMBLE_ENABLED` only a stub (`blelink_off.cpp`) is built.

## API
See `include/blelink/blelink.hpp`. One owner task calls `send()` / `recv()` and watches `epoch()`: whenever it changes, the
session over the old link is dead -- reset the endpoint and, if `connected()`, start a new session.

## Tests
`test_host/test_frag.cpp` (fragmentation). The NimBLE glue itself runs only on hardware; the integration (pairing,
reconnect, link cut during typing, forgetting, migration of a pre-BLE pairing) was exercised on a PC with a pipe-based
stand-in for this API.
