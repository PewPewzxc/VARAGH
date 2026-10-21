#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// VARAGH: the SD-card language pack written by scripts/gen_i18n.py
// (build_language_pack). Little-endian: "VLNG", u8 version, u8 0, u16 string
// count, u32 layout id, u32 data size, u16 offsets[count], data. An offset with
// bit 15 set points into the English strings built into the firmware.
namespace LanguagePackFormat {

constexpr size_t kHeaderSize = 16;
constexpr uint8_t kVersion = 1;

inline uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// Checks a whole pack held in memory against this firmware's string list and
// points data/offsets into it. A pack from another firmware, a cut-off
// download or an offset past the end is refused, so tr() never reads outside
// the buffer. buf must be 2-byte aligned (offsets are read in place).
inline bool parse(const uint8_t* buf, const size_t len, const uint32_t layoutId, const uint16_t count,
                  const char*& data, const uint16_t*& offsets) {
  if (!buf || len < kHeaderSize || std::memcmp(buf, "VLNG", 4) != 0 || buf[4] != kVersion) return false;
  const uint16_t packCount = static_cast<uint16_t>(buf[6] | (buf[7] << 8));
  if (packCount != count || readU32(buf + 8) != layoutId) return false;
  const uint32_t dataSize = readU32(buf + 12);
  const size_t offsetsSize = static_cast<size_t>(count) * 2;
  if (dataSize == 0 || len != kHeaderSize + offsetsSize + dataSize) return false;
  const char* d = reinterpret_cast<const char*>(buf + kHeaderSize + offsetsSize);
  if (d[dataSize - 1] != '\0') return false;  // every string ends inside the buffer
  const auto* o = reinterpret_cast<const uint16_t*>(buf + kHeaderSize);
  for (uint16_t i = 0; i < count; ++i) {
    if (!(o[i] & 0x8000) && o[i] >= dataSize) return false;
  }
  data = d;
  offsets = o;
  return true;
}

}  // namespace LanguagePackFormat
