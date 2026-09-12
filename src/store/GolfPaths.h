#pragma once

#include <stddef.h>

inline constexpr char GOLF_DEFAULT_ROOT[] = "/mnt/us/scorecard";
inline constexpr char GOLF_STATE_FILE[] = "state.json";
inline constexpr char GOLF_ROUNDS_DIR[] = "rounds";
inline constexpr char GOLF_COURSES_DIR[] = "courses";
inline constexpr char GOLF_INDEX_FILE[] = "index.csv";
inline constexpr size_t GOLF_PATH_CAPACITY = 512;
inline constexpr size_t GOLF_ARCHIVE_NAME_CAPACITY = 96;

const char* golfStorageRoot();
bool golfPath(char* output, size_t capacity, const char* relative);
