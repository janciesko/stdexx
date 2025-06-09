#include <cstdio>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

static inline auto task1(void *) -> aligned_t {
  std::cout << "Hello from qthread task 1!" << std::endl;
  return 0;
}

static inline auto task2(void *) -> aligned_t {
  std::cout << "Hello from qthread task 2!" << std::endl;
  return 0;
}

static inline auto task3(void *) -> aligned_t {
  std::cout << "Hello from qthread task 3!" << std::endl;
  return 42;
}

struct sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t)>;
  qthread_f func;

  template <class Receiver>
  struct op {
    Receiver rcv;

    //Option 1: Do not provide start() and run func in "on_qthreads_receiver"
    
    //Option 2: Fork off Qthread here. For now I am just setting 1 as a place holder
    //This is a design decision we can discuss
    void start() & noexcept { stdexec::set_value(std::move(rcv), 1); }
  };

  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv)};
  }
};

auto main() -> int {
  /*Explicit use of custom senders*/
  stdexx::qthreads_context ctx;
  stdexec::sender auto s1 =
    on_qthreads(stdexec::just(), ctx, sender{task1});
  stdexec::sender auto s2 =
    on_qthreads(stdexec::just(), ctx, sender{task2});
  stdexec::sync_wait(stdexec::when_all(s1, s2));

  //Example sync_wait
  auto my_sender = sender{task3};
  auto [a] = stdexec::sync_wait(std::move(my_sender)).value();

  //Example Connect and start
  auto op = stdexec::connect(my_sender, empty_recv::expect_value_receiver{42});
  stdexec::start(op);

  return (a == 42) ? 1 : 0;
}

#elif (STDEXX_REFERENCE)

auto main() -> int {}

#else
error "Not implemented."
#endif
