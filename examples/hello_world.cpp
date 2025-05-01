#include <cstdlib>
#include <iostream>

#include <stdexec/execution.hpp>

#include <stdexx.hpp>

int main() {
  stdexx::init();
  /* multi-shot currently doesn't work due to a quirk of our initial
  specialization of sync_wait.
   * We do have the correct overload provided now though.
  auto snd = stdexec::schedule(stdexx::scheduler{});
  stdexec::sync_wait(snd);
  */
  stdexec::sync_wait(stdexec::schedule(stdexx::scheduler{}));

  /* example of using stdexec::split.
   * We eventually should be able to do something similar with custom senders.
  int counter{};
  auto snd = stdexec::split(stdexec::just() | stdexec::then([&] { counter++;
  })); stdexec::sync_wait(snd | stdexec::then([] { })); stdexec::sync_wait(snd |
  stdexec::then([] { })); std::cout << counter << std::endl;*/

  /*stdexec::sender auto s =
    stdexec::schedule(stdexx::scheduler{}) | stdexec::then([] {
      std::cout << "Hello from user-level thread in then-functor!" << std::endl;
    }) |
    stdexec::bulk(20, [](int i) {
      std::cout << "Hello from user-level thread in bulk!(i=" << i << ")"
                << std::endl;
    });
  stdexec::sync_wait(std::move(s));*/

  stdexx::finalize();
  return 0;
}
