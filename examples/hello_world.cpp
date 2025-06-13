#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

aligned_t test_func(aligned_t val) noexcept {
  std::cout << "hello, passed value is: " << val << std::endl;
  return val;
}

auto main() -> int {
  stdexx::init();
  /*
  stdexec::sender auto begin = stdexec::schedule(stdexx::qthreads_scheduler{});
  stdexec::sender auto hi_again = stdexec::then(begin, [](int i) {
    std::cout << "Hello world! Have an int.\n";
    return 13;
  });
  auto val = stdexec::sync_wait(hi_again).value();
  std::cout << std::get<0>(val) << std::endl;
  */
  // TODO: multi-shot sender API should mean that the hello shows up twice.
  // currently it's showing three times, so we need to fix that.
  auto val1 =
    stdexec::sync_wait(stdexec::schedule(stdexx::qthreads_scheduler{})).value();
  stdexec::sender auto snd1 = stdexec::schedule(stdexx::qthreads_scheduler{});
  auto val2 = stdexec::sync_wait(std::move(snd1)).value();
  std::cout << "values from default sender from qthreads scheduler: "
            << std::get<0>(val1) << ", " << std::get<0>(val2) << std::endl;

  auto val3 = stdexec::sync_wait(
                stdexx::qthreads_func_sender<decltype(&test_func), aligned_t>(
                  &test_func, 4ull))
                .value();
  stdexx::qthreads_func_sender<decltype(&test_func), aligned_t> snd2(&test_func,
                                                                     4ull);
  auto val4 = stdexec::sync_wait(std::move(snd2)).value();
  std::cout << "values from qthreads func sender: " << std::get<0>(val3) << ", "
            << std::get<0>(val4) << std::endl;

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
