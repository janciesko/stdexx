#define once

#include <iostream>
#include <memory>
#include <stdexec/execution.hpp>
#include <stdio.h>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

namespace stdexx {

namespace ex = stdexec;

int context_init() { return qthread_initialize(); }

void context_finalize() { qthread_finalize(); }

struct qthreads_context {
  struct qthreads {};

  std::shared_ptr<qthreads> ctx = std::make_shared<qthreads>();

  qthreads_context() { context_init(); }

  ~qthreads_context() {
    if (ctx.use_count() == 1) context_finalize();
  }

  qthreads_context(qthreads_context const &other): ctx(other.ctx) {}

  qthreads_context &operator=(qthreads_context const &&other) { return *this; }
};

template <ex::receiver Receiver, ex::sender Work>
struct on_qthreads_receiver {
  qthreads_context ctx_;
  Work work_;
  Receiver receiver_;
  using receiver_concept = ex::receiver_t;

  void set_value(int i) noexcept {
    std::cout << "work_and_done" << std::endl;
    ex::sender auto work = ex::just(i) | work_;
    ex::sender auto and_done = ex::then(work, [](aligned_t *feb) {
      qthread_readFF(NULL, feb);
      std::cout << "FEB:" << feb << std::endl;
    });

    auto op = ex::connect(and_done, std::move(receiver_));
    ex::start(op);
  }

  void set_error(std::exception_ptr e) noexcept {
    ex::set_error(std::move(receiver_), e);
  }

  void set_stopped() noexcept { ex::set_stopped(std::move(receiver_)); }
};

template <ex::sender Previous, ex::sender Work>
struct on_qthreads_sender {
  qthreads_context ctx_;
  Previous previous_;
  Work work_;
  using sender_concept = ex::sender_t;
  using completion_signatures =
    ex::completion_signatures<ex::set_value_t(int t),
                              ex::set_error_t(std::exception_ptr),
                              ex::set_stopped_t()>;

  ex::env<> get_env() const noexcept { return {}; }

  template <ex::receiver Receiver>
  auto connect(Receiver receiver) noexcept {
    return ex::connect(previous_, on_qthreads_receiver{ctx_, work_, receiver});
  }
};

// This is motivated by the stdexec::then algorithm but takes a ctx and two
// senders instead
template <ex::sender Previous, ex::sender Work>
auto then(Previous prev,
          qthreads_context &ctx,
          Work work) -> on_qthreads_sender<Previous, Work> {
  return {ctx, prev, work};
}

} // namespace stdexx

