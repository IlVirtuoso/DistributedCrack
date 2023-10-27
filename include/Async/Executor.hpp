#pragma once
#include "Concepts.hpp"
#include "MultiThread/AutoResetEvent.hpp"
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

class DynData {
private:
  void *ptr = nullptr;
  size_t size;

public:
  template <typename T> void emplace(T value) {
    ptr = malloc(sizeof(T));
    std::memcpy(ptr, &value, sizeof(T));
    size = sizeof(T);
  }

  template <typename T> T &reintepret() {
    return reinterpret_cast<T &>(*((T *)ptr));
  }

  bool has_value() const { return ptr != nullptr; }

  ~DynData() {
    if (ptr != nullptr) {
      free(ptr);
      ptr = nullptr;
    }
  }
};

class Executor;
class Task {
  friend class Executor;
  ManualResetEvent _executed{false};

public:
  enum AsyncState { WAITING_EXECUTION, RESOLVED, FAILED };

protected:
  enum AsyncType { START, THEN, RESULT };
  AsyncState _state = WAITING_EXECUTION;
  DynData _result;
  std::mutex _lock{};
  sptr<Task> _thenHandler{};
  sptr<Task> _failureHandler{};
  sptr<Task> _father;
  void resolve(bool failed = false);

public:
  Task() {}
  virtual void operator()() = 0;
  virtual ~Task() = default;
  Task(Task &) = delete;
  AsyncState state() const { return _state; }
  DynData &result() { return this->_result; }
  void wait() { _executed.WaitOne(); }
  void set_then(sptr<Task> task);
  void set_failure(sptr<Task> task);
  void cancel();

  template <typename T> void make_data(T &&data) { _result.emplace(data); }
};

class PostableTask : public Task {
private:
  std::function<void()> _fnc;

public:
  PostableTask(std::function<void()> fnc) : Task(), _fnc(fnc) {}
  virtual void operator()() {
    _fnc();
    resolve();
  }
};

class Executor {
  friend class Scheduler;

public:
  enum State { IDLE, PROCESSING, WAITING_EXECUTION };

protected:
  std::thread *_executingThread = nullptr;
  State status = IDLE;
  bool _end = false;
  std::mutex _lock;
  sptr<Task> _currentExecution{};
  void push(sptr<Task> task);
  ManualResetEvent onCompleted{false};
  ManualResetEvent onAssigned{false};

  inline sptr<Task> take() {
    std::lock_guard l{_lock};
    return _currentExecution;
  }

  inline void reset() {
    std::lock_guard l{_lock};
    _currentExecution = nullptr;
  }

public:
  Executor();
  bool assign(sptr<Task> task);
  sptr<Task> post(std::function<void()> f);
  void start();
  void wait_termination();
  State state() const { return status; }
  void stop();
  ~Executor();
};

class Scheduler {
private:
  std::vector<sptr<Executor>> _executors{};
  int _previousCount = 0;
  bool _end = false;
  std::thread *_executionThread = nullptr;
  std::mutex schedLock{};
  int _maxExecutors = 10;
  int WaitAnyExecutorIdle();
  void forExecutors(std::function<void(sptr<Executor> &e)>);

protected:
  bool assign(sptr<Task> task);
  ManualResetEvent onThreadTerminated{false};

public:
  sptr<Task> post(std::function<void()> f);
  void schedule(sptr<Task> task);
  void start();
  void stop();
  void reset();

  inline Scheduler &setMaxExecutors(int maxThreads) {
    _maxExecutors = maxThreads;
    return *this;
  }
  static Scheduler &main();
  std::optional<sptr<Task>> take();
  bool empty() const;

protected:
  std::queue<sptr<Task>> mq{};
  Scheduler();
  ~Scheduler();
  static Scheduler *_instance;
  void routine();
};

void intrusive_ptr_add_ref(Task *p);
void intrusive_ptr_add_ref(Executor *p);
void intrusive_ptr_release(Task *p);
void intrusive_ptr_release(Executor *p);