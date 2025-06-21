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

  // Basic example:
  stdexec::sender auto begin = stdexec::schedule(stdexx::qthreads_scheduler{});
  stdexec::sender auto hi_again = stdexec::then(std::move(begin), []() {
    std::cout << "Hello world! Have an int.\n";
    return 13;
  });
  auto val = stdexec::sync_wait(std::move(hi_again)).value();
  std::cout << std::get<0>(val) << std::endl;

  // A more complete set of usage examples:
  stdexec::sync_wait(
    stdexec::schedule(stdexx::qthreads_scheduler{}) | stdexec::then([]() {
      std::cout << "hello from a then lambda returning void" << std::endl;
    }));

  stdexec::sender auto then =
    stdexec::schedule(stdexx::qthreads_scheduler{}) | stdexec::then([]() {
      std::cout << "hello from then returning a value" << std::endl;
      return (aligned_t)1;
    });
  auto val3 = stdexec::sync_wait(std::move(then)).value();
  std::cout << "value returned from then: " << std::get<0>(val3) << std::endl;

  auto val4 = stdexec::sync_wait(
                stdexx::qthreads_func_sender<decltype(&test_func), aligned_t>(
                  &test_func, 4ull))
                .value();
  stdexx::qthreads_func_sender<decltype(&test_func), aligned_t> snd2(&test_func,
                                                                     4ull);
  auto val5 = stdexec::sync_wait(std::move(snd2)).value();
  std::cout << "values from qthreads func sender: " << std::get<0>(val4) << ", "
            << std::get<0>(val5) << std::endl;

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
