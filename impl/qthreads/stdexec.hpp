#ifndef STDEXX_QTHREADS_STDEXEC_H
#define STDEXX_QTHREADS_STDEXEC_H

#include <stdexec/execution.hpp>
#include <stdio.h>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

namespace stdexx {

int init() { return qthread_initialize(); }

void finalize() { qthread_finalize(); }

// check whether a domain or tag provides an applicable
// transform_sender implementation.
// i.e. tag.transform_sender(std::forward(sender), env...) exists.
template <class DomainOrTag, class Sender, class... Env>
concept has_transform_sender =
  requires(DomainOrTag tag, Sender &&sender, Env const &...env) {
    tag.transform_sender(static_cast<Sender &&>(sender), env...);
  };

// This is just a version of "has_transform_sender"
// that sanitizes its inputs and checks specifically
// whether there's a transform_sender implementation for the default domain.
// - sender_expr validates that _Sender is actually a sender.
// - has_transform_sender checks that the default domain (retreived by
//   tag_of_t?) has an implementation for the given sender and additional
//   arguments.
template <class _Sender, class... _Env>
concept has_default_transform_sender =
  stdexec::sender_expr<_Sender> &&
  has_transform_sender<stdexec::tag_of_t<_Sender>, _Sender, _Env...>;

struct qthreads_domain {
  // transform_sender forwards to a default implementation if there is one.
  // This forwards to the sender's associated tag. I think that may just
  // be the sender type. It's not clear to me why there's a distinction
  // between the sender type and the associated tag. The main idea is
  // that transform_sender gets implemented as a method on the sender type
  // (or the tag type when that's different).
  // Maybe the tag thing is just a way to reduce redundancies in the
  // stdexec codebase.
  template <class _Sender, class... _Env>
  /* sanitize inputs */
    requires has_default_transform_sender<_Sender, _Env...>
  __attribute__((always_inline)) auto transform_sender(_Sender &&__sndr,
                                                       _Env &&...__env) const
    /* this giant noexcept thing is just to check if the transform_sender
     * implementation that will be called and decide whether this should be
     * noexcept based on that. */
    noexcept(stdexec::__detail::__has_nothrow_transform_sender<
             stdexec::tag_of_t<_Sender>,
             _Sender,
             _Env...>)
    /* not sure why decltype(auto) isn't good here. TODO: why? */
    -> stdexec::__detail::__transform_sender_result_t<
      stdexec::tag_of_t<_Sender>,
      _Sender,
      _Env...> {
    // Get the tag from the sender type (often the sender itself).
    // then use the implementation that type provides for transform_sender.
    return stdexec::tag_of_t<_Sender>().transform_sender(
      static_cast<_Sender &&>(__sndr), __env...);
  }

  // transform_sender falls back to this if there's no default.
  // It just calls the move constructor of the _Sender type and
  // returns the result. It's noexcept if that is.
  // So, apparently no transformation happens in this case.
  template <class _Sender, class... _Env>
  // _Env arguments are discarded.
  __attribute__((always_inline)) auto transform_sender(_Sender &&__sndr,
                                                       _Env &&...) const
    noexcept(stdexec::__nothrow_constructible_from<_Sender, _Sender>)
      -> _Sender {
    return static_cast<_Sender>(static_cast<_Sender &&>(__sndr));
  }

  // transform_env forwards to a default implementation if there is one.
  // This forwards to the sender's associated tag. I think that may just
  // be the sender type. It's not clear to me why there's a distinction
  // between the sender type and the associated tag. The main idea is
  // that transform_env gets implemented as a method on the sender type
  // (or the tag type when that's different).
  // Maybe the tag thing is just a way to reduce redundancies in the
  // stdexec codebase.
  template <class _Sender, class _Env>
    requires stdexec::__detail::__has_default_transform_env<_Sender, _Env>
  auto transform_env(_Sender &&__sndr, _Env &&__env) const noexcept
    -> stdexec::__detail::
      __transform_env_result_t<stdexec::tag_of_t<_Sender>, _Sender, _Env> {
    return stdexec::tag_of_t<_Sender>().transform_env(
      static_cast<_Sender &&>(__sndr), static_cast<_Env &&>(__env));
  }

  // transform_env falls back to this if there's no default.
  // Note that the first argument is ignored entirely.
  //   Note: it uses implicit conversions to do this instead of
  //   having a template argument for doing that.
  // It just calls the move constructor of the _Env type and
  // returns the result.
  // Apparently that's always expected to be noexcept?
  template <class _Env>
  auto transform_env(stdexec::__ignore, _Env &&__env) const noexcept -> _Env {
    return static_cast<_Env>(static_cast<_Env &&>(__env));
  }

  // The default domain just forwards apply_sender to the tag type.
  // In this case the tag indicates the operation being applied
  // (e.g. sync_wait). A tag is passed as an argument to apply_sender
  // so that the appropriate operation can be derived that way.
  template <class _Tag, class... _Args>
    requires stdexec::__detail::__has_apply_sender<_Tag, _Args...>
  __attribute__((always_inline)) auto apply_sender(_Tag,
                                                   _Args &&...__args) const
    -> stdexec::__detail::__apply_sender_result_t<_Tag, _Args...> {
    return _Tag().apply_sender(static_cast<_Args &&>(__args)...);
  }
};

struct scheduler {
  constexpr scheduler() = default;

  bool operator==(scheduler const &rhs) const noexcept { return true; }

  bool operator!=(scheduler const &rhs) const noexcept {
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
      std::cout << "Hello from qthreads in initial scheduling task! id = "
                << qthread_id() << std::endl;
      // This call to set_value does the other work from a bunch of the
      // algorithms in stdexec. The simpler ones just recursively do their work
      // here.
      stdexec::set_value(std::move(os->receiver));
      return 0;
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
  // start a chain of tasks on this scheduler.
  struct sender {
    using is_sender = void;

    // a feb to allow waiting on this sender.
    aligned_t feb;

    // The types of completion this sender supports.
    // In this case it can't do set_stopped, so it's not listed here.
    using completion_signatures =
      stdexec::completion_signatures<stdexec::set_value_t(),
                                     stdexec::set_error_t(int)>;

    template <typename Receiver>
    operation_state<Receiver> connect(Receiver &&receiver) {
      return {&feb, std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    static operation_state<Receiver> connect(sender &&s, Receiver &&receiver) {
      std::cout << "calling through single-shot connect." << std::endl;
      return {&s.feb, std::forward<Receiver>(receiver)};
    }

    template <typename Receiver>
    static operation_state<Receiver> connect(sender &s, Receiver &&receiver) {
      std::cout << "calling through multi-shot connect." << std::endl;
      return {&s.feb, std::forward<Receiver>(receiver)};
    }

    struct env {
      scheduler get_completion_scheduler() const noexcept { return {}; }
    };

    env get_env() const noexcept { return {}; }
  };

  // Called by stdexec::schedule to get a sender that can
  // start a chain of tasks on this scheduler.
  sender schedule() const noexcept { return {}; }

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
      if constexpr (stdexec::__completes_on<Sender, scheduler>) {
        return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      } else if constexpr (stdexec::__starts_on<Sender, scheduler, Env>) {
        return stdexec::__sexpr_apply(std::forward<Sender>(sndr),
                                      transform_bulk{});
      }
    }

    // template <typename tag, execution::sender sndr, typename... args>
    // decltype(auto) apply_sender(tag, sndr &&s, args&&... a) {
    //   std::abort();
    // }
  };

  domain get_domain() const noexcept { return {}; }
};
} // namespace stdexx

template <>
auto stdexec::__sync_wait::sync_wait_t::apply_sender<stdexx::scheduler::sender>(
  stdexx::scheduler::sender &&s) const
  -> std::optional<
    stdexec::__sync_wait::__sync_wait_result_t<stdexx::scheduler::sender>> {
  __state __local_state{};
  std::optional<
    stdexec::__sync_wait::__sync_wait_result_t<stdexx::scheduler::sender>>
    result{};

  // Launch the sender with a continuation that will fill in the __result
  // optional or set the exception_ptr in __local_state.
  std::cout << "calling connect" << std::endl;
  [[maybe_unused]]
  auto op = stdexec::connect(
    s, __receiver_t<stdexx::scheduler::sender>{&__local_state, &result});
  std::cout << "starting op" << std::endl;
  stdexec::start(op);

  // Wait for the variant to be filled in.

  std::cout << "successfully specialized sync_wait!" << std::endl;
  aligned_t r;
  qthread_readFF(&r, &s.feb);
  std::cout << "Returned from waiting" << std::endl;
  return result;
}

#endif
