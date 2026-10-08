#include <gtest/gtest.h>

#include <string>

#include "src/util/OnlineDictionaryText.h"

namespace {

using onlinedict::Extractor;
using onlinedict::Lang;
using onlinedict::Source;

// Feeds the answer in small pieces, as it arrives from the network.
bool extract(const Source source, const Lang lang, const std::string& word, const std::string& json,
             std::string& html, const size_t chunk = 7) {
  Extractor extractor(source, lang, word);
  for (size_t at = 0; at < json.size(); at += chunk) {
    extractor.feed(json.data() + at, std::min(chunk, json.size() - at));
  }
  return extractor.finish(html);
}

}  // namespace

TEST(OnlineDictionaryText, PicksTheLanguage) {
  EXPECT_EQ(onlinedict::languageFor("serendipity", "en-en"), Lang::English);
  EXPECT_EQ(onlinedict::languageFor("Fernweh", "de-de"), Lang::German);
  EXPECT_EQ(onlinedict::languageFor("Fernweh", "de-en"), Lang::German);
  EXPECT_EQ(onlinedict::languageFor("word", nullptr), Lang::English);
  EXPECT_EQ(onlinedict::languageFor("word", "xx-yy"), Lang::English);
  // Script wins over the dictionary in use, both ways.
  EXPECT_EQ(onlinedict::languageFor("\xDA\xA9\xD8\xAA\xD8\xA7\xD8\xA8", "en-en"), Lang::Persian);
  EXPECT_EQ(onlinedict::languageFor("book", "fa-fa"), Lang::English);
  EXPECT_STREQ(onlinedict::code(Lang::Persian), "fa");
}

TEST(OnlineDictionaryText, NormalizesPersianLetters) {
  // Arabic yeh and kaf, a fatha and a tatweel: "كِتـابي" becomes "کتابی".
  const std::string arabicForms = "  \xD9\x83\xD9\x90\xD8\xAA\xD9\x80\xD8\xA7\xD8\xA8\xD9\x8A ";
  EXPECT_EQ(onlinedict::normalizeWord(arabicForms), "\xDA\xA9\xD8\xAA\xD8\xA7\xD8\xA8\xDB\x8C");
  EXPECT_EQ(onlinedict::normalizeWord(" Fernweh\n"), "Fernweh");
  EXPECT_EQ(onlinedict::otherCase("Serendipity"), "serendipity");
  EXPECT_EQ(onlinedict::otherCase("gehen"), "Gehen");
  EXPECT_EQ(onlinedict::otherCase("\xC3\x84pfel"), "");
}

TEST(OnlineDictionaryText, BuildsUrls) {
  EXPECT_EQ(onlinedict::percentEncode("a b/\xC3\xBC"), "a%20b%2F%C3%BC");
  EXPECT_EQ(onlinedict::definitionUrl(Lang::English, "ice cream"),
            "https://en.wiktionary.org/api/rest_v1/page/definition/ice%20cream");
  EXPECT_EQ(onlinedict::definitionUrl(Lang::German, "gehen"),
            "https://de.wiktionary.org/w/api.php?action=query&prop=extracts&explaintext=1&format=json&redirects=1"
            "&titles=gehen");
  EXPECT_EQ(onlinedict::summaryUrl(Lang::English, "New York"),
            "https://en.wikipedia.org/api/rest_v1/page/summary/New_York");
}

TEST(OnlineDictionaryText, EnglishDefinitions) {
  const std::string json =
      R"({"de":[{"partOfSpeech":"Noun","language":"German","definitions":[{"definition":"not this one"}]}],)"
      R"("en":[{"partOfSpeech":"Noun","language":"English","definitions":[)"
      R"({"definition":"The <a rel=\"mw:WikiLink\" href=\"/wiki/x\">phenomenon</a> of a lucky &amp; unplanned find.",)"
      R"("parsedExamples":[{"example":"ignored"}],"examples":["It was pure <b>serendipity</b>.","second"]},)"
      R"({"definition":""},)"
      R"({"definition":"A happy accident — café 😀."},)"
      R"({"definition":"three"},{"definition":"four"},{"definition":"five","examples":["never shown"]}]},)"
      R"({"partOfSpeech":"Verb","language":"English","definitions":[{"definition":"To find by chance."},)"
      R"({"definition":"b"},{"definition":"c"}]}],)"
      R"("fr":[{"partOfSpeech":"Noun","language":"French","definitions":[{"definition":"nor this"}]}]})";
  std::string html;
  ASSERT_TRUE(extract(Source::WiktionaryDefinitions, Lang::English, "serendipity", json, html));
  EXPECT_EQ(html,
            "<p><i>noun</i><br><b>1.</b> The phenomenon of a lucky &amp; unplanned find."
            "<br><i>It was pure serendipity.</i>"
            "<br><b>2.</b> A happy accident \xE2\x80\x94 caf\xC3\xA9 \xF0\x9F\x98\x80."
            "<br><b>3.</b> three<br><b>4.</b> four</p>"
            "<p><i>verb</i><br><b>1.</b> To find by chance.<br><b>2.</b> b</p>"
            "<p><i>Wiktionary (CC BY-SA 4.0)</i></p>");

  // The same answer one byte at a time gives the same entry.
  std::string again;
  ASSERT_TRUE(extract(Source::WiktionaryDefinitions, Lang::English, "serendipity", json, again, 1));
  EXPECT_EQ(again, html);

  // An answer without the wanted language is no entry.
  std::string none;
  EXPECT_FALSE(extract(Source::WiktionaryDefinitions, Lang::English, "x",
                       R"({"fr":[{"partOfSpeech":"Noun","definitions":[{"definition":"mot"}]}]})", none));
  EXPECT_FALSE(extract(Source::WiktionaryDefinitions, Lang::English, "x", R"({"status":404,"type":"Internal error"})",
                       none));
}

TEST(OnlineDictionaryText, GermanEntryPage) {
  const std::string json =
      R"({"batchcomplete":"","query":{"pages":{"18841":{"pageid":18841,"ns":0,"title":"gehen","extract":)"
      R"("\n== gehen (Deutsch) ==\n\n\n=== Verb, unregelmäßig ===\n\nNebenformen:\ngehn\nWorttrennung:\nge·hen)"
      R"(\nAussprache:\nIPA: [ˈɡeːən], in Österreich auch: [ɡeːn]\nHörbeispiele:  gehen (Info))"
      R"(\nBedeutungen:\nintransitiv:\n[1] sich schreitend fortbewegen\n[2] einen Ort verlassen\n[3] funktionieren)"
      R"(\n[4] viel gekauft werden\n[5] nicht mehr gezeigt\nHerkunft:\nmittelhochdeutsch gān\nSynonyme:\n[1] laufen, schreiten)"
      R"(\nGegenwörter:\n[1] stehen\nBeispiele:\n[1] Ich gehe zum Bahnhof.\n[2] Er ging.\n[3] Ein dritter Satz.)"
      R"(\n\n==== Übersetzungen ====\n[1] englisch: go\n\n=== Substantiv, n ===\nBedeutungen:\n[1] das Gehen als Sportart)"
      R"(\n[2] zweite\n[3] dritte\n\n== gehen (Niederdeutsch) ==\n=== Verb ===\nBedeutungen:\n[1] nicht mehr Deutsch\n"}}}})";
  std::string html;
  ASSERT_TRUE(extract(Source::WiktionaryExtract, Lang::German, "gehen", json, html));
  EXPECT_EQ(html,
            "<p><i>Verb, unregelm\xC3\xA4\xC3\x9Fig</i> [\xCB\x88\xC9\xA1" "e\xCB\x90\xC9\x99n]"
            "<br><b>1.</b> sich schreitend fortbewegen<br><b>2.</b> einen Ort verlassen"
            "<br><b>3.</b> funktionieren<br><b>4.</b> viel gekauft werden"
            "<br><i>Ich gehe zum Bahnhof.</i><br><i>Er ging.</i></p>"
            "<p><b>Synonyme:</b> laufen, schreiten</p><p><b>Gegenw\xC3\xB6rter:</b> stehen</p>"
            "<p><i>Substantiv, n</i><br><b>1.</b> das Gehen als Sportart<br><b>2.</b> zweite</p>"
            "<p><i>Wiktionary (CC BY-SA 4.0)</i></p>");

  std::string none;
  EXPECT_FALSE(extract(Source::WiktionaryExtract, Lang::German, "Qq",
                       R"({"batchcomplete":"","query":{"pages":{"-1":{"ns":0,"title":"Qq","missing":""}}}})", none));
}

TEST(OnlineDictionaryText, PersianEntryPage) {
  // == فارسی == / === ریشه لغت === (skipped) / === اسم === with the headword, two senses and a rule.
  const std::string json =
      R"({"query":{"pages":{"7":{"title":"کتاب","extract":)"
      R"("\n== فارسی ==\n\n=== ریشه لغت ===\nعربی)"
      R"(\n\n=== اسم ===\nکتاب \n\nنوشته، مکتوب.)"
      R"(\nنسک\n––––\n\n==== برگردان‌ها ====\nفرهنگ\n"}}}})";
  std::string html;
  ASSERT_TRUE(extract(Source::WiktionaryExtract, Lang::Persian, "\xDA\xA9\xD8\xAA\xD8\xA7\xD8\xA8", json, html));
  EXPECT_EQ(html,
            "<p><i>\xD8\xA7\xD8\xB3\xD9\x85</i>"
            "<br><b>1.</b> \xD9\x86\xD9\x88\xD8\xB4\xD8\xAA\xD9\x87\xD8\x8C \xD9\x85\xDA\xA9\xD8\xAA\xD9\x88\xD8\xA8."
            "<br><b>2.</b> \xD9\x86\xD8\xB3\xDA\xA9</p>"
            "<p><i>Wiktionary (CC BY-SA 4.0)</i></p>");
}

TEST(OnlineDictionaryText, UnknownPageLayoutFallsBackToItsOpeningLines) {
  const std::string json = R"({"query":{"pages":{"3":{"extract":"Plain first line.\nSecond <line> here."}}}})";
  std::string html;
  ASSERT_TRUE(extract(Source::WiktionaryExtract, Lang::German, "x", json, html));
  EXPECT_EQ(html, "<p>Plain first line. Second &lt;line&gt; here.</p><p><i>Wiktionary (CC BY-SA 4.0)</i></p>");
}

TEST(OnlineDictionaryText, WikipediaSummary) {
  const std::string json =
      R"({"type":"standard","title":"Serendipity","displaytitle":"<span>Serendipity</span>",)"
      R"("namespace":{"id":0,"text":""},"titles":{"canonical":"Serendipity","normalized":"Serendipity"},)"
      R"("description":"Unplanned, fortunate discovery","extract":"Serendipity is an unplanned fortunate discovery.",)"
      R"("extract_html":"<p>ignored</p>"})";
  std::string html;
  ASSERT_TRUE(extract(Source::WikipediaSummary, Lang::English, "serendipity", json, html));
  EXPECT_EQ(html,
            "<p><b>Serendipity</b> (Unplanned, fortunate discovery)<br>"
            "Serendipity is an unplanned fortunate discovery.</p><p><i>Wikipedia (CC BY-SA 4.0)</i></p>");

  std::string none;
  EXPECT_FALSE(extract(Source::WikipediaSummary, Lang::English, "Mercury",
                       R"({"type":"disambiguation","title":"Mercury","extract":"Mercury most commonly refers to:"})",
                       none));
  EXPECT_FALSE(extract(Source::WikipediaSummary, Lang::English, "x",
                       R"({"type":"https://mediawiki.org/wiki/HyperSwitch/errors/not_found","title":"Not found."})",
                       none));
}

TEST(OnlineDictionaryText, LongSummaryIsCutOnAWord) {
  std::string extractText;
  while (extractText.size() < 2000) extractText += "word ";
  std::string html;
  ASSERT_TRUE(extract(Source::WikipediaSummary, Lang::English, "x",
                      R"({"type":"standard","title":"X","extract":")" + extractText + R"("})", html));
  EXPECT_LT(html.size(), 1100u);
  EXPECT_NE(html.find("word\xE2\x80\xA6</p>"), std::string::npos);
}
