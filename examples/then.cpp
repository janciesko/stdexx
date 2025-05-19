
#include <cstdlib>
#include <iostream>
#include <stdexx.hpp>
#if 1
int main() {}
#else
int main() {
  auto x = then(stdexx::just(42), [](int i) {
    std::printf("Got: %d\n", i);
    return i;
  });

  // Prints "Got: 42"
  auto [a] = stdexx::sync_wait(std::move(x)).value();
  (void)a;
}
#endif
