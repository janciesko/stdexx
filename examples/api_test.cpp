#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

struct example_one {
  example_one() {
    stdexec::sender auto my_sender =
      stdexec::schedule(stdexx::qthreads_scheduler{});
    auto val = stdexec::sync_wait(my_sender).value();
    std::cout << std::get<0>(val) << std::endl;
  }
};

struct example_two {
  example_two() {
    stdexec::sender auto my_sender =
      stdexec::schedule(stdexx::qthreads_scheduler{});
    stdexec::sender auto algorithm =
      stdexec::then(my_sender, [](int arg) { return 1; });

    auto val = stdexec::sync_wait(algorithm).value();
    std::cout << std::get<0>(val) << std::endl;
  }
};

struct example_three {
  example_three() {
    stdexec::sender auto s =
      stdexec::schedule(stdexx::qthreads_scheduler{}) |
      stdexec::then([](int arg) {
        std::cout << "Hello from user-level thread in then-functor!"
                  << std::endl;
      }) |
      stdexec::bulk(stdexec::par, 20, [](int i) {
        std::cout << "Hello from user-level thread in bulk!(i=" << i << ")"
                  << std::endl;
      });
    stdexec::sync_wait(std::move(s));
  }
};

struct example_four {
  example_four() {
   /* auto snd = stdexec::schedule(thread_pool.scheduler()) |
               stdexec::then([] { return 123; }) |
               stdexec::continues_on(stdexx::qthreads_scheduler{}) |
               stdexec::then([](int i) { return 123 * 5; }) |
               stdexec::continues_on(thread_pool.scheduler()) |
               stdexec::then([](int i) { return i - 5; });
    auto [result] = stdexec::sync_wait(snd).value();*/
  } // result == 610
};

auto main() -> int {
  stdexx::init();
  using types =
    std::variant<example_one, example_two, example_three, example_four>;
  std::vector<types> v = {{}, {}, {}, {}, {}};
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

  sender auto add_42 = then(hi_again, [](int arg) { return arg + 42; });
  auto [i] = sync_wait(std::move(add_42)).value();
  std::cout << "Result: " << i << std::endl;

  // Sync_wait provides a run_loop scheduler
  std::tuple<run_loop::__scheduler> t = sync_wait(get_scheduler()).value();
  (void)t;

  auto y = let_value(get_scheduler(), [](auto sched) {
    return starts_on(sched, then(just(), [] {
                       std::cout << "from run_loop\n";
                       return 42;
                     }));
  });
  sync_wait(std::move(y));
  sync_wait(when_all(just(42), get_scheduler(), get_stop_token()));
  return 0;
}
#else
error "Not implemented."
#endif
