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


#include <cstdlib>
#include <numeric>
#include <iostream>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#if 1
auto main()->int{};
#else

long serial_fib(long n) {
  return n < 2 ? n : serial_fib(n - 1) + serial_fib(n - 2);
}


template <typename Scheduler>
struct fib_s {
  using sender_concept = stdexec::sender_t;
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(long)>;

  long cutoff;
  long n;
  Scheduler sched;

  template <class Receiver>
  struct operation {
    Receiver rcvr_;
    long cutoff;
    long n;
    Scheduler sched;

    void start() noexcept {
      if (this->n < this->cutoff) {
        stdexec::set_value(static_cast<Receiver &&>(this->rcvr_),
                          serial_fib(this->n));
      } else {
        auto mkchild = [&](long n) {
          return stdexec::starts_on(
            this->sched, fib_sender(fib_s{this->cutoff, n, this->sched}));
        };

        stdexec::start_detached(
          stdexec::when_all(mkchild(this->n - 1), mkchild(this->n - 2)) |
          stdexec::then([rcvr = static_cast<Receiver &&>(this->rcvr_)](
                         long a, long b) mutable {
            stdexec::set_value(static_cast<Receiver &&>(rcvr), a + b);
          }));
      }
    }
  };

  template <stdexec::receiver_of<completion_signatures> Receiver>
  operation<Receiver> connect(Receiver rcvr) {
    return {static_cast<Receiver &&>(rcvr), this->cutoff, this->n, this->sched};
  }
};

template <class Scheduler>
fib_s(long cutoff, long n, Scheduler sched) -> fib_s<Scheduler>;

template <typename duration, typename F>
auto measure(F &&f) {
  std::chrono::steady_clock::time_point start =
    std::chrono::steady_clock::now();
  f();
  return std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() -
                                              start)
    .count();
}

template <class... Ts>
using any_sender_of = typename exec::any_receiver_ref<
  stdexx::completion_signatures<Ts...>>::template any_sender<>;

using fib_sender = any_sender_of<stdexx::set_value_t(long)>;

int main(int argc, char **argv) {
  if (argc < 5) {
    std::cerr
      << "Usage: fibonacci cutoff n nruns"
      << std::endl;
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

  exec::static_thread_pool pool;


  std::vector<unsigned long> times;
  long result;
  for (unsigned long i = 0; i < nruns; ++i) {
    auto snd = fib_sender(fib_s{cutoff, n, pool.get_scheduler()});
    auto time = measure<std::chrono::milliseconds>(
      [&] { std::tie(result) = stdexec::sync_wait(std::move(snd)).value(); });
    times.push_back(static_cast<unsigned int>(time));
  }

  std::cout << "Avg time: "
            << (std::accumulate(times.begin() + warmup, times.end(), 0u) /
                (times.size() - warmup))
            << "ms. Result: " << result << std::endl;
}
#endif