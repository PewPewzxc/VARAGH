// Self-check for src/util/LanguagePackFormat.h against a pack written by
// scripts/gen_i18n.py:  g++ -std=c++20 -Isrc test/language_pack/language_pack_check.cpp
//   && ./a.out <pack.lng> <I18N_LAYOUT_ID hex> <StrId::_COUNT>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "util/LanguagePackFormat.h"

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  FILE* f = std::fopen(argv[1], "rb");
  assert(f);
  std::vector<uint16_t> store(64 * 1024);
  auto* buf = reinterpret_cast<uint8_t*>(store.data());
  const size_t len = std::fread(buf, 1, store.size() * 2, f);
  std::fclose(f);
  const auto id = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 16));
  const auto count = static_cast<uint16_t>(std::atoi(argv[3]));
  const char* data = nullptr;
  const uint16_t* offsets = nullptr;
  using LanguagePackFormat::parse;
  assert(parse(buf, len, id, count, data, offsets));
  assert(!parse(buf, len, id ^ 1, count, data, offsets));        // another firmware
  assert(!parse(buf, len, id, count + 1, data, offsets));        // another string list
  assert(!parse(buf, len - 1, id, count, data, offsets));        // cut-off download
  uint16_t saved = 0;
  for (uint16_t i = 0; i < count; ++i) {
    auto* o = reinterpret_cast<uint16_t*>(buf + LanguagePackFormat::kHeaderSize) + i;
    if (*o & 0x8000) continue;
    saved = *o;
    *o = 0x7FFF;  // past the end of the data
    assert(!parse(buf, len, id, count, data, offsets));
    *o = saved;
    break;
  }
  buf[0] = 'X';
  assert(!parse(buf, len, id, count, data, offsets));
  std::puts("language pack check ok");
  return 0;
}
