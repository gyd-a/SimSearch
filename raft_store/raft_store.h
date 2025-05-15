#pragma once

#include <braft/raft.h>           // braft::Node braft::StateMachine
#include <braft/storage.h>        // braft::SnapshotWriter
#include <braft/util.h>           // braft::AsyncClosureGuard
#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <gflags/gflags.h>        // DEFINE_*
#include <sys/types.h>            // O_CREAT
#include <mutex>

#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"

#include "engine/engine.h"

class Block;

// Implements Closure which encloses RPC stuff
class BlockClosure : public braft::Closure {
 public:
  BlockClosure(Block* block, const raft_rpc::BlockRequest* request,
               raft_rpc::BlockResponse* response, butil::IOBuf* data,
               google::protobuf::Closure* done)
      : _block(block),
        _request(request),
        _response(response),
        _data(data),
        _done(done) {}
  ~BlockClosure() {}

  const raft_rpc::BlockRequest* request() const { return _request; }
  raft_rpc::BlockResponse* response() const { return _response; }
  void Run();
  butil::IOBuf* data() const { return _data; }

 private:
  // Disable explicitly delete
  Block* _block;
  const raft_rpc::BlockRequest* _request;
  raft_rpc::BlockResponse* _response;
  butil::IOBuf* _data;
  google::protobuf::Closure* _done;
};

class SharedFD : public butil::RefCountedThreadSafe<SharedFD> {
 public:
  explicit SharedFD(int fd) : _fd(fd) {}
  int fd() const { return _fd; }

 private:
  friend class butil::RefCountedThreadSafe<SharedFD>;
  ~SharedFD() {
    if (_fd >= 0) {
      while (true) {
        const int rc = ::close(_fd);
        if (rc == 0 || errno != EINTR) {
          break;
        }
      }
      _fd = -1;
    }
  }
  int _fd;
};

// Implementation of example::Block as a braft::StateMachine.
class Block : public braft::StateMachine {
 public:
  Block();
  ~Block();
  // Starts this node
  int start(const std::string& root_path, const std::string& group, 
            const std::string& conf, int port);
  // Impelements Service methods
  void write(const raft_rpc::BlockRequest* request,
             raft_rpc::BlockResponse* response, butil::IOBuf* data,
             google::protobuf::Closure* done);
  void read(const raft_rpc::BlockRequest* request,
            raft_rpc::BlockResponse* response, butil::IOBuf* buf);

  bool is_leader() const;
  // Shut this node down.
  void shutdown();
  // Blocking this thread until the node is eventually down.
  void join();

  std::string test_snapshort() {
    this->_node->snapshot(nullptr);
    return "";
  };

  const std::string& root_path() { return _root_path; }
  const std::string& group() { return _group; }
  const std::string& conf() { return _conf; }
  int port() { return _port; }

 private:
  typedef scoped_refptr<SharedFD> scoped_fd;

  scoped_fd get_fd() const {
    BAIDU_SCOPED_LOCK(_fd_mutex);
    return _fd;
  }
  friend class BlockClosure;
  void redirect(raft_rpc::BlockResponse* response);

  // @braft::StateMachine
  void on_apply(braft::Iterator& iter);

  struct SnapshotArg {
    scoped_fd fd;
    braft::SnapshotWriter* writer;
    braft::Closure* done;
    std::string root_path;
  };

  static int link_overwrite(const char* old_path, const char* new_path);

  static void* save_snapshot(void* arg);

  void on_snapshot_save(braft::SnapshotWriter* writer, braft::Closure* done);

  int on_snapshot_load(braft::SnapshotReader* reader);

  void on_leader_start(int64_t term);

  void on_leader_stop(const butil::Status& status);

  void on_shutdown();

  void on_error(const ::braft::Error& e);

  void on_configuration_committed(const ::braft::Configuration& conf);

  void on_stop_following(const ::braft::LeaderChangeContext& ctx);

  void on_start_following(const ::braft::LeaderChangeContext& ctx);
  // end of @braft::StateMachine

 private:
  mutable butil::Mutex _fd_mutex;
  braft::Node* volatile _node;
  butil::atomic<int64_t> _leader_term;
  scoped_fd _fd;
  std::string _root_path;
  std::string _group;
  std::string _conf;
  int _port;
  bool _check_term;
};


class RaftManager {
 public:
  ~RaftManager();

  std::string CreateBlock();

  std::string DeleteBlock();

  Block* block();

  std::mutex& mtx() { return _mtx; }

  std::string StartRaftServer(const std::string& root_path,
                              const std::string& group,
                              const std::string& conf, int port);

  static RaftManager& GetInstance();
 private:
  RaftManager() {};
  RaftManager(const RaftManager&) = delete;
  RaftManager(RaftManager&&) = delete;
  RaftManager& operator=(const RaftManager&) = delete;
  RaftManager& operator=(RaftManager&&) = delete;

  std::mutex _mtx;
  Block* _block = nullptr;
};