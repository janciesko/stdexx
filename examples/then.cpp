
#include <iostream>
#include <stdexec/execution.hpp>

int main() {
  auto x = stdexec::then(stdexec::just(42), [](int i) {
    std::printf("Got: %d\n", i);
    return i;
  });

  // Prints "Got: 42"
  auto [a] = stdexec::sync_wait(std::move(x)).value();
  (void)a;
}
