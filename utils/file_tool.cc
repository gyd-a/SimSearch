#include "utils/file_tool.h"

#include <butil/file_util.h>
#include <unistd.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>



std::string BackupFile(const std::string& filename) {
  butil::FilePath file_path(filename);
  if (butil::PathExists(file_path)) {  // 存在文件
    for (int i = 0; i < 100; ++i) {
      std::string new_path_file = filename + std::to_string(i);
      butil::FilePath fp(new_path_file);
      if (butil::PathExists(fp) && !butil::DirectoryExists(fp)) {
        // 存在了这个名字的文件，名字加1
      } else {
        if (rename(filename.c_str(), new_path_file.c_str()) != 0) {
          return filename + " file rename to " + new_path_file + " file failed";
        }
      }
    }
  }
  return "";
}
