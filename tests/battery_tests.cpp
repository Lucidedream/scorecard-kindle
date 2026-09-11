#include "Battery.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

void writeFile(const char* path, const char* content) {
  FILE* file = fopen(path, "w");
  assert(file != nullptr);
  fputs(content, file);
  fclose(file);
}

void setBaseEnv(const char* dir) { setenv("BATTERY_SUPPLY_DIR", dir, 1); }

void testMissingDirectory() {
  setBaseEnv("/tmp/scorecard-battery-tests-does-not-exist");
  assert(readBatteryPercent() == -1);
}

void testNoCapacityFile() {
  char base[] = "/tmp/scorecard-battery-tests-empty.XXXXXX";
  assert(mkdtemp(base) != nullptr);
  char usb[256];
  snprintf(usb, sizeof(usb), "%s/usb", base);
  mkdir(usb, 0755);
  setBaseEnv(base);
  assert(readBatteryPercent() == -1);
}

void testSkipsNonBatterySupply() {
  char base[] = "/tmp/scorecard-battery-tests-mixed.XXXXXX";
  assert(mkdtemp(base) != nullptr);
  char usb[256];
  char usbType[256];
  char usbCapacity[256];
  snprintf(usb, sizeof(usb), "%s/usb", base);
  mkdir(usb, 0755);
  snprintf(usbType, sizeof(usbType), "%s/type", usb);
  snprintf(usbCapacity, sizeof(usbCapacity), "%s/capacity", usb);
  writeFile(usbType, "USB\n");
  writeFile(usbCapacity, "5\n");  // present but must be skipped: not a battery

  char battery[256];
  char batteryType[256];
  char batteryCapacity[256];
  snprintf(battery, sizeof(battery), "%s/battery", base);
  mkdir(battery, 0755);
  snprintf(batteryType, sizeof(batteryType), "%s/type", battery);
  snprintf(batteryCapacity, sizeof(batteryCapacity), "%s/capacity", battery);
  writeFile(batteryType, "Battery\n");
  writeFile(batteryCapacity, "77\n");

  setBaseEnv(base);
  assert(readBatteryPercent() == 77);
}

void testNoTypeFileStillReads() {
  char base[] = "/tmp/scorecard-battery-tests-notype.XXXXXX";
  assert(mkdtemp(base) != nullptr);
  char node[256];
  char capacity[256];
  snprintf(node, sizeof(node), "%s/bd7181x_bat", base);
  mkdir(node, 0755);
  snprintf(capacity, sizeof(capacity), "%s/capacity", node);
  writeFile(capacity, "42\n");
  setBaseEnv(base);
  assert(readBatteryPercent() == 42);
}

void testRejectsOutOfRange() {
  char base[] = "/tmp/scorecard-battery-tests-badvalue.XXXXXX";
  assert(mkdtemp(base) != nullptr);
  char node[256];
  char capacity[256];
  snprintf(node, sizeof(node), "%s/battery", base);
  mkdir(node, 0755);
  snprintf(capacity, sizeof(capacity), "%s/capacity", node);
  writeFile(capacity, "150\n");
  setBaseEnv(base);
  assert(readBatteryPercent() == -1);
}

}  // namespace

int main() {
  testMissingDirectory();
  testNoCapacityFile();
  testSkipsNonBatterySupply();
  testNoTypeFileStillReads();
  testRejectsOutOfRange();
  puts("Battery tests passed");
  return 0;
}
