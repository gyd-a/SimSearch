
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// ------------------- 抽象基类 -------------------
class IResource {
 public:
  virtual ~IResource() = default;
  virtual std::unordered_map<std::string, std::string> get_resource() = 0;
};

// ------------------- DynamicResource -------------------
class DynamicResource {
 public:
  DynamicResource(std::shared_ptr<IResource> resourcePtr, int flushInterval)
      : resourcePtr_(resourcePtr),
        flushInterval_(flushInterval),
        idx_(0),
        updateTime_(std::chrono::steady_clock::now()) {
    twoBuffer_[0] = getResource();
    twoBuffer_[1] = getResource();
  }

  std::unordered_map<std::string, std::string> getResourceWrapper() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - updateTime_)
            .count() > flushInterval_) {
      updateResource();
    }
    // std::lock_guard<std::mutex> lock(mutex_);
    return twoBuffer_[idx_];
  }

 private:
  std::shared_ptr<IResource> resourcePtr_;
  int flushInterval_;
  std::mutex mutex_;
  std::unordered_map<std::string, std::string> twoBuffer_[2];
  int idx_;
  std::chrono::steady_clock::time_point updateTime_;

  void updateResource() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - updateTime_)
            .count() > flushInterval_) {
      auto newRes = resourcePtr_->get_resource();
      int newIdx = (idx_ + 1) % 2;
      if (!newRes.empty()) {
        twoBuffer_[newIdx] = newRes;
        idx_ = newIdx;
        updateTime_ = now;
      } else {
        std::cerr << "Resource update failed: empty result\n";
      }
    }
  }

  std::unordered_map<std::string, std::string> getResource() {
    try {
      return resourcePtr_->get_resource();
    } catch (const std::exception& e) {
      std::cerr << "Exception in get_resource(): " << e.what() << "\n";
      return {};
    }
  }
};

// ------------------- DynamicResourceManager -------------------
class DynamicResourceManager {
 public:
  bool addDynamicResource(const std::string& name,
                          std::shared_ptr<IResource> resource,
                          int flushInterval) {
    auto dynRes = std::make_shared<DynamicResource>(resource, flushInterval);
    auto res = dynRes->getResourceWrapper();
    if (!res.empty()) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - lastAddedTime_)
              .count() < 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }

      int newIdx = (idx_ + 1) % 2;
      resourceMap_[newIdx][name] = dynRes;

      for (const auto& [key, val] : resourceMap_[idx_]) {
        if (resourceMap_[newIdx].find(key) == resourceMap_[newIdx].end()) {
          resourceMap_[newIdx][key] = val;
        }
      }

      idx_ = newIdx;
      lastAddedTime_ = std::chrono::steady_clock::now();
      return true;
    }
    std::cerr << "Failed to add dynamic resource: " << name << "\n";
    return false;
  }

  std::unordered_map<std::string, std::string> getDynamicResource(
      const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (resourceMap_[idx_].count(name)) {
      return resourceMap_[idx_][name]->getResourceWrapper();
    }
    return {};
  }

 private:
  std::mutex mutex_;
  int idx_ = 0;
  std::unordered_map<std::string, std::shared_ptr<DynamicResource>>
      resourceMap_[2];
  std::chrono::steady_clock::time_point lastAddedTime_ =
      std::chrono::steady_clock::now();
};

// ------------------- 示例资源类 -------------------
class MyResource : public IResource {
 public:
  std::unordered_map<std::string, std::string> get_resource() override {
    return {{"msg", "hello from MyResource"},
            {"time", std::to_string(std::time(nullptr))}};
  }
};

// // ------------------- 主函数测试 -------------------
// int main() {
//   DynamicResourceManager mgr;
//   auto res = std::make_shared<MyResource>();
//   mgr.addDynamicResource("res1", res, 2);  // 2秒刷新

//   while (true) {
//     auto data = mgr.getDynamicResource("res1");
//     std::cout << "Resource msg: " << data["msg"] << ", time: " << data["time"]
//               << std::endl;
//     std::this_thread::sleep_for(std::chrono::seconds(1));
//   }
// }
