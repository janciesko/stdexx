#pragma once

#include <catch2/catch_all.hpp>

namespace empty_recv {

namespace ex = stdexec;

#if defined(__clang__) && defined(__cpp_lib_tuple_like)
#define CHECK_TUPLE(...) CHECK((__VA_ARGS__))
#else
#define CHECK_TUPLE CHECK
#endif

struct recv0 {
  using receiver_concept = stdexec::receiver_t;

  void set_value() noexcept {}

  void set_stopped() noexcept {}

  void set_error(std::exception_ptr) noexcept {}
};

struct recv_int {
  using receiver_concept = stdexec::receiver_t;

  void set_value(int) noexcept {}

  void set_stopped() noexcept {}

  void set_error(std::exception_ptr) noexcept {}
};

struct recv0_ec {
  using receiver_concept = stdexec::receiver_t;

  void set_value() noexcept {}

  void set_stopped() noexcept {}

  void set_error(std::error_code) noexcept {}

  void set_error(std::exception_ptr) noexcept {}
};

struct recv_int_ec {
  using receiver_concept = stdexec::receiver_t;

  void set_value(int) noexcept {}

  void set_stopped() noexcept {}

  void set_error(std::error_code) noexcept {}

  void set_error(std::exception_ptr) noexcept {}
};

template <class _Env = ex::env<>>
class base_expect_receiver {
  std::atomic<bool> called_{false};
  _Env env_{};
public:
  using receiver_concept = stdexec::receiver_t;
  base_expect_receiver() = default;

  ~base_expect_receiver() { CHECK(called_.load()); }

  explicit base_expect_receiver(_Env env): env_(std::move(env)) {}

  base_expect_receiver(base_expect_receiver &&other):
    called_(other.called_.exchange(true)), env_(std::move(other.env_)) {}

  auto
  operator=(base_expect_receiver &&other) -> base_expect_receiver & = delete;

  void set_called() { called_.store(true); }

  auto get_env() const noexcept -> _Env { return env_; }
};

template <class _Env = ex::env<>>
struct expect_void_receiver : base_expect_receiver<_Env> {
  expect_void_receiver() = default;

  explicit expect_void_receiver(_Env env):
    base_expect_receiver<_Env>(std::move(env)) {}

  void set_value() noexcept { this->set_called(); }

  template <class... Ts>
  void set_value(Ts...) noexcept {
    FAIL_CHECK(
      "set_value called on expect_void_receiver with some non-void value");
  }

  void set_stopped() noexcept {
    FAIL_CHECK("set_stopped called on expect_void_receiver");
  }

  void set_error(std::exception_ptr) noexcept {
    FAIL_CHECK("set_error called on expect_void_receiver");
  }
};

struct env_tag {};

template <class Env = ex::env<>, class... Ts>
struct expect_value_receiver : base_expect_receiver<Env> {
  explicit(sizeof...(Ts) != 1) expect_value_receiver(Ts... vals):
    values_(std::move(vals)...) {}

  expect_value_receiver(env_tag, Env env, Ts... vals):
    base_expect_receiver<Env>(std::move(env)), values_(std::move(vals)...) {}

  void set_value(Ts const &...vals) noexcept {
    CHECK(values_ == std::tie(vals...));
    this->set_called();
  }

  template <class... Us>
  void set_value(Us const &...) noexcept {
    FAIL_CHECK(
      "set_value called with wrong value types on expect_value_receiver");
  }

  void set_stopped() noexcept {
    FAIL_CHECK("set_stopped called on expect_value_receiver");
  }

  template <class E>
  void set_error(E) noexcept {
    FAIL_CHECK("set_error called on expect_value_receiver");
  }
private:
  std::tuple<Ts...> values_;
};

} // namespace empty_recv
