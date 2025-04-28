#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

int main() {
  stdexx::init();
  // stdexec::schedule(stdexx::scheduler{})
  stdexec::sync_wait(stdexec::schedule(stdexx::scheduler{}));
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
