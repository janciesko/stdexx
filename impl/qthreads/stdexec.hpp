#ifndef STDEXX_QTHREADS_STDEXEC_H
#define STDEXX_QTHREADS_STDEXEC_H

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
    // In this case it can't do set_stopped, so it's not listed here.
    using completion_signatures =
      stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
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

  /*
  template <typename Sender, typename Shape, typename F>
  struct qthreads_bulk_sender {
    [[no_unique_address]] std::decay_t<Sender> sender;
    [[no_unique_address]] std::decay_t<Shape> shape;
    [[no_unique_address]] std::decay_t<F> f;

    template <typename Sender_, typename Shape_, typename F_>
    qthreads_bulk_sender(Sender_ &&sender, Shape_ &&shape, F_ &&f):
      sender(std::forward<Sender_>(sender)), shape(std::forward<Shape_>(shape)),
      f(std::forward<F_>(f)) {}

    qthreads_bulk_sender(qthreads_bulk_sender &) = default;
    qthreads_bulk_sender(qthreads_bulk_sender const &) = default;
    qthreads_bulk_sender &operator=(qthreads_bulk_sender &) = default;
    qthreads_bulk_sender &operator=(qthreads_bulk_sender const &) = default;

    using is_sender = void;

    using completion_signatures = stdexec::make_completion_signatures<
      Sender,
      stdexec::env<>,
      stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>>;

    template <typename Receiver>
    struct operation_state {
      struct bulk_receiver {
        using is_receiver = void;

        operation_state *op_state;

        template <typename E>
        void set_error(E &&e) noexcept {
          stdexec::set_error(std::move(op_state->receiver), std::forward<E>(e));
        }

        void set_stopped() noexcept {
          stdexec::set_stopped(std::move(op_state->receiver));
        }

        static void
        task(std::size_t startat, std::size_t stopat, void *arg) noexcept {
          auto &f = static_cast<operation_state *>(arg)->f;
          // TODO: Handle exceptions, if f is noexcept(false).
          for (std::size_t i = startat; i < stopat; ++i) {
            // TODO: Pass values sent by predecessor as references.
            f(i);
          }
        }

        template <typename... Ts>
        void set_value(Ts &&...ts) noexcept {
          // TODO: Don't spawn tasks if there is no work to be done? Maybe
          // qt_loop_balance already does that?
          // TODO: Are there other qt_loop_* functions that are better?
          // TODO: Is there a non-blocking version of this where one can attach
          // a continuation? One can also emulate this in task.
          std::cout << "launching qt_loop_balance" << std::endl;
          qt_loop_balance(static_cast<std::size_t>(0),
                          static_cast<std::size_t>(op_state->shape),
                          &task,
                          op_state);
          std::cout << "inside set_value" << std::endl;
          stdexec::set_value(std::move(op_state->receiver),
                             std::forward<Ts>(ts)...);
        }

        constexpr stdexec::env<> get_env() const noexcept { return {}; }
      };

      using operation_state_type =
        stdexec::connect_result_t<Sender, bulk_receiver>;

      operation_state_type op_state;
      [[no_unique_address]] std::decay_t<Shape> shape;
      [[no_unique_address]] std::decay_t<F> f;
      [[no_unique_address]] std::decay_t<Receiver> receiver;

      // TODO: Store values sent by predecessor.

      template <typename Sender_,
                typename Shape_,
                typename F_,
                typename Receiver_>
      operation_state(Sender_ &&sender,
                      Shape_ &&shape,
                      F_ &&f,
                      Receiver_ &&receiver):
        op_state(
          stdexec::connect(std::forward<Sender_>(sender), bulk_receiver{this})),
        shape(std::forward<Shape_>(shape)), f(std::forward<F_>(f)),
        receiver(std::forward<Receiver_>(receiver)) {}

      void start() noexcept { stdexec::start(op_state); }
    };

    template <typename Receiver>
    auto connect(Receiver &&receiver) {
      return operation_state<std::decay_t<Receiver>>{
        std::move(sender),
        std::move(shape),
        std::move(f),
        std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
      return operation_state<std::decay_t<Receiver>>{
        sender, shape, f, std::forward<Receiver>(receiver)};
    }

    constexpr auto get_env() const noexcept { return stdexec::get_env(sender); }
  };

  // TODO: This uses the new eager and lazy transform_sender customization
  // mechanism from P2300. It's mostly copy-pasted from stdexec and uses stdexec
  // internals. Remove the internals.
  struct transform_bulk {
    template <class Data, class Sender>
    auto operator()(stdexec::bulk_t, Data &&data, Sender &&sndr) {
      auto [shape, fun] = std::forward<Data>(data);
      return qthreads_bulk_sender<Sender, decltype(shape), decltype(fun)>{
        std::forward<Sender>(sndr), shape, std::move(fun)};
    }
  };

  struct domain {
    template <stdexec::sender_expr_for<stdexec::bulk_t> Sender>
    auto transform_sender(Sender &&sndr) const noexcept {
      return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                    transform_bulk{});
    }

    template <stdexec::sender_expr_for<stdexec::bulk_t> Sender, class Env>
    auto transform_sender(Sender &&sndr, Env const &env) const noexcept {
      if constexpr (stdexec::__completes_on<Sender, qthreads_scheduler>) {
        return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      } else if constexpr (stdexec::__starts_on<Sender, qthreads_scheduler,
  Env>) { return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      }
    }

    // template <typename tag, execution::sender sndr, typename... args>
    // decltype(auto) apply_sender(tag, sndr &&s, args&&... a) {
    //   std::abort();
    // }
  };
  */
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

#endif
