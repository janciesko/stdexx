#include <exec/static_thread_pool.hpp>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

auto main() -> int {}; // todo

#elif (STDEXX_REFERENCE)
int main() {
  // Declare a pool of 3 worker threads:
  exec::static_thread_pool pool(3);

  auto sched = pool.get_scheduler();

  auto fun = [](int i) { return i * i; };
  auto work = stdexx::when_all(
    stdexx::starts_on(sched, stdexx::just(0) | stdexx::then(fun)),
    stdexx::starts_on(sched, stdexx::just(1) | stdexx::then(fun)),
    stdexx::starts_on(sched, stdexx::just(2) | stdexx::then(fun)));

  // Launch the work and wait for the result
  auto [i, j, k] = stdexx::sync_wait(std::move(work)).value();

  // Print the results:
  std::printf("%d %d %d\n", i, j, k);
}
#else
error "Not implemented."
#endif
