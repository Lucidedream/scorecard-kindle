#include "CareerStatsScreen.h"

#include <stdio.h>

namespace {

void setValue(CareerStatsView& view, const uint8_t row, const char* label, const char* value) {
  view.rows[row].label = label;
  snprintf(view.rows[row].value, sizeof(view.rows[row].value), "%s", value);
}

void setUnsigned(CareerStatsView& view, const uint8_t row, const char* label, const uint32_t value) {
  view.rows[row].label = label;
  snprintf(view.rows[row].value, sizeof(view.rows[row].value), "%u", value);
}

void setRatio(CareerStatsView& view, const uint8_t row, const char* label, const uint32_t numerator,
              const uint32_t denominator, const bool percentage) {
  view.rows[row].label = label;
  if (denominator == 0) snprintf(view.rows[row].value, sizeof(view.rows[row].value), "—");
  else if (percentage)
    snprintf(view.rows[row].value, sizeof(view.rows[row].value), "%.1f%%",
             100.0 * static_cast<double>(numerator) / denominator);
  else
    snprintf(view.rows[row].value, sizeof(view.rows[row].value), "%.1f",
             static_cast<double>(numerator) / denominator);
}

void setAverageToPar(char* output, const size_t capacity, const double average, const uint8_t par) {
  const double toPar = average - par;
  snprintf(output, capacity, "%.1f · %+.1f", average, toPar);
}

}  // namespace

uint8_t stepCareerStatsSection(const uint8_t section, const bool forward) {
  const uint8_t current = section < GOLF_CAREER_SECTION_COUNT ? section : 0;
  return forward ? static_cast<uint8_t>((current + 1) % GOLF_CAREER_SECTION_COUNT)
                 : static_cast<uint8_t>((current + GOLF_CAREER_SECTION_COUNT - 1) %
                                        GOLF_CAREER_SECTION_COUNT);
}

CareerStatsView careerStatsView(const GolfCareerTally& tally, const uint8_t requestedSection) {
  CareerStatsView view{};
  const uint8_t section = requestedSection < GOLF_CAREER_SECTION_COUNT ? requestedSection : 0;
  static constexpr const char* TITLES[GOLF_CAREER_SECTION_COUNT] = {
      "CAREER · 1 / 5", "SCORE SHAPE · 2 / 5", "BY PAR · 3 / 5",
      "AROUND THE GREEN · 4 / 5", "RECORDS · 5 / 5"};
  snprintf(view.title, sizeof(view.title), "%s", TITLES[section]);
  view.empty = tally.rounds == 0;
  if (view.empty) return view;

  if (section == 0) {
    view.rowCount = 3;
    setUnsigned(view, 0, "Rounds played", tally.rounds);
    setRatio(view, 1, "Scoring average", tally.strokes, tally.rounds, false);
    view.rows[2].label = "Average to par";
    if (tally.parRounds == 0) snprintf(view.rows[2].value, sizeof(view.rows[2].value), "—");
    else snprintf(view.rows[2].value, sizeof(view.rows[2].value), "%+.1f",
                  static_cast<double>(tally.toParTotal) / tally.parRounds);
  } else if (section == 1) {
    static constexpr const char* LABELS[6] = {
        "Eagle or better", "Birdie", "Par", "Bogey", "Double", "Triple or worse"};
    uint32_t total = 0;
    for (uint8_t i = 0; i < 6; ++i) total += tally.dist[i];
    view.rowCount = 6;
    for (uint8_t i = 0; i < 6; ++i) {
      view.rows[i].label = LABELS[i];
      if (total == 0) snprintf(view.rows[i].value, sizeof(view.rows[i].value), "%u · —", tally.dist[i]);
      else snprintf(view.rows[i].value, sizeof(view.rows[i].value), "%u · %.1f%%", tally.dist[i],
                    100.0 * static_cast<double>(tally.dist[i]) / total);
    }
  } else if (section == 2) {
    static constexpr const char* LABELS[3] = {"Par 3", "Par 4", "Par 5"};
    view.rowCount = 3;
    for (uint8_t i = 0; i < 3; ++i) {
      view.rows[i].label = LABELS[i];
      if (tally.parHoles[i] == 0) snprintf(view.rows[i].value, sizeof(view.rows[i].value), "—");
      else setAverageToPar(view.rows[i].value, sizeof(view.rows[i].value),
                           static_cast<double>(tally.parStrokes[i]) / tally.parHoles[i],
                           static_cast<uint8_t>(i + 3));
    }
  } else if (section == 3) {
    view.rowCount = 4;
    setRatio(view, 0, "Scrambling", tally.scrambles, tally.scrambleChances, true);
    setRatio(view, 1, "Sand saves", tally.sandSaves, tally.sandSaveChances, true);
    setRatio(view, 2, "Putts per GIR", tally.puttsOnGir, tally.girHoles, false);
    setRatio(view, 3, "3-putt holes", tally.threePuttHoles, tally.holesPlayed, true);
  } else {
    view.rowCount = 5;
    view.rows[0].label = "Lowest round";
    if (tally.lowestRound == 0) snprintf(view.rows[0].value, sizeof(view.rows[0].value), "—");
    else snprintf(view.rows[0].value, sizeof(view.rows[0].value), "%u · %s", tally.lowestRound,
                  tally.lowestCourse);
    if (tally.lowestNine == 0) setValue(view, 1, "Lowest nine", "—");
    else setUnsigned(view, 1, "Lowest nine", tally.lowestNine);
    if (tally.fewestPutts == 0) setValue(view, 2, "Fewest putts", "—");
    else setUnsigned(view, 2, "Fewest putts", tally.fewestPutts);
    uint32_t distributionTotal = 0;
    for (uint8_t i = 0; i < 6; ++i) distributionTotal += tally.dist[i];
    if (distributionTotal == 0) setValue(view, 3, "Most pars in a round", "—");
    else setUnsigned(view, 3, "Most pars in a round", tally.mostPars);
    if (tally.holesPlayed == 0) setValue(view, 4, "Longest bogey-free run", "—");
    else setUnsigned(view, 4, "Longest bogey-free run", tally.longestBogeyFreeRun);
  }
  return view;
}
