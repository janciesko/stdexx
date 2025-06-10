#include <cstdio>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

static inline auto task1(void *) -> aligned_t {
  std::cout << "Hello from qthread task 1!" << std::endl;
  return 42;
}

static inline auto task2(void *) -> aligned_t {
  std::cout << "Hello from qthread task 2!" << std::endl;
  return 42;
}

static inline auto task3(void *) -> aligned_t {
  std::cout << "Hello from qthread task 3!" << std::endl;
  return 42;
}

// This is a wrapper for a qthread_f
// This wrapper does not provide the return value of qthread_f (the receiver
// does)
struct sender_wrapper {
  using sender_concept = stdexec::sender_t;
  qthread_f func;
  aligned_t feb;

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t *),
                                   stdexec::set_error_t(std::exception_ptr),
                                   stdexec::set_stopped_t()>;

  template <class Receiver>
  struct op {
    Receiver rcv;
    aligned_t *feb;
    qthread_f func;

    void start() & noexcept {
      int r = qthread_fork(func, NULL, feb);
      if (r) stdexec::set_error(std::move(rcv), std::exception_ptr());
      stdexec::set_value(std::move(rcv), feb);
    }
  };

  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), &feb, func};
  }

  stdexec::env<> get_env() const noexcept { return {}; }
};

auto main() -> int {
  /*Explicit use of custom senders*/
  stdexx::qthreads_context ctx;
  stdexec::sender auto s1 =
    on_qthreads(stdexec::just(), ctx, sender_wrapper{task1});
  stdexec::sender auto s2 =
    on_qthreads(stdexec::just(), ctx, sender_wrapper{task2});
  stdexec::sync_wait(stdexec::when_all(s1, s2));

  // Example sync_wait
  auto s3 = sender_wrapper{task3};
  auto [val] = stdexec::sync_wait(std::move(s3)).value();
  assert(*val == 42);

  // Example connect and start
  auto op =
    stdexec::connect(s3, empty_recv::expect_value_receiver{(aligned_t)42});
  stdexec::start(op);
  assert(*val == 42);
}

#elif (STDEXX_REFERENCE)

auto main() -> int {}

#else
error "Not implemented."
#endif
