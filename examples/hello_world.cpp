#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

auto main() -> int {
  stdexx::init();
  stdexec::sender auto begin = stdexec::schedule(stdexx::qthreads_scheduler{});
  stdexec::sender auto hi_again = stdexec::then(begin, [](int i) {
    std::cout << "Hello world! Have an int.\n";
    return 13;
  });
  auto val = stdexec::sync_wait(hi_again).value();
  std::cout << std::get<0>(val) << std::endl;

  stdexx::finalize();
  return 0;
}

#elif (STDEXX_REFERENCE)

#include "exec/static_thread_pool.hpp"

auto main() -> int {
  using namespace stdexx;
  exec::static_thread_pool ctx{8};
  scheduler auto sch = ctx.get_scheduler();

  sender auto begin = schedule(sch);
  sender auto hi_again = then(begin, [] {
    std::cout << "Hello world! Have an int.\n";
    return 13;
  });
  auto val = sync_wait(std::move(hi_again)).value();
  std::cout << std::get<0>(val) << std::endl;
  return 0;
}
#else
error "Not implemented."
#endif
