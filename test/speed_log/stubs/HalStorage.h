#pragma once

// In-memory storage for the speed log tests: append-mode opens, rename,
// remove, an open-failure switch and an open counter.
#include <Arduino.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using oflag_t = int;
inline constexpr oflag_t O_RDONLY = 0x0;
inline constexpr oflag_t O_WRONLY = 0x1;
inline constexpr oflag_t O_RDWR = 0x2;
inline constexpr oflag_t O_APPEND = 0x8;
inline constexpr oflag_t O_CREAT = 0x200;

class HalFile {
 public:
  HalFile() = default;
  HalFile(std::shared_ptr<std::vector<uint8_t>> data, const bool append) : data_(std::move(data)) {
    if (append) pos_ = data_->size();
  }
  explicit operator bool() const { return static_cast<bool>(data_); }
  size_t size() const { return data_ ? data_->size() : 0; }
  size_t write(const void* buf, const size_t n) {
    if (!data_) return 0;
    const auto* p = static_cast<const uint8_t*>(buf);
    if (pos_ + n > data_->size()) data_->resize(pos_ + n);
    std::copy(p, p + n, data_->begin() + static_cast<std::ptrdiff_t>(pos_));
    pos_ += n;
    return n;
  }
  int read(void* buf, const size_t n) {
    if (!data_) return -1;
    const size_t k = std::min(n, data_->size() - std::min(pos_, data_->size()));
    std::copy_n(data_->data() + pos_, k, static_cast<uint8_t*>(buf));
    pos_ += k;
    return static_cast<int>(k);
  }
  bool seekSet(const size_t p) {
    if (!data_ || p > data_->size()) return false;
    pos_ = p;
    return true;
  }
  bool close() {
    data_.reset();
    return true;
  }

 private:
  std::shared_ptr<std::vector<uint8_t>> data_;
  size_t pos_ = 0;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage storage;
    return storage;
  }

  bool failOpen = false;
  int openCalls = 0;

  void reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    files_.clear();
    failOpen = false;
    openCalls = 0;
  }
  HalFile open(const char* path, const oflag_t flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++openCalls;
    if (failOpen) return {};
    auto it = files_.find(path);
    if (it == files_.end()) {
      if (!(flags & O_CREAT)) return {};
      it = files_.emplace(path, std::make_shared<std::vector<uint8_t>>()).first;
    }
    return HalFile(it->second, (flags & O_APPEND) != 0);
  }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    file = open(path, O_RDONLY);
    return static_cast<bool>(file);
  }
  bool exists(const char* path) {
    std::lock_guard<std::mutex> lock(mutex_);
    return files_.count(path) != 0;
  }
  bool remove(const char* path) {
    std::lock_guard<std::mutex> lock(mutex_);
    return files_.erase(path) != 0;
  }
  bool rename(const char* from, const char* to) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = files_.find(from);
    if (it == files_.end() || files_.count(to)) return false;
    files_[to] = it->second;
    files_.erase(it);
    return true;
  }
  std::string text(const char* path) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = files_.find(path);
    return it == files_.end() ? std::string() : std::string(it->second->begin(), it->second->end());
  }
  void put(const char* path, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    files_[path] = std::make_shared<std::vector<uint8_t>>(text.begin(), text.end());
  }

 private:
  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files_;
};

#define Storage HalStorage::getInstance()
