#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#define InsertDataToEngine 1
#define DeleteDataToEngine 2

class Engine {
 public:
  Engine() {}
  ~Engine();

  bool Init(const std::string& path);

  std::string CreatOrLoad(const std::string& snapshot_path);

  std::string BatchAdd(const std::vector<std::pair<int32_t, int16_t>>& kv_list,
                       const std::string& data);

  bool Mget(const std::vector<int>& keys, std::vector<int32_t>& doc_lengths,
            std::string& res_buf);

  std::string Get(int32_t key);

  std::string Snapshot(const std::string& raft_root, std::vector<std::string>& filenames);

  std::string Delete();

  static Engine& GetInstance();

 private:
  Engine(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine& operator=(Engine&&) = delete;

 private:
  const std::string _mata_filename = "/engine_mata.dat";
  const std::string _data_filename = "/engine_data.txt";

  std::string _path;
  int _data_fd = -1;
  int _mata_fd = -1;
  const int _one_doc_mata_size =
      sizeof(int32_t) + sizeof(int64_t) + sizeof(int16_t);
  int _data_size = 0;
  int64_t _mata_offset = 0;
  int64_t _raw_data_offset = 0;
  std::map<int32_t, std::pair<int64_t, int16_t>> _key_to_offset;
  std::mutex _mtx;
};
