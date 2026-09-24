#pragma once
// Minimal host stand-in for the SD card API used by HighlightStore.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

#ifndef O_RDONLY
#define O_RDONLY 0x00
#endif
#ifndef O_WRONLY
#define O_WRONLY 0x01
#endif
#ifndef O_RDWR
#define O_RDWR 0x02
#endif
#ifndef O_CREAT
#define O_CREAT 0x40
#endif
#ifndef O_APPEND
#define O_APPEND 0x400
#endif

inline std::string testRoot;

class HalFile {
  std::string path_;
  FILE* file_ = nullptr;
  std::filesystem::directory_iterator iterator_, end_;
  bool directory_ = false;

 public:
  HalFile() = default;
  HalFile(std::string path, const char* mode) : path_(std::move(path)) {
    directory_ = std::filesystem::is_directory(path_);
    if (directory_) {
      iterator_ = std::filesystem::directory_iterator(path_);
    } else if (mode) {
      file_ = std::fopen(path_.c_str(), mode);
    }
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  HalFile(HalFile&& o) noexcept { *this = std::move(o); }
  HalFile& operator=(HalFile&& o) noexcept {
    close();
    path_ = std::move(o.path_);
    file_ = std::exchange(o.file_, nullptr);
    directory_ = std::exchange(o.directory_, false);
    iterator_ = std::move(o.iterator_);
    return *this;
  }
  ~HalFile() { close(); }
  bool close() {
    if (file_) std::fclose(file_);
    file_ = nullptr;
    directory_ = false;
    return true;
  }
  explicit operator bool() const { return file_ || directory_; }
  bool isDirectory() const { return directory_; }
  HalFile openNextFile() {
    if (!directory_ || iterator_ == end_) return {};
    auto p = iterator_->path();
    ++iterator_;
    return HalFile(p.string(), std::filesystem::is_directory(p) ? nullptr : "rb");
  }
  size_t getName(char* out, size_t cap) {
    const auto n = std::filesystem::path(path_).filename().string();
    std::snprintf(out, cap, "%s", n.c_str());
    return n.size();
  }
  size_t fileSize() {
    if (!file_) return 0;
    const long pos = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    const long n = std::ftell(file_);
    std::fseek(file_, pos, SEEK_SET);
    return static_cast<size_t>(n);
  }
  bool seekSet(size_t offset) { return file_ && std::fseek(file_, static_cast<long>(offset), SEEK_SET) == 0; }
  int read(void* data, size_t length) { return file_ ? static_cast<int>(std::fread(data, 1, length, file_)) : -1; }
  size_t write(const void* data, size_t length) { return file_ ? std::fwrite(data, 1, length, file_) : 0; }
  size_t write(uint8_t b) { return write(&b, 1); }
};

class TestStorage {
 public:
  HalFile open(const char* path, int oflag = O_RDONLY) {
    const std::string full = testRoot + path;
    if (std::filesystem::is_directory(full)) return HalFile(full, nullptr);
    const char* mode = "rb";
    if (oflag & O_APPEND) {
      mode = "ab";
    } else if ((oflag & O_RDWR) && (oflag & O_CREAT)) {
      if (!std::filesystem::exists(full)) {
        FILE* f = std::fopen(full.c_str(), "wb");
        if (f) std::fclose(f);
      }
      mode = "r+b";
    } else if (oflag & O_RDWR) {
      mode = "r+b";
    } else if (oflag & O_WRONLY) {
      mode = "wb";
    }
    return HalFile(full, mode);
  }
  bool exists(const char* path) { return std::filesystem::exists(testRoot + path); }
  bool ensureDirectoryExists(const char* path) {
    std::error_code e;
    std::filesystem::create_directories(testRoot + path, e);
    return std::filesystem::is_directory(testRoot + path);
  }
  bool remove(const char* path) { return std::filesystem::remove(testRoot + path); }
  bool rename(const char* src, const char* dst) {
    std::error_code e;
    std::filesystem::rename(testRoot + src, testRoot + dst, e);
    return !e;
  }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    file = HalFile(testRoot + path, "rb");
    return static_cast<bool>(file) && !file.isDirectory();
  }
  bool openFileForWrite(const char*, const char* path, HalFile& file) {
    file = HalFile(testRoot + path, "wb");
    return static_cast<bool>(file);
  }
};
inline TestStorage Storage;
