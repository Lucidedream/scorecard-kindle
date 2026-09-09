#include "PgmCanvas.h"
#include "TouchInput.h"

#include "GolfRound.h"
#include "GolfRules.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr char SCREEN_PATH[] = "/tmp/scorecard-screen.pgm";
constexpr char STATE_DIR[] = "/mnt/us/scorecard";
constexpr char DIAGNOSTIC_SCREEN_PATH[] = "/mnt/us/scorecard/screen.pgm";
constexpr char STATE_PATH[] = "/mnt/us/scorecard/state.bin";
constexpr char STATE_TEMP_PATH[] = "/mnt/us/scorecard/state.tmp";
constexpr uint32_t STATE_VERSION = 1;

struct SavedRound {
  char magic[8];
  uint32_t version;
  GolfRound round;
  uint32_t checksum;
};

PgmCanvas canvas;

uint32_t checksumRound(const GolfRound& round) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&round);
  uint32_t hash = 2166136261U;
  for (std::size_t index = 0; index < sizeof(round); ++index) {
    hash ^= bytes[index];
    hash *= 16777619U;
  }
  return hash;
}

void initializeRound(GolfRound& round) {
  std::memset(&round, 0, sizeof(round));
  initializeGolfPlayerDefaults(round);
  std::memcpy(round.courseName, "QUICK ROUND", sizeof("QUICK ROUND"));
  round.holeCount = 18;
  round.currentPlayer = 0;
  round.players[0].tee = TeeSelection::Blue;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
}

bool saveRound(const GolfRound& round) {
  mkdir(STATE_DIR, 0755);
  SavedRound saved{};
  std::memcpy(saved.magic, "KGSCORE", sizeof("KGSCORE"));
  saved.version = STATE_VERSION;
  saved.round = round;
  saved.checksum = checksumRound(round);

  FILE* file = std::fopen(STATE_TEMP_PATH, "wb");
  if (file == nullptr) return false;
  const bool wrote = std::fwrite(&saved, 1, sizeof(saved), file) == sizeof(saved);
  const bool flushed = std::fflush(file) == 0;
  const bool synced = fsync(fileno(file)) == 0;
  const bool closed = std::fclose(file) == 0;
  if (!wrote || !flushed || !synced || !closed) return false;
  return std::rename(STATE_TEMP_PATH, STATE_PATH) == 0;
}

bool loadRound(GolfRound& round) {
  FILE* file = std::fopen(STATE_PATH, "rb");
  if (file == nullptr) return false;
  SavedRound saved{};
  const bool read = std::fread(&saved, 1, sizeof(saved), file) == sizeof(saved);
  const bool noExtraData = std::fgetc(file) == EOF;
  std::fclose(file);
  if (!read || !noExtraData || std::memcmp(saved.magic, "KGSCORE", sizeof("KGSCORE")) != 0 ||
      saved.version != STATE_VERSION || saved.checksum != checksumRound(saved.round)) {
    return false;
  }
  if (saved.round.holeCount == 0 || saved.round.holeCount > GolfRound::MAX_HOLES ||
      saved.round.currentHole >= saved.round.holeCount) {
    return false;
  }
  round = saved.round;
  return true;
}

unsigned int holeGross(const GolfPlayerScore& score, const uint8_t hole) {
  return static_cast<unsigned int>(score.in100[hole]) + score.out100[hole] + score.penaltyCount[hole];
}

unsigned int roundGross(const GolfRound& round) {
  unsigned int total = 0;
  const GolfPlayerScore& score = round.players[0].score;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) total += holeGross(score, hole);
  return total;
}

void drawButton(const int x, const int y, const int width, const int height, const char* label, const int scale = 8) {
  canvas.drawRect(x, y, width, height, 5);
  canvas.drawTextCentered(x + width / 2, y + (height - 7 * scale) / 2, label, scale);
}

void drawCounter(const int y, const char* label, const uint8_t value) {
  constexpr int rowHeight = 245;
  char valueText[8];
  std::snprintf(valueText, sizeof(valueText), "%u", static_cast<unsigned int>(value));
  canvas.drawText(55, y + 24, label, 6);
  drawButton(55, y + 92, 210, 120, "-", 11);
  canvas.drawTextCentered(PgmCanvas::WIDTH / 2, y + 100, valueText, 14);
  drawButton(815, y + 92, 210, 120, "+", 11);
  canvas.fillRect(40, y + rowHeight - 3, 1000, 3);
}

bool render(const GolfRound& round, const bool fullRefresh, const bool showOnDevice = true) {
  canvas.clear();
  canvas.fillRect(0, 0, PgmCanvas::WIDTH, 120);
  canvas.drawText(45, 30, "SCORECARD", 8, false);
  canvas.drawText(895, 30, "EXIT", 6, false);

  char holeText[32];
  std::snprintf(holeText, sizeof(holeText), "HOLE %u/%u", static_cast<unsigned int>(round.currentHole + 1),
                static_cast<unsigned int>(round.holeCount));
  drawButton(45, 145, 200, 145, "PREV", 7);
  canvas.drawTextCentered(PgmCanvas::WIDTH / 2, 175, holeText, 9);
  drawButton(835, 145, 200, 145, "NEXT", 7);

  const GolfPlayerScore& score = round.players[0].score;
  const uint8_t hole = round.currentHole;
  drawCounter(330, "PUTTS", score.putts[hole]);
  drawCounter(575, "INSIDE 100", score.in100[hole]);
  drawCounter(820, "OUTSIDE 100", score.out100[hole]);

  char totals[64];
  std::snprintf(totals, sizeof(totals), "HOLE %u   ROUND %u", holeGross(score, hole), roundGross(round));
  canvas.drawTextCentered(PgmCanvas::WIDTH / 2, 1135, totals, 8);
  canvas.drawTextCentered(PgmCanvas::WIDTH / 2, 1270, "TAP PLUS OR MINUS", 6);
  canvas.drawTextCentered(PgmCanvas::WIDTH / 2, 1340, "SAVES AUTOMATICALLY", 5);

  if (!canvas.write(SCREEN_PATH)) return false;
  if (!showOnDevice) return true;
  canvas.write(DIAGNOSTIC_SCREEN_PATH);
  const char* command = fullRefresh
                            ? "/mnt/us/libkh/bin/fbink -c -f -W GC16 -i /tmp/scorecard-screen.pgm >/mnt/us/scorecard/fbink.log 2>&1"
                            : "/mnt/us/libkh/bin/fbink -c -W DU -i /tmp/scorecard-screen.pgm >/mnt/us/scorecard/fbink.log 2>&1";
  const int status = std::system(command);
  FILE* log = std::fopen("/mnt/us/scorecard/runtime.log", "a");
  if (log != nullptr) {
    std::fprintf(log, "FBInk status: %d\n", status);
    std::fclose(log);
  }
  return status == 0;
}

bool handleTap(GolfRound& round, const TouchPoint tap) {
  if (tap.y < 125 && tap.x > 820) return false;
  if (tap.y >= 130 && tap.y < 310) {
    if (tap.x < 300) {
      round.currentHole = static_cast<uint8_t>((round.currentHole + round.holeCount - 1) % round.holeCount);
    } else if (tap.x > 780) {
      round.currentHole = static_cast<uint8_t>((round.currentHole + 1) % round.holeCount);
    }
    return true;
  }

  GolfField field;
  if (tap.y >= 330 && tap.y < 575) {
    field = GolfField::Putts;
  } else if (tap.y >= 575 && tap.y < 820) {
    field = GolfField::In100;
  } else if (tap.y >= 820 && tap.y < 1065) {
    field = GolfField::Out100;
  } else {
    return true;
  }

  GolfPlayerScore& score = round.players[0].score;
  if (tap.x < 320) {
    decrementGolfCounter(score, round.currentHole, field);
  } else if (tap.x > 760) {
    incrementGolfCounter(score, round.currentHole, field);
  }
  return true;
}

}  // namespace

int main(const int argc, char** argv) {
  GolfRound round{};
  if (!loadRound(round)) initializeRound(round);

  if (argc == 3 && std::strcmp(argv[1], "--render") == 0) {
    canvas.clear();
    if (!render(round, true, false)) return 1;
    return std::rename(SCREEN_PATH, argv[2]) == 0 ? 0 : 1;
  }

  TouchInput input;
  if (!input.openDevice()) {
    std::fprintf(stderr, "Could not find the Kindle touchscreen\n");
    return 2;
  }

  FILE* log = std::fopen("/mnt/us/scorecard/runtime.log", "a");
  if (log != nullptr) {
    std::fprintf(log, "Started with touchscreen: %s\n", input.deviceName());
    std::fclose(log);
  }

  bool running = true;
  unsigned int paints = 0;
  while (running) {
    if (!render(round, paints == 0 || paints % 8 == 0)) return 3;
    ++paints;
    TouchPoint tap{};
    if (!input.waitForTap(tap)) return 4;
    running = handleTap(round, tap);
    if (!saveRound(round)) return 5;
  }

  unlink(SCREEN_PATH);
  return 0;
}
