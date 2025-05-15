#include <iostream>
#include <list>
#include <mutex>
#include <vector>
#include <memory>
#include "utils/hash_algo.h"
#include "utils/one_writer_multi_reader_map.h"

// 写操作，使用独占写锁
bool OneWriterMultiReaderMap::Put(const std::string& key, int16_t idx) {
  size_t index = StrHashUint64(key) % buckets_.size();
  auto& bucket = buckets_[index];
  std::lock_guard<std::mutex> lock(mtx_);
  // 在桶中查找并更新
  for (auto& cell : bucket) {
    if (cell.idx_ >= 0) {
      return false;
    }
  }
  for (auto& cell : bucket) {
    if (cell.key_ == key) {
      cell.idx_ = idx;
      return true;
    }
  }
  Cell cell;
  cell.key_ = key;
  cell.idx_ = idx;
  bucket.push_back(cell);
  return true;
}

// 读操作，使用共享读锁
int16_t OneWriterMultiReaderMap::Get(const std::string& key) {
  size_t index = StrHashUint64(key) % buckets_.size();
  auto& bucket = buckets_[index];
  for (const auto& cell : bucket) {
    if (cell.key_ == key) {
      return cell.idx_;
    }
  }
  return -1;
}

bool OneWriterMultiReaderMap::Delete(const std::string& key) {
  size_t index = StrHashUint64(key)  % buckets_.size();
  auto& bucket = buckets_[index];
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& cell : bucket) {
    if (cell.key_ == key && cell.idx_ >= 0) {
      cell.idx_ = -1;
      return true;
    }
  }
  return false;
}
