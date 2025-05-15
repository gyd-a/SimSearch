#pragma once

#include <list>
#include <mutex>
#include <vector>
#include <memory>


class OneWriterMultiReaderMap {
 public:
  OneWriterMultiReaderMap(uint16_t bucket_count)
      : buckets_(bucket_count) {};

  ~OneWriterMultiReaderMap() {};
  // 写操作，使用独占写锁
  bool Put(const std::string& key, int16_t idx);

  // 读操作，使用共享读锁
  int16_t Get(const std::string& key);

  bool Delete(const std::string& key);

 private:
  struct Cell {
    int16_t idx_;
    std::string key_;
  };
  // 桶：每个桶是一个 list
  std::vector<std::list<Cell>> buckets_;
  std::mutex mtx_;
};
