
#include <butil/file_util.h>
#include <butil/logging.h>
#include <engine/engine.h>
#include <errno.h>   // errno
#include <fcntl.h>   // open
#include <unistd.h>  // read, write, lseek, close

#include <cstring>  // strerror
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "utils/file_tool.h"

Engine& Engine::GetInstance() {
  static Engine engine;
  return engine;
}

Engine::~Engine() {
  std::lock_guard<std::mutex> lock(_mtx);
  if (_data_fd > 0) {
    close(_data_fd);
    _data_fd = -1;
  }
  if (_mata_fd > 0) {
    close(_mata_fd);
    _mata_fd = -1;
  }
}

// bool Engine::GetPath
bool Engine::Init(const std::string& path) {
  _path = path;
  return true;
}

std::string Engine::CreatOrLoad(const std::string& snapshot_path) {
  std::lock_guard<std::mutex> lock(_mtx);
  std::string data_path = _path + _data_filename;
  std::string mata_path = _path + _mata_filename;
  if (snapshot_path.size() > 0) {
    LOG(INFO) << "snapshot_path=[" << snapshot_path << "] start Engine load.";
    std::string ss_data_path = snapshot_path + _data_filename;
    std::string ss_mata_path = snapshot_path + _mata_filename;

    butil::FilePath ss_data_fp(ss_data_path);
    butil::FilePath ss_mata_fp(ss_mata_path);
    if (butil::PathExists(ss_mata_fp) &&
        butil::PathExists(ss_data_fp)) {  // 存在文件
      LOG(INFO) << "snapshot file exist, backup engine files. backup_data_file:"
                << ss_data_path << ", backup_mata_file:" << ss_mata_path;
      BackupFile(data_path);
      BackupFile(mata_path);
      butil::CopyFile(ss_data_fp, butil::FilePath(data_path));
      butil::CopyFile(ss_mata_fp, butil::FilePath(mata_path));
    }
  }

  _data_fd = open(data_path.c_str(), O_RDWR | O_CREAT, 0666);
  if (_data_fd <= 0) {
    LOG(ERROR) << "data.txt Failed to open file: " << strerror(errno);
    return "open '" + data_path + "' file failed";
  }
  _mata_fd = open(mata_path.c_str(), O_RDWR | O_CREAT, 0666);
  if (_mata_fd <= 0) {
    LOG(ERROR) << "mata.dat Failed to open file: " << strerror(errno);
    return "open '" + mata_path + "' file failed";
  }
  _raw_data_offset = lseek(_data_fd, 0, SEEK_END);
  _mata_offset = lseek(_mata_fd, 0, SEEK_END);
  _data_size = _mata_offset / _one_doc_mata_size;

  char buf[_one_doc_mata_size];
  for (int i = 0; i < _data_size; ++i) {
    lseek(_mata_fd, i * _one_doc_mata_size, SEEK_SET);
    if (read(_mata_fd, buf, _one_doc_mata_size) != _one_doc_mata_size) {
      LOG(ERROR) << "Engine::Init(" << _path
                 << ") failed, read file error, offset:"
                 << i * _one_doc_mata_size << ", length:" << _one_doc_mata_size;
      return "read mata file failed.";
    }
    int32_t key = *((int32_t*)buf);
    int64_t off = *((ino64_t*)(buf + sizeof(int32_t)));
    int16_t len = *((int16_t*)(buf + sizeof(int32_t) + sizeof(int64_t)));
    _key_to_offset[key] = {off, len};
  }
  LOG(INFO) << "Engine::Init(" << _path << ") success, data_size:" << _data_size
            << ", raw_data_offset:" << _raw_data_offset;
  return "";
}

std::string Engine::BatchAdd(
    const std::vector<std::pair<int32_t, int16_t>>& kv_list,
    const std::string& data) {
  std::lock_guard<std::mutex> lock(_mtx);
  char buf[_one_doc_mata_size];
  int64_t all_len = 0;
  for (int i = 0; i < kv_list.size(); ++i) {
    const auto& key = kv_list[i].first;
    const auto& data_len = kv_list[i].second;

    lseek(_data_fd, _raw_data_offset, SEEK_SET);
    if (write(_data_fd, data.c_str() + all_len, data_len) != data_len) {
      std::ostringstream oss;
      oss << "Engine::write raw data failed, offset:" << _raw_data_offset
          << ", length:" << data_len;
      LOG(ERROR) << oss.str();
      return oss.str();
    }

    *((int32_t*)buf) = key;
    *((ino64_t*)(buf + sizeof(int32_t))) = _raw_data_offset;
    *((int16_t*)(buf + sizeof(int32_t) + sizeof(int64_t))) = data_len;

    lseek(_mata_fd, _mata_offset, SEEK_SET);
    if (write(_mata_fd, buf, _one_doc_mata_size) != _one_doc_mata_size) {
      std::ostringstream oss;
      oss << "Engine::write mata data failed, offset:" << _mata_offset
          << ", length:" << _one_doc_mata_size;
      LOG(ERROR) << oss.str();
      return oss.str();
    }

    _key_to_offset[key] = {_raw_data_offset, data_len};
    _raw_data_offset += data_len;
    _mata_offset += _one_doc_mata_size;
    all_len += data_len;
    ++_data_size;
  }
  return "";
}

bool Engine::Mget(const std::vector<int>& keys,
                  std::vector<int32_t>& doc_lengths, std::string& res_buf) {
  std::lock_guard<std::mutex> lock(_mtx);
  int64_t all_len = 0;
  doc_lengths.resize(keys.size(), 0);
  for (int i = 0; i < keys.size(); ++i) {
    int32_t key = keys[i];
    if (!_key_to_offset.count(key)) {
      continue;
    }
    auto [off, size] = _key_to_offset[key];
    all_len += size;
    doc_lengths[i] = size;
  }
  res_buf.resize(all_len);
  int64_t off = 0;
  for (int i = 0; i < keys.size(); ++i) {
    int32_t key = keys[i];
    if (!_key_to_offset.count(key)) {
      continue;
    }
    auto [off, size] = _key_to_offset[key];
    lseek(_data_fd, off, SEEK_SET);
    if (read(_data_fd, &res_buf[off], size) != size) {
      LOG(ERROR) << "Engine read raw data failed, offset" << off
                 << ", size:" << size;
      return false;
    }
    off += size;
  }
  return true;
}

std::string Engine::Get(int32_t key) {
  std::lock_guard<std::mutex> lock(_mtx);
  if (!_key_to_offset.count(key)) {
    return "";
  }
  auto [off, size] = _key_to_offset[key];
  std::string res;
  res.resize(size);
  lseek(_data_fd, off, SEEK_SET);
  if (read(_data_fd, &res[0], size) != size) {
    LOG(ERROR) << "Engine read raw data failed, offset" << off
               << ", size:" << size << ", key:" << key;
    return res;
  }
  LOG(INFO) << "read data_fd error, off:" << off << ", size:" << size
            << ", key:" << key << ", val:[" << res << "]";
  return res;
}

std::string Engine::Snapshot(const std::string& raft_root, std::vector<std::string>& filenames) {
  std::lock_guard<std::mutex> lock(_mtx);
  fsync(_data_fd);
  fsync(_mata_fd);
  std::string old_data_path = _path + _data_filename;
  std::string old_mata_path = _path + _mata_filename;
  std::string new_data_path = raft_root + _data_filename;
  std::string new_mata_path = raft_root + _mata_filename;
  butil::FilePath old_data_fp(old_data_path);
  butil::FilePath old_mata_fp(old_mata_path);
  butil::FilePath new_data_fp(new_data_path);
  butil::FilePath new_mata_fp(new_mata_path);

  butil::CopyFile(old_data_fp, new_data_fp);
  butil::CopyFile(old_mata_fp, new_mata_fp);
  filenames.push_back(_data_filename);
  filenames.push_back(_mata_filename);
  return "";
}

std::string Engine::Delete() {
  LOG(WARNING) << "****************** delete Engine ***************";
  std::lock_guard<std::mutex> lock(_mtx);
  if (_data_fd >= 0) {
    close(_data_fd);
  }
  if (_mata_fd >= 0) {
    close(_mata_fd);
  }
  _key_to_offset.clear();
  _data_size = 0;
  _mata_offset = 0;
  _raw_data_offset = 0;
  _key_to_offset.clear();
  auto data_file = _path + _mata_filename;
  auto mata_file = _path + _data_filename;
  butil::DeleteFile(butil::FilePath(data_file), false);
  butil::DeleteFile(butil::FilePath(mata_file), false);
  return "";
}
