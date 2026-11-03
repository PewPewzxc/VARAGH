#pragma once

#include <GfxRenderer.h>
#include <Utf8.h>

#include <cstdint>
#include <string>

// Plain-letter stand-ins for pronunciation (IPA), Greek and symbol characters a
// font cannot draw, shared by the lookup popup and the flashcards.
inline void appendDictionaryApproximation(uint32_t cp, std::string& out) {
  switch (cp) {
    case 0x00A0:
      out += ' ';
      break;  // nbsp
    case 0x00E6:
      out += "ae";
      break;  // ae
    case 0x00F0:
      out += "th";
      break;  // eth
    case 0x00F8:
      out += 'o';
      break;  // o slash
    case 0x0127:
      out += 'h';
      break;  // h stroke
    case 0x014B:
      out += "ng";
      break;  // eng
    case 0x0153:
      out += "oe";
      break;      // oe
    case 0x0250:  // turned a (German "-er" ending)
    case 0x0251:  // script a
    case 0x0252:
      out += 'a';
      break;  // turned alpha
    case 0x0254:
      out += 'o';
      break;  // open o
    case 0x0259:
      out += 'e';
      break;      // schwa
    case 0x025A:  // rhotacized schwa
    case 0x025D:
      out += "er";
      break;  // reversed epsilon hook
    case 0x025B:  // open e
    case 0x025C:
      out += 'e';
      break;  // reversed open e
    case 0x0261:
      out += 'g';
      break;  // script g
    case 0x026A:
      out += 'i';
      break;  // small capital i
    case 0x026B:
      out += 'l';
      break;  // velarized l
    case 0x0272:
      out += "ny";
      break;  // n hook
    case 0x0273:
      out += 'n';
      break;      // retroflex n
    case 0x0279:  // turned r
    case 0x027B:  // turned r hook
    case 0x027D:  // r tail
    case 0x027E:
      out += 'r';
      break;  // tap r
    case 0x0280:
      out += 'R';
      break;  // small capital r
    case 0x0281:
      out += 'r';
      break;  // inverted small capital r (German uvular r)
    case 0x0283:
      out += "sh";
      break;  // esh
    case 0x028A:
      out += 'u';
      break;  // upsilon
    case 0x028C:
      out += 'u';
      break;  // turned v
    case 0x028F:
      out += "\xC3\xBC";
      break;  // small capital y (German short ü)
    case 0x0292:
      out += "zh";
      break;  // ezh
    case 0x0294:
      out += '\'';
      break;  // glottal stop
    case 0x02A4:
      out += 'j';
      break;  // dezh digraph
    case 0x02A7:
      out += "ch";
      break;  // tesh digraph
    case 0x02B0:
      out += 'h';
      break;  // modifier h
    case 0x02B2:
      out += 'y';
      break;  // modifier j
    case 0x02B7:
      out += 'w';
      break;  // modifier w
    case 0x02BC:
      out += '\'';
      break;      // modifier apostrophe
    case 0x02C8:  // primary stress
    case 0x02CC:
      break;  // secondary stress
    case 0x02D0:
      out += ':';
      break;  // length mark
    case 0x03B2:
      out += 'b';
      break;  // beta
    case 0x03B8:
      out += "th";
      break;  // theta
    case 0x03C7:
      out += "kh";
      break;      // chi
    case 0x2013:  // en dash
    case 0x2014:  // em dash
    case 0x2212:
      out += '-';
      break;      // minus
    case 0x2018:  // left single quote
    case 0x2019:
      out += '\'';
      break;      // right single quote
    case 0x201C:  // left double quote
    case 0x201D:
      out += '"';
      break;  // right double quote
    case 0x2026:
      out += "...";
      break;  // ellipsis
    // Symbols common in English and bilingual dictionaries. Callers only get
    // here when the definition font cannot draw the original codepoint.
    case 0x2010:  // hyphen
    case 0x2011:  // non-breaking hyphen
    case 0x2012:
      out += '-';
      break;  // figure dash
    case 0x2032:
      out += '\'';
      break;  // prime
    case 0x2033:
      out += '"';
      break;  // double prime
    case 0x2022:  // bullet
    case 0x2023:  // triangular bullet
    case 0x25AA:  // small black square
    case 0x25CF:
      out += "\xC2\xB7";
      break;  // black circle -> middle dot
    case 0x2190:
      out += "<-";
      break;  // leftwards arrow
    case 0x2192:  // rightwards arrow
    case 0x21D2:
      out += "->";
      break;  // rightwards double arrow
    case 0x2194:
      out += "<->";
      break;  // left right arrow
    case 0x2248:
      out += '~';
      break;  // almost equal
    case 0x2260:
      out += "!=";
      break;  // not equal
    case 0x2264:
      out += "<=";
      break;  // less or equal
    case 0x2265:
      out += ">=";
      break;  // greater or equal
    case 0x27E8:  // mathematical left angle bracket
    case 0x2039:
      out += '<';
      break;  // single left-pointing angle quote
    case 0x27E9:  // mathematical right angle bracket
    case 0x203A:
      out += '>';
      break;  // single right-pointing angle quote
    case 0x2122:
      out += "(TM)";
      break;  // trade mark
    case 0x2116:
      out += "No.";
      break;  // numero sign
    case 0x2715:  // multiplication x
    case 0x2717:
      out += 'x';
      break;  // ballot x
    case 0x2713:  // check mark
    case 0x2714:
      out += 'v';
      break;  // heavy check mark
    default:
      // Nothing close enough exists in the font: drop the symbol rather than
      // draw a replacement box. IPA, Greek and combining marks were always
      // dropped here; other symbols now follow the same rule.
      break;
  }
}

// `text` with every character `fontId` cannot draw (itself or through its script
// fallback) replaced by its stand-in, so nothing shows as a replacement diamond.
inline std::string approximateUndrawable(const GfxRenderer& renderer, const int fontId, const std::string& text) {
  const auto font = renderer.getFontMap().find(fontId);
  if (font == renderer.getFontMap().end()) return text;
  std::string out;
  out.reserve(text.size());
  const auto* p = reinterpret_cast<const unsigned char*>(text.c_str());
  uint32_t cp = 0;
  while ((cp = utf8NextCodepoint(&p))) {
    if (cp < 0x80 || font->second.hasCodepoint(cp) || renderer.scriptFallbackCovers(fontId, cp)) {
      utf8AppendCodepoint(cp, out);
    } else {
      appendDictionaryApproximation(cp, out);
    }
  }
  return out;
}
