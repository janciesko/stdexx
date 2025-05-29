#include <stdexx.hpp>
#include <cstdio>

#if (STDEXX_QTHREADS)

//tbd

#elif(STDEXX_REFERENCE)

struct my_sender {
  using sender_concept = stdexec::sender_t;
  using completion_signatures = stdexec::
    completion_signatures<stdexec::set_value_t(int)>;

  template <class Receiver>
  struct op {
    Receiver rcv;
    void start() & noexcept {      
        std::printf("success!\n");
        stdexec::set_value(std::move(rcv), 42);
    }
  };

  template <class Receiver>
  friend auto tag_invoke(stdexec::connect_t, my_sender, Receiver rcv) -> op<Receiver> {
    return {std::move(rcv)};
  }

};

auto main() -> int {
  auto algorithm = stdexx::then(stdexx::just(), my_sender{});
  auto [a] = stdexec::sync_wait(std::move(algorithm)).value();
  return (a == 42) ? 1: 0;
}



#else
error "Not implemented."
#endif