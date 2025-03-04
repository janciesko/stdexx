/*
 * Copyright (c) 2023 Intel Corporation
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if 1
int main() {}
#else
#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

long serial_fib(long n) {
  return n < 2 ? n : serial_fib(n - 1) + serial_fib(n - 2);
}

template <class... Ts>
using any_sender_of =
  typename exec::any_receiver_ref<stdexx::completion_signatures<Ts...>>::template any_sender<>;

using fib_sender = any_sender_of<stdexx::set_value_t(long)>;

template <typename Scheduler>
struct fib_s {
  using sender_concept = stdexx::sender_t;
  using completion_signatures = stdexx::completion_signatures<stdexx::set_value_t(long)>;

  long cutoff;
  long n;
  Scheduler sched;

  template <class Receiver>
  struct operation {
    Receiver rcvr_;
    long cutoff;
    long n;
    Scheduler sched;

    friend void tag_invoke(stdexx::start_t, operation& self) noexcept {
      if (self.n < self.cutoff) {
        stdexx::set_value(static_cast<Receiver&&>(self.rcvr_), serial_fib(self.n));
      } else {
        auto mkchild = [&](long n) {
          return stdexx::starts_on(self.sched, fib_sender(fib_s{self.cutoff, n, self.sched}));
        };

        stdexx::start_detached(
          stdexx::when_all(mkchild(self.n - 1), mkchild(self.n - 2))
          | stdexx::then([rcvr = static_cast<Receiver&&>(self.rcvr_)](long a, long b) mutable {
              stdexx::set_value(static_cast<Receiver&&>(rcvr), a + b);
            }));
      }
    }
  };

  template <stdexx::receiver_of<completion_signatures> Receiver>
  friend operation<Receiver> tag_invoke(stdexx::connect_t, fib_s self, Receiver rcvr) {
    return {static_cast<Receiver&&>(rcvr), self.cutoff, self.n, self.sched};
  }
};

template <class Scheduler>
fib_s(long cutoff, long n, Scheduler sched) -> fib_s<Scheduler>;

template <typename duration, typename F>
auto measure(F&& f) {
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  f();
  return std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - start).count();
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cerr << "Usage: example.benchmark.fibonacci cutoff n nruns {tbb|static}" << std::endl;
    return -1;
  }

  // skip 'warmup' iterations for performance measurements
  static constexpr size_t warmup = 1;

  long cutoff = std::strtol(argv[1], nullptr, 10);
  long n = std::strtol(argv[2], nullptr, 10);
  std::size_t nruns = std::strtoul(argv[3], nullptr, 10);

  if (nruns <= warmup) {
    std::cerr << "nruns should be >= " << warmup << std::endl;
    return -1;
  }

  std::variant<execpools::tbb_thread_pool, exec::static_thread_pool> pool;

  if (argv[4] == std::string_view("tbb")) {
    pool.emplace<execpools::tbb_thread_pool>(static_cast<int>(std::thread::hardware_concurrency()));
  } else {
    pool.emplace<exec::static_thread_pool>(
      std::thread::hardware_concurrency(), exec::bwos_params{}, exec::get_numa_policy());
  }

  std::vector<unsigned long> times;
  long result;
  for (unsigned long i = 0; i < nruns; ++i) {
    auto snd = std::visit(
      [&](auto&& pool) { return fib_sender(fib_s{cutoff, n, pool.get_scheduler()}); }, pool);

    auto time = measure<std::chrono::milliseconds>([&] {
      std::tie(result) = stdexx::sync_wait(std::move(snd)).value();
    });
    times.push_back(static_cast<unsigned int>(time));
  }

  std::cout << "Avg time: "
            << (std::accumulate(times.begin() + warmup, times.end(), 0u) / (times.size() - warmup))
            << "ms. Result: " << result << std::endl;
}

#endif