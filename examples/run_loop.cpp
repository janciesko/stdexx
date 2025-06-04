#include <cstdio>
#include <stdexx.hpp>
#include <thread>
using namespace std::literals;

#if (STDEXX_QTHREADS)

auto main() -> int {}; // todo

#elif (STDEXX_REFERENCE)

int main() {
  stdexx::run_loop loop;

  std::jthread worker([&](std::stop_token st) {
    std::stop_callback cb{st, [&] { loop.finish(); }};
    loop.run();
  });

  stdexx::sender auto hello = stdexx::just("hello world"s);
  stdexx::sender auto print = std::move(hello) | stdexx::then([](auto msg) {
                                std::puts(msg.c_str());
                                return 0;
                              });

  stdexx::scheduler auto io_thread = loop.get_scheduler();
  stdexx::sender auto work = stdexx::starts_on(io_thread, std::move(print));

  auto [result] = stdexx::sync_wait(std::move(work)).value();

  return result;
}

#else
error "Not implemented."
#endif

