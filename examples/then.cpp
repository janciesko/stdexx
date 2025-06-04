
#include <iostream>
#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

auto main() -> int {}; // todo

#elif (STDEXX_REFERENCE)
int main() {
  auto x = stdexx::then(stdexx::just(42), [](int i) {
    std::printf("Got: %d\n", i);
    return i;
  });

  // Prints "Got: 42"
  auto [a] = stdexx::sync_wait(std::move(x)).value();
  (void)a;
}
#else
error "Not implemented."
#endif
