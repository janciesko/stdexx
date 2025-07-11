#include <cstdio>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

struct sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(int)>;

  int value_{0};

  /*
  - Op state is templated on receiver (callback)
  - Has start
  - Calls set_value on (rcv, value) synchronously
  - This uses compatible completion signature to provide value to rcv
  - This will mark completion of sender (the  recv is the callback
  */
  template <class Receiver>
  struct op {
    Receiver rcv;
    int val;

    void start() & noexcept {
      std::printf("success!\n");
      stdexec::set_value(std::move(rcv), val);
    }
  };

  /*
  - Connects sender and receiver
  - Returns op state templated on the Reiver type
  - ! The initialization of op sets the rcv (callback)
  - ! The callback is called on completion of op::start
  */
  template <stdexec::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), value_};
  }
};

auto main() -> int {
  /*Sync_wait*/
  auto my_sender = sender{42};
  auto [a] = stdexec::sync_wait(std::move(my_sender)).value();

  /*Via connect*/
  auto op = stdexec::connect(my_sender, empty_recv::expect_value_receiver{42});
  stdexec::start(op);

  return (a == 42) ? 1 : 0;
}

#elif (STDEXX_REFERENCE)

struct sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(int)>;

  int value_{0};

  /*
  - Op state is templated on receiver (callback)
  - Has start
  - Calls set_value on (rcv, value) synchronously
  - This uses compatible completion signature to provide value to rcv
  - This will mark completion of sender (the recv is the callback)
  */
  template <class Receiver>
  struct op {
    Receiver rcv;
    int val;

    void start() & noexcept {
      std::printf("success!\n");
      stdexec::set_value(std::move(rcv), val);
    }
  };

  /*
  - Connects sender and receiver
  - Returns op state templated on the Reiver type
  - ! The initialization of op sets the rcv (callback)
  - ! The callback is called on completion of op::start
  */
  template <stdexx::receiver Receiver>
  auto connect(Receiver rcv) noexcept -> op<Receiver> {
    return {std::move(rcv), value_};
  }
};

auto main() -> int {
  /*Sync_wait*/
  auto my_sender = sender{42};
  auto [a] = stdexx::sync_wait(std::move(my_sender)).value();

  /*Via connect*/
  auto op = stdexx::connect(my_sender, empty_recv::expect_value_receiver{42});
  stdexx::start(op);

  return (a == 42) ? 1 : 0;
}

#else
error "Not implemented."
#endif
