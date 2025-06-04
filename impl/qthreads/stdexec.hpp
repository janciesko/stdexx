#define once

#include <iostream>
#include <stdexec/execution.hpp>
#include <stdio.h>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

namespace stdexx {

int init() { return qthread_initialize(); }

void finalize() { qthread_finalize(); }

template <class Tag, class... Env>
struct transform_sender_for;

template <class Tag>
struct apply_sender_for;

struct qthreads_domain /*: stdexec::default_domain*/ {
  template <stdexec::sender_expr Sender,
            class Tag = stdexec::tag_of_t<Sender>,
            class... Env>
    requires stdexec::__callable<stdexec::__sexpr_apply_t,
                                 Sender,
                                 transform_sender_for<Tag, Env...>>
  static auto transform_sender(Sender &&sndr, Env const &...env) {
    return stdexec::__sexpr_apply(static_cast<Sender &&>(sndr),
                                  transform_sender_for<Tag, Env...>{env...});
  }

  template <class Tag, stdexec::sender Sender, class... Args>
    requires stdexec::__callable<apply_sender_for<Tag>, Sender, Args...>
  static auto apply_sender(Tag, Sender &&sndr, Args &&...args) {
    return apply_sender_for<Tag>{}(static_cast<Sender &&>(sndr),
                                   static_cast<Args &&>(args)...);
  }
};

struct qthreads_scheduler {
  constexpr qthreads_scheduler() = default;

  qthreads_domain get_domain() const noexcept { return {}; }

  [[nodiscard]] auto query(stdexec::get_domain_t) -> qthreads_domain {
    return {};
  }

  // For some reason get_domain can currently only be specialized this way.
  // TODO: fix and/or report this upstream.
  // In theory adding get_domain as a method or using the query interface
  // should work, but neither actually do.
  friend qthreads_domain tag_invoke(stdexec::get_domain_t,
                                    qthreads_scheduler const &) {
    return {};
  }

  bool operator==(qthreads_scheduler const &rhs) const noexcept { return true; }

  bool operator!=(qthreads_scheduler const &rhs) const noexcept {
    return !(*this == rhs);
  }

  template <typename Receiver>
  struct operation_state {
    aligned_t *feb;
    [[no_unique_address]] std::decay_t<Receiver> receiver;

    template <typename Receiver_>
    operation_state(aligned_t *f, Receiver_ &&receiver):
      feb(f), receiver(std::forward<Receiver_>(receiver)) {}

    operation_state(operation_state &&) = delete;
    operation_state(operation_state const &) = delete;
    operation_state &operator=(operation_state &&) = delete;
    operation_state &operator=(operation_state const &) = delete;

    // This one's not a part of the stdexec standard.
    // This is just the function that gets passed to qthread_fork.
    static aligned_t task(void *arg) noexcept {
      auto *os = static_cast<operation_state *>(arg);
      // TODO: Call into a user-provided function pointer here instead.
      // TODO: How do we pipe the template parameters around to accomodate
      // different signatures (and return types) here?
      std::cout << "Hello from qthreads in initial scheduling task! id = "
                << qthread_id() << std::endl;
      // This call to set_value does the other work from a bunch of the
      // algorithms in stdexec. The simpler ones just recursively do their work
      // here.
      aligned_t ret = 0u;
      stdexec::set_value(std::move(os->receiver), ret);
      return ret;
    }

    inline void start() noexcept {
      auto st = stdexec::get_stop_token(stdexec::get_env(receiver));
      if (st.stop_requested()) {
        stdexec::set_stopped(std::move(receiver));
        return;
      }
      std::cout << "calling qthread_fork" << std::endl;
      int r = qthread_fork(&task, this, feb);
      assert(!r);
      // qthread_readFF(NULL, &ret);

      if (r != QTHREAD_SUCCESS) {
        stdexec::set_error(std::move(this->receiver), r);
      }
    }
  };

  // sender type returned by stdexec::schedule in order to
  // start a chain of tasks on this qthreads_scheduler.
  struct qthreads_sender {
    using is_sender = void;

    // a feb to allow waiting on this sender.
    aligned_t feb;

    // The types of completion this sender supports.
    // Even though qthreads doesn't support cancellation, the
    // corresponding sender and operation state can still
    // cancel forking a qthread if cancellation has been
    // requested by the time the operation state's start routine
    // gets called.
    // The default sync_wait returns an optional, not a variant, so
    // set_value must have a single return type and only one entry here.
    // In this case we use the return value to expose the return value
    // from the underlying qthread.
    using completion_signatures =
      stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
                                     stdexec::set_stopped_t(),
                                     stdexec::set_error_t(int)>;

    template <typename Receiver>
    operation_state<Receiver> connect(Receiver &&receiver) {
      return {&feb, std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    static operation_state<Receiver> connect(qthreads_sender &&s,
                                             Receiver &&receiver) {
      std::cout << "calling through single-shot connect." << std::endl;
      return {&s.feb, std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    static operation_state<Receiver> connect(qthreads_sender &s,
                                             Receiver &&receiver) {
      std::cout << "calling through multi-shot connect." << std::endl;
      return {&s.feb, std::forward<Receiver>(receiver)};
    }

    struct env {
      qthreads_scheduler get_completion_scheduler() const noexcept {
        return {};
      }

      qthreads_domain get_domain() const noexcept { return {}; }

      [[nodiscard]] auto query(stdexec::get_domain_t) -> qthreads_domain {
        return {};
      }

      friend qthreads_domain tag_invoke(stdexec::get_domain_t, env const &) {
        return {};
      }
    };

    env get_env() const noexcept { return {}; }

    qthreads_domain get_domain() const noexcept { return {}; }

    [[nodiscard]] auto query(stdexec::get_domain_t) -> qthreads_domain {
      return {};
    }

    friend qthreads_domain tag_invoke(stdexec::get_domain_t,
                                      qthreads_sender const &) {
      return {};
    }
  };

  // Called by stdexec::schedule to get a qthreads_sender that can
  // start a chain of tasks on this scheduler.
  qthreads_sender schedule() const noexcept { return {}; }
};

template <>
struct apply_sender_for<stdexec::sync_wait_t> {
  template <typename S>
  auto operator()(S &&sn);

  template <>
  auto operator()(qthreads_scheduler::qthreads_sender &&sn) {
    std::cout << "starting specialized apply_sender" << std::endl;
    stdexec::__sync_wait::__state __local_state{};
    std::optional<stdexec::__sync_wait::__sync_wait_result_t<
      qthreads_scheduler::qthreads_sender>>
      result{};

    // Launch the sender with a continuation that will fill in the __result
    // optional or set the exception_ptr in __local_state.
    std::cout << "calling connect" << std::endl;
    [[maybe_unused]]
    auto op = stdexec::connect(
      sn,
      stdexec::__sync_wait::__receiver_t<qthreads_scheduler::qthreads_sender>{
        &__local_state, &result});
    std::cout << "starting op" << std::endl;
    stdexec::start(op);

    // Wait for the variant to be filled in.

    std::cout << "successfully specialized sync_wait!" << std::endl;
    aligned_t r;
    qthread_readFF(&r, &sn.feb);
    std::cout << "Returned from waiting" << std::endl;
    return result;
  }
};

} // namespace stdexx

/*
template <>
auto
stdexec::__sync_wait::sync_wait_t::apply_sender<stdexx::qthreads_scheduler::qthreads_sender>(
  stdexx::qthreads_scheduler::qthreads_sender &&s) const
  -> std::optional<
    stdexec::__sync_wait::__sync_wait_result_t<stdexx::qthreads_scheduler::qthreads_sender>>
{
  __state __local_state{};
  std::optional<
    stdexec::__sync_wait::__sync_wait_result_t<stdexx::qthreads_scheduler::qthreads_sender>>
    result{};

  // Launch the sender with a continuation that will fill in the __result
  // optional or set the exception_ptr in __local_state.
  std::cout << "calling connect" << std::endl;
  [[maybe_unused]]
  auto op = stdexec::connect(
    s, __receiver_t<stdexx::qthreads_scheduler::qthreads_sender>{&__local_state,
&result}); std::cout << "starting op" << std::endl; stdexec::start(op);

  // Wait for the variant to be filled in.

  std::cout << "successfully specialized sync_wait!" << std::endl;
  aligned_t r;
  qthread_readFF(&r, &s.feb);
  std::cout << "Returned from waiting" << std::endl;
  return result;
}
*/

