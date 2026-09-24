#pragma once
// Host stand-in: only the strings HighlightStore uses.
enum class TestStr { STR_FAVORITE_LINES, STR_FLASHCARDS };
using StrId = TestStr;
constexpr TestStr STR_FAVORITE_LINES = TestStr::STR_FAVORITE_LINES;
constexpr TestStr STR_FLASHCARDS = TestStr::STR_FLASHCARDS;
inline const char* testTr(TestStr id) { return id == TestStr::STR_FAVORITE_LINES ? "Favorite Lines" : "Flashcards"; }
#define tr(id) testTr(id)
