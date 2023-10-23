#pragma once
#include "Executor.hpp"
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <cstddef>
#include <functional>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <vector>

template <typename T> using iptr = boost::intrusive_ptr<T>;

template <typename> struct BaseAsyncTask;

template <typename T, typename... Args>
struct BaseAsyncTask<T(Args...)> : public Task {

  std::function<T(Args...)> _fnc;
  std::tuple<Args...> _args;

  BaseAsyncTask(auto &&fnc, Args... args)
      : _fnc(std::move(fnc)), _args(args...) {}

  BaseAsyncTask(BaseAsyncTask &) = delete;

  void set_args(Args... args) { _args = std::tuple<Args...>{args...}; }

  virtual void operator()() override {
    const auto argsize = sizeof...(Args);
    const bool isvoid = std::is_void_v<T>;
    if (_father != nullptr) {
      if constexpr (isvoid) {
        if constexpr (argsize > 0)
          _fnc(_father->result().reintepret<Args...>());
        else
          _fnc();
      } else {
        if constexpr (argsize > 0) {
          _result.emplace(_fnc(_father->result().reintepret<Args...>()));
        } else {
          _result.emplace(_fnc);
        }
      }
      resolve();
    } else {
      if constexpr (isvoid) {
        if constexpr (argsize > 0)
          std::apply(_fnc, _args);
        else
          _fnc();
      } else {
        if constexpr (sizeof...(Args) > 0) {
          _result.emplace(std::apply(_fnc, _args));
        } else {
          _result.emplace(_fnc());
        }
      }
    }
  }
};

struct BaseAsync {
protected:
  iptr<Task> _actual;

public:
  BaseAsync() {}
  BaseAsync(iptr<Task> actual) : _actual(actual) {}

  void start_impl(iptr<Task> task) { Scheduler::main().schedule(task); }

  void then_impl(iptr<Task> task) { _actual->set_then(task); }

  void fail_impl(iptr<Task> task) { task->set_failure(task); }
};

template <typename T, typename... Args> struct Async : BaseAsync {

public:
  Async() : BaseAsync() {}
  Async(iptr<Task> actual) : BaseAsync(actual) {}

  template <typename F>
    requires(std::is_invocable_v<F, Args...>)
  auto &&start(F &&fnc, Args... args) {
    auto task = iptr<Task>(new BaseAsyncTask{std::forward<F>(fnc)});
    _actual = task;
    start_impl(task);
    return *this;
  }
};

template <typename T, typename... Args> struct AsyncLoop : public Task {
  std::function<T(Args...)> _iterFnc;
  std::function<bool(T)> _terminator;
  std::tuple<Args...> _args;
  AsyncLoop(std::function<bool(T)> predicate, std::function<T(Args...)> iterFnc,
            Args... args)
      : _terminator(predicate), _iterFnc(iterFnc), _args(args...) {
    Scheduler::main().schedule(boost::intrusive_ptr<Task>{this});
  }

  virtual void operator()() {
    T res = std::apply(_iterFnc, _args);
    if (_terminator(res)) {
      resolve();
    } else {
      Scheduler::main().schedule(this);
    }
  }
};

template <> struct AsyncLoop<void> : public Task {
  std::function<void(boost::intrusive_ptr<Task>)> _iterFnc;
  AsyncLoop(std::function<void(boost::intrusive_ptr<Task>)> iterFnc)
      : _iterFnc(iterFnc) {
    Scheduler::main().schedule(boost::intrusive_ptr<Task>(this));
  }
  virtual void operator()() {
    auto itrPtr = boost::intrusive_ptr<Task>(this);
    _iterFnc(itrPtr);
    Scheduler::main().schedule(itrPtr);
  }
};

struct AsyncMultiLoop : public Task {
  std::function<void(size_t)> _iterLambda;
  std::mutex _lock{};
  size_t _iterations;
  AsyncMultiLoop(size_t iterations, std::function<void(size_t)> iterLambda);
  virtual void operator()();
};