#include <stdexx.hpp>
#include <cstdio>

#if (STDEXX_QTHREADS)

auto main() -> int {}

#elif(STDEXX_REFERENCE)

struct fail_some {
  using sender_concept = stdexec::sender_t;
  using completion_signatures = stdexec::
    completion_signatures<stdexec::set_value_t(int), stdexec::set_error_t(std::exception_ptr)>;

  template <class R>
  struct op {
    R r_;

    void start() & noexcept {
      static int i = 0;
      if (++i < 3) {
        std::printf("fail!\n");
        stdexec::set_error(std::move(r_), std::exception_ptr{});
      } else {
        std::printf("success!\n");
        stdexec::set_value(std::move(r_), 42);
      }
    }
  };

  template <class R>
  friend auto tag_invoke(stdexec::connect_t, fail_some, R r) -> op<R> {
    return {std::move(r)};
  }
};

auto main() -> int {
  auto x = test::retry(fail_some{});
  // prints:
  //   fail!
  //   fail!
  //   success!
  auto [a] = stdexec::sync_wait(std::move(x)).value();
  (void) a;
}

#else
error "Not implemented."
#endif