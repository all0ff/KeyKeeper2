#pragma once
// Host-test stand-in: only the one setting TypeEngine reads.
namespace settings {
struct UsbSettings { bool cyrillic_auto_switch_layout = false; };
struct Settings { UsbSettings usb; };
Settings& all(); // defined in fake_hid.cpp
}
