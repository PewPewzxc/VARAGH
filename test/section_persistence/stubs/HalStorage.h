#pragma once

#include <Print.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using oflag_t = uint8_t;
inline constexpr oflag_t O_RDONLY = 0;
inline constexpr oflag_t O_RDWR = 1;

struct HostFileData {
  std::vector<uint8_t> bytes;
  size_t failAt = std::numeric_limits<size_t>::max();
};

class HalFile : public Print {
 public:
  HalFile() = default;
  explicit HalFile(std::shared_ptr<HostFileData> data) : data_(std::move(data)) {}

  void flush() {}
  size_t size() const { return data_ ? data_->bytes.size() : 0; }
  size_t fileSize() const { return size(); }
  int available() const { return data_ && cursor_ < data_->bytes.size(); }
  size_t position() const { return cursor_ + batchLength_; }

  int read(void* output, const size_t length) {
    if (!data_) return 0;
    const size_t readable = std::min(length, data_->bytes.size() - std::min(cursor_, data_->bytes.size()));
    if (readable > 0) std::copy_n(data_->bytes.data() + cursor_, readable, static_cast<uint8_t*>(output));
    cursor_ += readable;
    return static_cast<int>(readable);
  }

  bool seek(const size_t position) {
    if (!data_ || position > data_->bytes.size()) return false;
    cursor_ = position;
    return true;
  }
  bool seekSet(const size_t position) { return seek(position); }

  size_t write(const uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* input, const size_t length) override {
    if (batch_) {
      if (batchLength_ + length > batchCapacity_ && !flushWriteBatch()) return 0;
      if (length <= batchCapacity_) {
        std::copy_n(input, length, batch_ + batchLength_);
        batchLength_ += length;
        return length;
      }
    }
    return writeThrough(input, length);
  }
  size_t write(const void* input, const size_t length) { return write(static_cast<const uint8_t*>(input), length); }

  // Write batching as in lib/hal/HalStorage.h: writes collect in the buffer
  // and reach the file in one call when the batch ends or fills.
  void beginWriteBatch(uint8_t* buffer, const size_t capacity) {
    const bool earlierOk = flushWriteBatch();
    clearWriteBatch();
    batchOk_ = earlierOk;
    if (buffer && capacity > 0) {
      batch_ = buffer;
      batchCapacity_ = capacity;
    }
  }
  bool endWriteBatch() {
    const bool ok = flushWriteBatch();
    clearWriteBatch();
    return ok;
  }
  bool sync() const { return static_cast<bool>(data_); }
  bool close() {
    const bool ok = endWriteBatch();
    data_.reset();
    cursor_ = 0;
    return ok;
  }
  bool isOpen() const { return static_cast<bool>(data_); }
  explicit operator bool() const { return isOpen(); }

 private:
  size_t writeThrough(const uint8_t* input, const size_t length) {
    if (!data_ || cursor_ >= data_->failAt) return 0;
    const size_t writable = std::min(length, data_->failAt - cursor_);
    if (cursor_ + writable > data_->bytes.size()) data_->bytes.resize(cursor_ + writable);
    if (writable > 0) std::copy_n(input, writable, data_->bytes.begin() + static_cast<std::ptrdiff_t>(cursor_));
    cursor_ += writable;
    return writable;
  }
  bool flushWriteBatch() {
    if (batchLength_ > 0) {
      if (writeThrough(batch_, batchLength_) != batchLength_) batchOk_ = false;
      batchLength_ = 0;
    }
    return batchOk_;
  }
  void clearWriteBatch() {
    batch_ = nullptr;
    batchCapacity_ = 0;
    batchLength_ = 0;
    batchOk_ = true;
  }

  std::shared_ptr<HostFileData> data_;
  size_t cursor_ = 0;
  uint8_t* batch_ = nullptr;
  size_t batchCapacity_ = 0;
  size_t batchLength_ = 0;
  bool batchOk_ = true;
};

using FsFile = HalFile;

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage storage;
    return storage;
  }

  void reset() { files_.clear(); }
  bool mkdir(const char*, bool = true) { return true; }
  bool exists(const char* path) const { return files_.contains(path); }
  bool remove(const char* path) { return files_.erase(path) > 0; }
  bool rename(const char* from, const char* to) {
    const auto found = files_.find(from);
    if (found == files_.end() || files_.contains(to)) return false;
    files_[to] = found->second;
    files_.erase(found);
    return true;
  }

  HalFile open(const char* path, oflag_t) {
    const auto found = files_.find(path);
    return found == files_.end() ? HalFile{} : HalFile(found->second);
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    file = open(path.c_str(), O_RDONLY);
    return static_cast<bool>(file);
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    auto data = std::make_shared<HostFileData>();
    files_[path] = data;
    file = HalFile(std::move(data));
    return true;
  }

  void failWritesAt(const std::string& path, size_t offset) { files_.at(path)->failAt = offset; }
  const std::vector<uint8_t>& bytes(const std::string& path) const { return files_.at(path)->bytes; }
  void put(const std::string& path, std::vector<uint8_t> bytes) {
    auto data = std::make_shared<HostFileData>();
    data->bytes = std::move(bytes);
    files_[path] = std::move(data);
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<HostFileData>> files_;
};

#define Storage HalStorage::getInstance()
