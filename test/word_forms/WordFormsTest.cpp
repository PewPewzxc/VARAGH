#include "WordForms.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

// Position of `form` in the candidates, or -1.
int rank(const std::vector<std::string>& candidates, const std::string& form) {
  const auto it = std::find(candidates.begin(), candidates.end(), form);
  return it == candidates.end() ? -1 : static_cast<int>(it - candidates.begin());
}

// `better` is proposed, and before `worse` when that is proposed too.
void expectBefore(const std::string& word, const std::string& better, const std::string& worse,
                  const std::vector<std::string>& candidates) {
  const int b = rank(candidates, better);
  const int w = rank(candidates, worse);
  EXPECT_GE(b, 0) << word << " should propose " << better;
  if (w >= 0) {
    EXPECT_LT(b, w) << word << ": " << better << " should come before " << worse;
  }
}

}  // namespace

TEST(WordFormsEnglish, SmallestChangeComesFirst) {
  // The 1.2.1 bug: "nodes" found "nod" before "node".
  const struct {
    const char* word;
    const char* base;
    const char* wrong;
  } cases[] = {
      {"nodes", "node", "nod"},     {"Nodes", "node", "nod"},     {"codes", "code", "cod"},
      {"hopes", "hope", "hop"},     {"bites", "bite", "bit"},     {"notes", "note", "not"},
      {"tones", "tone", "ton"},     {"robes", "robe", "rob"},     {"hoped", "hope", "hop"},
      {"hoping", "hope", "hop"},    {"coding", "code", "cod"},    {"making", "make", "mak"},
      {"riding", "ride", "rid"},    {"later", "late", "lat"},     {"latest", "late", "lat"},
      {"planing", "plane", "plan"}, {"biting", "bite", "bit"},    {"using", "use", "us"},
      {"boxes", "box", "boxe"},     {"wishes", "wish", "wishe"},  {"classes", "class", "classe"},
      {"watches", "watch", "watche"}, {"buzzes", "buzz", "buzze"},
  };
  for (const auto& c : cases) expectBefore(c.word, c.base, c.wrong, WordForms::english(c.word));
}

TEST(WordFormsEnglish, CommonForms) {
  const struct {
    const char* word;
    const char* base;
  } cases[] = {
      {"cats", "cat"},          {"flies", "fly"},         {"wolves", "wolf"},       {"stopped", "stop"},
      {"running", "run"},       {"hopping", "hop"},       {"visited", "visit"},     {"opening", "open"},
      {"fixing", "fix"},        {"eating", "eat"},        {"wanted", "want"},       {"happily", "happy"},
      {"quickly", "quick"},     {"bigger", "big"},        {"happiest", "happy"},    {"kindness", "kind"},
      {"unhappy", "happy"},     {"studies", "study"},     {"tried", "try"},         {"crying", "cry"},
      {"goes", "go"},           {"heroes", "hero"},       {"analyses", "analysis"}, {"women", "woman"},
  };
  for (const auto& c : cases) {
    EXPECT_GE(rank(WordForms::english(c.word), c.base), 0) << c.word << " should propose " << c.base;
  }
  // Words that need no ending removed still get their exact spelling looked up
  // first by the dictionary; the candidates never repeat the word itself.
  for (const char* word : {"node", "later", "water"}) {
    EXPECT_EQ(rank(WordForms::english(word), word), -1) << word;
  }
}

TEST(WordFormsEnglish, PrefixRemovalComesLast) {
  const auto c = WordForms::english("unhappiness");
  ASSERT_FALSE(c.empty());
  EXPECT_LT(rank(c, "unhappy"), rank(c, "happiness"));
}

TEST(WordFormsPersian, Detection) {
  EXPECT_TRUE(WordForms::isArabicScript("کتاب"));
  EXPECT_FALSE(WordForms::isArabicScript("Buch"));
  EXPECT_FALSE(WordForms::isArabicScript("Bücher"));
}

TEST(WordFormsPersian, NounEndings) {
  const struct {
    const char* word;
    const char* base;
  } cases[] = {
      {"کتاب‌ها", "کتاب"},     {"کتابها", "کتاب"},     {"کتاب‌های", "کتاب"},    {"کتاب‌هایم", "کتاب"},
      {"خانه‌های", "خانه"},    {"خانه‌ای", "خانه"},    {"خانه‌ام", "خانه"},     {"درختان", "درخت"},
      {"دانشجویان", "دانشجو"}, {"بندگان", "بنده"},     {"کلمات", "کلمه"},       {"تحقیقات", "تحقیق"},
      {"کتابم", "کتاب"},       {"کتابش", "کتاب"},      {"کتابمان", "کتاب"},     {"دوستانشان", "دوست"},
      {"بزرگ‌تر", "بزرگ"},     {"بزرگترین", "بزرگ"},   {"زیباتر", "زیبا"},      {"کتابی", "کتاب"},
  };
  for (const auto& c : cases) {
    EXPECT_GE(rank(WordForms::persian(c.word), c.base), 0) << c.word << " should propose " << c.base;
  }
}

TEST(WordFormsPersian, Verbs) {
  const struct {
    const char* word;
    const char* infinitive;
  } cases[] = {
      {"می‌روم", "رفتن"},       {"میروم", "رفتن"},       {"نمی‌روم", "رفتن"},     {"رفتم", "رفتن"},
      {"رفتند", "رفتن"},        {"رفته", "رفتن"},        {"رفته‌ام", "رفتن"},     {"بروید", "رفتن"},
      {"می‌کنم", "کردن"},       {"کردیم", "کردن"},       {"می‌گوید", "گفتن"},     {"می‌بینی", "دیدن"},
      {"نمی‌خواهم", "خواستن"},  {"می‌شود", "شدن"},       {"می‌دهند", "دادن"},     {"می‌آیم", "آمدن"},
      {"می‌دانم", "دانستن"},    {"می‌نویسد", "نوشتن"},   {"خواندیم", "خواندن"},   {"می‌پرسد", "پرسیدن"},
      {"می‌خورم", "خوردن"},     {"خوردند", "خوردن"},     {"می‌رسیم", "رسیدن"},    {"نشستم", "نشستن"},
  };
  for (const auto& c : cases) {
    EXPECT_GE(rank(WordForms::persian(c.word), c.infinitive), 0) << c.word << " should propose " << c.infinitive;
  }
}

TEST(WordFormsPersian, PastVerbBeforeNounEnding) {
  const auto c = WordForms::persian("کردیم");
  EXPECT_LT(rank(c, "کردن"), rank(c, "کردی"));
  const auto d = WordForms::persian("نشستم");
  EXPECT_LT(rank(d, "نشستن"), rank(d, "نشست"));
  // A noun with a possessive ending still finds the noun: "دردم" -> "درد".
  EXPECT_GE(rank(WordForms::persian("دردم"), "درد"), 0);
}

TEST(WordFormsPersian, SpellingAndVowelMarks) {
  // Arabic yeh/kaf and vowel marks: the cleaned spelling is the first candidate.
  EXPECT_EQ(WordForms::persian("كتابي").front(), "کتابی");
  EXPECT_EQ(WordForms::persian("کِتاب").front(), "کتاب");
  EXPECT_EQ(WordForms::persian("مُحَمَّد").front(), "محمد");
  // A clean word is never repeated.
  EXPECT_EQ(rank(WordForms::persian("کتاب"), "کتاب"), -1);
}

TEST(WordFormsPersian, BoundedAndNoShortJunk) {
  for (const char* word : {"دوستانشان", "می‌نویسیم", "کتاب‌هایمان", "بزرگ‌ترین‌ها"}) {
    const auto c = WordForms::persian(word);
    EXPECT_LE(c.size(), WordForms::MAX_VARIANTS) << word;
    for (const auto& form : c) EXPECT_GE(form.size(), 4u) << word << " -> " << form;  // at least 2 letters
  }
}
