#pragma once

// Reads the device's battery level from the kernel's power-supply nodes
// (/sys/class/power_supply/<name>/capacity, or $BATTERY_SUPPLY_DIR when set,
// for host testing). Scans every node rather than assuming a fixed name,
// since the exact power-supply name is board-specific and unconfirmed on
// this device. Returns 0-100, or -1 if no readable battery capacity was
// found.
int readBatteryPercent();
