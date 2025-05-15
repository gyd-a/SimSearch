

#include "raft_store/raft_store.h"
#include "config/conf.h"
#include <butil/logging.h>

// Implementation of example::Block as a braft::StateMachine.

Block::Block() : _node(NULL), _leader_term(-1), _fd(NULL) {}

Block::~Block() { delete _node; }

// Starts this node
int Block::start(const std::string& root_path, const std::string& group,
                 const std::string& conf, int port) {
  _root_path = root_path;
  _group = group;
  _conf = conf;
  _port = port;
  _check_term = true;
  if (!butil::CreateDirectory(butil::FilePath(_root_path))) {
    LOG(ERROR) << "Fail to create directory " << _root_path;
    return -1;
  }
  std::string data_path = _root_path + "/data";
  int fd = ::open(data_path.c_str(), O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    PLOG(ERROR) << "Fail to open " << data_path;
    return -1;
  }
  _fd = new SharedFD(fd);
  butil::EndPoint addr(butil::my_ip(), port);
  braft::NodeOptions node_options;
  if (node_options.initial_conf.parse_from(conf) != 0) {
    LOG(ERROR) << "Fail to parse configuration `" << conf << '\'';
    return -1;
  }
  node_options.election_timeout_ms = toml_conf_.ps.raft_election_timeout_ms;
  node_options.fsm = this;
  node_options.node_owns_fsm = false;
  node_options.snapshot_interval_s = toml_conf_.ps.raft_snap_interval_s;
  std::string prefix = "local://" + _root_path;
  node_options.log_uri = prefix + "/log";
  node_options.raft_meta_uri = prefix + "/raft_meta";
  node_options.snapshot_uri = prefix + "/snapshot";
  node_options.disable_cli = true;
  braft::Node* node = new braft::Node(_group, braft::PeerId(addr));
  if (node->init(node_options) != 0) {
    LOG(ERROR) << "Fail to init raft node";
    delete node;
    return -1;
  }
  _node = node;
  return 0;
}

// Impelements Service methods
void Block::write(const raft_rpc::BlockRequest* request,
                  raft_rpc::BlockResponse* response, butil::IOBuf* data,
                  google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // Serialize request to the replicated write-ahead-log so that all the
  // peers in the group receive this request as well.
  // Notice that _value can't be modified in this routine otherwise it
  // will be inconsistent with others in this group.

  // Serialize request to IOBuf
  DLOG(INFO) << "************ Block::write() function run ************";
  const int64_t term = _leader_term.load(butil::memory_order_relaxed);
  if (term < 0) {
    DLOG(ERROR) << "************ Block::write() return redirect ************";
    return redirect(response);
  }
  butil::IOBuf log;
  const uint32_t meta_size_raw = butil::HostToNet32(request->ByteSizeLong());
  log.append(&meta_size_raw, sizeof(uint32_t));
  butil::IOBufAsZeroCopyOutputStream wrapper(&log);
  if (!request->SerializeToZeroCopyStream(&wrapper)) {
    LOG(ERROR) << "******* Block::write() Fail to serialize request *******";
    response->set_success(false);
    return;
  }
  log.append(*data);
  // Apply this log as a braft::Task
  braft::Task task;
  task.data = &log;
  // This callback would be iovoked when the task actually excuted or fail
  task.done =
      new BlockClosure(this, request, response, data, done_guard.release());
  if (_check_term) {
    // ABA problem can be avoid if expected_term is set
    task.expected_term = term;
  }
  // Now the task is applied to the group, waiting for the result.
  DLOG(INFO) << "************ Block::write() run _node->apply ************";
  return _node->apply(task);
}

void Block::read(const raft_rpc::BlockRequest* request,
                 raft_rpc::BlockResponse* response, butil::IOBuf* buf) {
#ifndef DEBUG_BUILD
  std::string dkeys;
  auto docs_info = request->docs_info();
  for (int i = 0; i < docs_info.size(); ++i) {
    dkeys += docs_info[i].key() + ", ";
  }
  DLOG(INFO) << "read() api, request key_list:[" << dkeys << "]";
#endif
  // In consideration of consistency. GetRequest to follower should be rejected.
  if (!is_leader()) {
    // This node is a follower or it's not up-to-date. Redirect to
    // the leader if possible.
    return redirect(response);
  }

  if (request->docs_info().size() < 0) {
    response->set_success(false);
    response->set_msg("request docs_info size is 0");
    LOG(ERROR) << "request docs_info size is 0";
    return;
  }

  // This is the leader and is up-to-date. It's safe to respond client
  scoped_fd fd = get_fd();
  butil::IOPortal portal;
  // int offset = atoi(request->key_list()[0].c_str()) * request->data_size();
  // const ssize_t nr = braft::file_pread(&portal, fd->fd(), offset, request->data_size());
  std::vector<int32_t> keys, doc_lengths;
  const auto& doc_infos = request->docs_info();
  for (int i = 0; i < doc_infos.size(); ++i) {
    keys.push_back(atoi(doc_infos[i].key().c_str()));
  }
  std::string data_buf;
  Engine::GetInstance().Mget(keys, doc_lengths, data_buf);
  portal.append(data_buf);
  for (int i = 0; i < doc_infos.size(); ++i) {
    auto p_doc_info = response->add_docs_info();
    p_doc_info->set_key(doc_infos[i].key());
    p_doc_info->set_doc_len(doc_lengths[i]);
  }
  response->set_key_type(request->key_type());
  int nr = 11;
  if (nr < 0) {
    // Some disk error occurred, shutdown this node and another leader
    // will be elected
    PLOG(ERROR) << "Fail to read from fd=" << fd->fd();
    _node->shutdown(NULL);
    response->set_success(false);
    return;
  }
  buf->swap(portal);
  // if (buf->length() < (size_t)request->data_size()) {
  //   buf->resize(request->data_size());
  // }
  response->set_success(true);
}

bool Block::is_leader() const {
  return _leader_term.load(butil::memory_order_acquire) > 0;
}

// Shut this node down.
void Block::shutdown() {
  if (_node) {
    _node->shutdown(NULL);
  }
}

// Blocking this thread until the node is eventually down.
void Block::join() {
  if (_node) {
    _node->join();
  }
}

void Block::redirect(raft_rpc::BlockResponse* response) {
  response->set_success(false);
  if (_node) {
    braft::PeerId leader = _node->leader_id();
    if (!leader.is_empty()) {
      response->set_redirect(leader.to_string());
    }
  }
}

// @braft::StateMachine
void Block::on_apply(braft::Iterator& iter) {
  // A batch of tasks are committed, which must be processed through
  // |iter|
  int test_id = 0;
  for (; iter.valid(); iter.next()) {
    DLOG(INFO) << "************ on_apply() function impl *****************";
    raft_rpc::BlockResponse* response = NULL;
    // This guard helps invoke iter.done()->Run() asynchronously to
    // avoid that callback blocks the StateMachine
    braft::AsyncClosureGuard closure_guard(iter.done());
    butil::IOBuf data;
    std::vector<std::pair<int32_t, int16_t>> doc_infos;
    if (iter.done()) {
      DLOG(INFO) << "************* on_apply() function go in iter.done() **************";
      // This task is applied by this node, get value from this closure to avoid additional parsing.
      BlockClosure* c = dynamic_cast<BlockClosure*>(iter.done());
      // offset = atoi(c->request()->key_list()[0].c_str()) * c->request()->data_size();
      data.swap(*(c->data()));
      response = c->response();
      const auto& pb_doc_infos = c->request()->docs_info();
      for (int i = 0; i < pb_doc_infos.size(); ++i) {
        auto doc_info = pb_doc_infos[i];
        int32_t key = atoi(doc_info.key().c_str());
        int16_t doc_len = doc_info.doc_len();
        doc_infos.emplace_back(key, doc_len);
      }
      test_id = c->request()->test_id();
    } else {
      DLOG(INFO) << "********** on_apply() function go in iter.done()=false ************";
      // Have to parse BlockRequest from this log.
      uint32_t meta_size = 0;
      butil::IOBuf saved_log = iter.data();
      saved_log.cutn(&meta_size, sizeof(uint32_t));
      // Remember that meta_size is in network order which hould be
      // covert to host order
      meta_size = butil::NetToHost32(meta_size);
      butil::IOBuf meta;
      saved_log.cutn(&meta, meta_size);
      butil::IOBufAsZeroCopyInputStream wrapper(meta);
      raft_rpc::BlockRequest request;
      CHECK(request.ParseFromZeroCopyStream(&wrapper));
      data.swap(saved_log);
      // offset = atoi(request.key_list()[0].c_str()) * request.data_size();
      const auto& pb_doc_infos = request.docs_info();
      for (int i = 0; i < pb_doc_infos.size(); ++i) {
        auto doc_info = pb_doc_infos[i];
        int32_t key = atoi(doc_info.key().c_str());
        int16_t doc_len = doc_info.doc_len();
        doc_infos.emplace_back(key, doc_len);
      }
      test_id = request.test_id();
    }
    
    std::string data_buf;
    data.copy_to(&data_buf);
    DLOG(INFO) << "*********** on_apply() test_id:" << test_id << " *************";
    // const ssize_t nw = braft::file_pwrite(data, _fd->fd(), offset);
    std::string msg = Engine::GetInstance().BatchAdd(doc_infos, data_buf);
    DLOG(INFO) << "********* on_apply() Engine BatchAdd msg:[" << msg << "] ********";
    // if (msg.size() > 0 || test_id > 0) {
    if (false) {
      LOG(ERROR) << "********* on_apply() Write Engine fail, msg:[" << msg << "] **********";
      if (response) {
        response->set_success(false);
      }
      // Let raft run this closure.
      closure_guard.release();
      // Some disk error occurred, notify raft and never apply any data ever after
      iter.set_error_and_rollback();
      return;
    }

    if (response) {
      response->set_success(true);
    }

    // The purpose of following logs is to help you understand the way
    // this StateMachine works.
    // Remove these logs in performance-sensitive servers.
    // LOG_IF(INFO, FLAGS_log_applied_task)
    
    LOG(INFO) << "******* on_apply() Write " << data.size() << " bytes success ********";
    // LOG(INFO) << "Write " << data.size() << " bytes"
    //           << " from offset=" << offset << " at log_index=" << iter.index();
  }
}

int Block::link_overwrite(const char* old_path, const char* new_path) {
  if (::unlink(new_path) < 0 && errno != ENOENT) {
    PLOG(ERROR) << "Fail to unlink " << new_path;
    return -1;
  }
  return ::link(old_path, new_path);
}

void* Block::save_snapshot(void* arg) {
  LOG(INFO) << "============start impl snapshot===========";
  SnapshotArg* sa = (SnapshotArg*)arg;
  std::unique_ptr<SnapshotArg> arg_guard(sa);
  // Serialize StateMachine to the snapshot
  brpc::ClosureGuard done_guard(sa->done);
  std::string snapshot_path = sa->writer->get_path();
  std::vector<std::string> filenames;
  Engine::GetInstance().Snapshot(snapshot_path, filenames);
  LOG(INFO) << "==== complete impl snapshot, =snapshot_path[" << snapshot_path
            << "] =======";
  for (const auto& filename : filenames) {
    if (sa->writer->add_file(filename) != 0) {
      sa->done->status().set_error(EIO, "Fail to add file[" + filename + "] to writer");
    }
  }

/*
  std::string snapshot_path = sa->writer->get_path() + "/data";
  // Sync buffered data before
  int rc = 0;
  LOG(INFO) << "Saving snapshot to " << snapshot_path;
  for (; (rc = ::fdatasync(sa->fd->fd())) < 0 && errno == EINTR;) {
  }
  if (rc < 0) {
    sa->done->status().set_error(EIO, "Fail to sync fd=%d : %m", sa->fd->fd());
    return NULL;
  }
  std::string data_path = sa->root_path + "/data";
  if (link_overwrite(data_path.c_str(), snapshot_path.c_str()) != 0) {
    sa->done->status().set_error(EIO, "Fail to link data : %m");
    return NULL;
  }

  // Snapshot is a set of files in raft. Add the only file into the writer here.
  if (sa->writer->add_file("data") != 0) {
    sa->done->status().set_error(EIO, "Fail to add file to writer");
    return NULL;
  }
*/
  return NULL;
}

void Block::on_snapshot_save(braft::SnapshotWriter* writer,
                             braft::Closure* done) {
  // Save current StateMachine in memory and starts a new bthread to avoid
  // blocking StateMachine since it's a bit slow to write data to disk
  // file.
  SnapshotArg* arg = new SnapshotArg;
  arg->fd = _fd;
  arg->writer = writer;
  arg->done = done;
  arg->root_path = _root_path;
  bthread_t tid;
  bthread_start_urgent(&tid, NULL, save_snapshot, arg);
}

int Block::on_snapshot_load(braft::SnapshotReader* reader) {
  // Load snasphot from reader, replacing the running StateMachine
  CHECK(!is_leader()) << "Leader is not supposed to load snapshot";
  std::string snapshot_path = reader->get_path();
  auto msg = Engine::GetInstance().CreatOrLoad(snapshot_path);
  if (msg.size() > 0) {
    return -1;
  }
/*
  if (reader->get_file_meta("data", NULL) != 0) {
    LOG(ERROR) << "Fail to find `data' on " << reader->get_path();
    return -1;
  }
  // reset fd
  _fd = NULL;
  std::string snapshot_path = reader->get_path() + "/data";
  std::string data_path = _root_path + "/data";
  if (link_overwrite(snapshot_path.c_str(), data_path.c_str()) != 0) {
    PLOG(ERROR) << "Fail to link data";
    return -1;
  }
  // Reopen this file
  int fd = ::open(data_path.c_str(), O_RDWR, 0644);
  if (fd < 0) {
    PLOG(ERROR) << "Fail to open " << data_path;
    return -1;
  }
  _fd = new SharedFD(fd);
*/
  return 0;
}

void Block::on_leader_start(int64_t term) {
  _leader_term.store(term, butil::memory_order_release);
  LOG(INFO) << "Node becomes leader";
}

void Block::on_leader_stop(const butil::Status& status) {
  _leader_term.store(-1, butil::memory_order_release);
  LOG(INFO) << "Node stepped down : " << status;
}

void Block::on_shutdown() { LOG(INFO) << "This node is down"; }

void Block::on_error(const ::braft::Error& e) {
  LOG(ERROR) << "Met raft error " << e;
}

void Block::on_configuration_committed(const ::braft::Configuration& conf) {
  LOG(INFO) << "Configuration of this group is " << conf;
}

void Block::on_stop_following(const ::braft::LeaderChangeContext& ctx) {
  LOG(INFO) << "Node stops following " << ctx;
}

void Block::on_start_following(const ::braft::LeaderChangeContext& ctx) {
  LOG(INFO) << "Node start following " << ctx;
}

void BlockClosure::Run() {
  // Auto delete this after Run()
  std::unique_ptr<BlockClosure> self_guard(this);
  // Repsond this RPC.
  brpc::ClosureGuard done_guard(_done);
  if (status().ok()) {
    return;
  }
  // std::cout << "========================== offset:" << _request->offset()
  //           << " size:" << _request->size() << "\n";
  // Try redirect if this request failed.
  _block->redirect(_response);
}


RaftManager& RaftManager::GetInstance() {
  static RaftManager braft_mrg;
  return braft_mrg;
}

RaftManager::~RaftManager() {
  DeleteBlock();
}

std::string RaftManager::CreateBlock() {
  DeleteBlock();
  _block = new Block();
  return "";
}

std::string RaftManager::DeleteBlock() {
  LOG(WARNING) << "************ delete raft block **************";
  if (_block) {
    delete _block;
    _block = nullptr;
  }
  return "";
}

Block* RaftManager::block() { return _block; }

std::string RaftManager::StartRaftServer(const std::string& root_path,
                                         const std::string& group,
                                         const std::string& conf, int port) {
  LOG(INFO) << "StartRaftServer, root_path:[" << root_path << "] group:["
            << group << "] conf:[" << conf << "] port:[" << port << "]";
  if (_block == nullptr) {
    std::string msg = "block is nullptr";
    LOG(ERROR) << msg;
    return msg;
  }
  // It's ok to start Block
  if (_block->start(root_path, group, conf, port) != 0) {
    std::string msg = "Fail to start Block";
    LOG(ERROR) << msg;
    return msg;
  }
  LOG(INFO) << "Block service is running successful.";
  return "";
}
