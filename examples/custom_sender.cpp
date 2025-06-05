#include <cstdio>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

auto main() -> int {}

#elif (STDEXX_REFERENCE)


struct sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(int)>;

  /*
  - Op state is templated on receiver (callback)
  - Has start
  - Calls set_value on (rcv, value) synchronously
  - This uses compatible completion signature to provide value to rcv
  - This will invoke scheduler to invoke next sender (the recv is the glue)
  */
  template <class Receiver>
  struct op {
    Receiver rcv;
    void start() & noexcept {
      std::printf("success!\n");
      stdexec::set_value(std::move(rcv), 42);
    }
  };

  /*
  - Scheduler invokes sender via the tag invoke
  - Invoke accepts args: connect_t, self, and rcv (rcv is provided by the sender
  adapter)
  - Returns an op state intance, initialized with rcv.
  - ! The initialization of op sets the rcv (callback)
  - ! The callback is called on completion of op::start
  */
  template <class Receiver>
  friend auto
  tag_invoke(stdexx::connect_t, sender, Receiver rcv) -> op<Receiver> {
    return {std::move(rcv)};
  }
};

auto main() -> int {

  /*Sync_wait*/
  auto my_sender = sender{};
  auto [a] = stdexx::sync_wait(std::move(my_sender)).value();

  /*Via connect*/
  auto op = stdexx::connect(sender{}, empty_recv::expect_value_receiver{10});
  ex::start(op);

  return (a == 42) ? 1 : 0;
}

#else
error "Not implemented."
#endif
