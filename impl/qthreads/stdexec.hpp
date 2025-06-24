#define once

#include <stdio.h>

#include <concepts>
#include <iostream>
#include <type_traits>

#include <stdexec/execution.hpp>

#include <qthread/qloop.h>
#include <qthread/qthread.h>

namespace stdexx {

int init() { return qthread_initialize(); }

void finalize() { qthread_finalize(); }

struct qthreads_domain;
struct qthreads_scheduler;
struct qthreads_env;

struct qthreads_sender_tag : stdexec::sender_t {};

template <typename S>
concept is_qthreads_sender =
  std::derived_from<typename S::sender_concept, qthreads_sender_tag>;

template <typename Der>
struct qthreads_base_sender;
struct qthreads_sender;
template <typename Val>
struct qthreads_just_sender;
template <typename Func>
struct qthreads_basic_func_sender;
template <typename Func, typename Arg>
struct qthreads_func_sender;

template <class Tag, class... Env>
struct transform_sender_for;

template <class Tag>
struct apply_sender_for;

struct qthreads_domain {
  // The example we're following for transform_sender uses
  // stdexec::sender_expr as the concept for Sender here,
  // but that seems to require some internal or undocumented
  // stuff right now, so trying to get that concept to match
  // a qthreads_sender causes the compiler to word-vomit
  // a mountain of unhelpful concept mismatch errors.
  // For now just skip the concept check here.
  template <stdexec::sender_expr Sender, class... Env>
  auto transform_sender(Sender &&sndr, Env const &...env) const {
    using Tag = stdexec::tag_of_t<Sender>;
    // The stream domain example in nvexec uses some internal
    // idioms to unpack the info from inside stdexec::then.
    // For now we're following that same pattern rather than chase
    // down the info manually.
    // Is there even an established method in the spec for getting the
    // invocable back out of a sender adaptor?
    return stdexec::__sexpr_apply(static_cast<Sender &&>(sndr),
                                  transform_sender_for<Tag, Env...>{env...});
  }

  template <typename Sn, typename... Env>
    requires is_qthreads_sender<Sn>
  auto &&transform_sender(Sn &&sndr, Env const &...env) const noexcept;

  template <class Tag, stdexec::sender Sender, class... Args>
    requires stdexec::__callable<apply_sender_for<Tag>, Sender, Args...>
  static auto apply_sender(Tag, Sender &&sndr, Args &&...args) {
    return apply_sender_for<Tag>{}(static_cast<Sender &&>(sndr),
                                   static_cast<Args &&>(args)...);
  }
};

struct qthreads_scheduler {
  constexpr qthreads_scheduler() = default;

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_scheduler const &) noexcept;

  bool operator==(qthreads_scheduler const &rhs) const noexcept { return true; }

  bool operator!=(qthreads_scheduler const &rhs) const noexcept {
    return !(*this == rhs);
  }

  // Called by stdexec::schedule to get a qthreads_sender that can
  // start a chain of tasks on this scheduler.
  qthreads_sender schedule() const noexcept;
};

template <typename Derived_Op_State, typename Receiver>
struct qt_os_base {
  aligned_t feb;
  Receiver receiver;

  template <typename Receiver_>
  qt_os_base(Receiver_ &&r): feb(0u), receiver(std::forward<Receiver_>(r)) {}

  qt_os_base(qt_os_base &&) = delete;
  qt_os_base(qt_os_base const &) = delete;
  qt_os_base &operator=(qt_os_base &&) = delete;
  qt_os_base &operator=(qt_os_base const &) = delete;

  inline void start() noexcept {
    auto st = stdexec::get_stop_token(stdexec::get_env(receiver));
    if (st.stop_requested()) {
      stdexec::set_stopped(std::move(receiver));
      return;
    }
    int r = qthread_fork(&Derived_Op_State::task, this, &feb);

    if (r != QTHREAD_SUCCESS) {
      stdexec::set_error(std::move(this->receiver), r);
    }
  }
};

template <typename Receiver>
struct operation_state : qt_os_base<operation_state<Receiver>, Receiver> {
  static aligned_t task(void *arg) noexcept {
    auto *os = static_cast<operation_state *>(arg);
    // This call to set_value does the other work from a bunch of the
    // algorithms in stdexec. The simpler ones just recursively do their work
    // here.
    stdexec::set_value(std::move(os->receiver));
    return 0u;
  }
};

template <typename Val, typename Receiver>
struct just_operation_state :
  qt_os_base<just_operation_state<Val, Receiver>, Receiver> {
  Val val;

  template <typename Val_, typename Receiver_>
  just_operation_state(Val_ &&v, Receiver_ &&receiver):
    qt_os_base<just_operation_state<Val, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    val(std::forward<Val_>(v)) {}

  static aligned_t task(void *os_void) noexcept {
    just_operation_state *os =
      reinterpret_cast<just_operation_state *>(os_void);
    stdexec::set_value(std::move(os->receiver), std::move(os->val));
    return 0u;
  }
};

template <typename Func, typename Arg, typename Receiver>
struct func_operation_state :
  qt_os_base<func_operation_state<Func, Arg, Receiver>, Receiver> {
  Func func;
  Arg arg;

  template <typename Func_, typename Arg_, typename Receiver_>
  func_operation_state(Func_ &&f, Arg_ &&a, Receiver_ &&receiver):
    qt_os_base<func_operation_state<Func, Arg, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    func(std::forward<Func_>(f)), arg(std::forward<Arg_>(a)) {}

  static aligned_t task(void *os_void) noexcept {
    // TODO: we can probably forward C++ exceptions out of here. Figure out
    // how.
    func_operation_state *os =
      reinterpret_cast<func_operation_state *>(os_void);
    aligned_t ret = (os->func)(os->arg);
    stdexec::set_value(std::move(os->receiver), ret);
    return ret;
  }
};

template <typename Func, typename Receiver>
struct basic_func_operation_state :
  qt_os_base<basic_func_operation_state<Func, Receiver>, Receiver> {
  Func func;

  template <typename Func_, typename Receiver_>
  basic_func_operation_state(Func_ &&f, Receiver_ &&receiver):
    qt_os_base<basic_func_operation_state<Func, Receiver>, Receiver>(
      std::forward<Receiver_>(receiver)),
    func(std::forward<Func_>(f)) {}

  static aligned_t task(void *os_void) noexcept {
    // TODO: we can probably forward C++ exceptions out of here. Figure out
    // how.
    basic_func_operation_state *os =
      reinterpret_cast<basic_func_operation_state *>(os_void);
    aligned_t ret = (os->func)();
    stdexec::set_value(std::move(os->receiver), ret);
    return ret;
  }
};

struct qthreads_env {
  qthreads_scheduler get_completion_scheduler() const noexcept { return {}; }

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_env const &) noexcept;
};

template <typename derived_qthreads_sender>
struct qthreads_base_sender {
  using sender_concept = qthreads_sender_tag;

  qthreads_env get_env() const noexcept { return {}; }

  friend qthreads_domain tag_invoke(stdexec::get_domain_t const,
                                    qthreads_sender const &) noexcept;
};

// sender type returned by stdexec::schedule in order to
// start a chain of tasks on this qthreads_scheduler.
struct qthreads_sender : qthreads_base_sender<qthreads_sender> {
  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  operation_state<Receiver> connect(Receiver &&receiver) && {
    return {std::forward<Receiver>(receiver)};
  }
};

template <typename Val>
struct qthreads_just_sender : qthreads_base_sender<qthreads_just_sender<Val>> {
  Val val;

  qthreads_just_sender(Val &&v) noexcept: val(v) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(Val &&),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static just_operation_state<Val, Receiver> connect(qthreads_just_sender &&s,
                                                     Receiver &&receiver) {
    return {std::move(s.val), std::forward<Receiver>(receiver)};
  }
};

template <typename Func>
struct qthreads_basic_func_sender :
  qthreads_base_sender<qthreads_basic_func_sender<Func>> {
  Func func;

  qthreads_basic_func_sender(Func &&f) noexcept: func(f) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static basic_func_operation_state<Func, Receiver>
  connect(qthreads_basic_func_sender &&s, Receiver &&receiver) {
    return {std::move(s.func), std::forward<Receiver>(receiver)};
  }
};

template <typename Func, typename Arg>
struct qthreads_func_sender :
  qthreads_base_sender<qthreads_func_sender<Func, Arg>> {
  Func func;
  Arg arg;

  qthreads_func_sender(Func f, Arg a) noexcept: func(f), arg(a) {}

  using completion_signatures =
    stdexec::completion_signatures<stdexec::set_value_t(aligned_t),
                                   stdexec::set_stopped_t(),
                                   stdexec::set_error_t(int)>;

  template <typename Receiver>
  static func_operation_state<Func, Arg, Receiver>
  connect(qthreads_func_sender &&s, Receiver &&receiver) {
    return {
      std::move(s.func), std::move(s.arg), std::forward<Receiver>(receiver)};
  }
};

qthreads_sender qthreads_scheduler::schedule() const noexcept { return {}; }

template <bool returns_void>
struct set_value_impl;

template <>
struct set_value_impl<true> {
  template <typename Rec, typename Func, typename... Args>
  static decltype(auto) impl(Rec &&rec, Func &&func, Args &&...args) {
    std::invoke(std::move(func), static_cast<Args &&>(args)...);
    stdexec::set_value(std::move(rec));
  }
};

template <>
struct set_value_impl<false> {
  template <typename Rec, typename Func, typename... Args>
  static decltype(auto) impl(Rec &&rec, Func &&func, Args &&...args) {
    return stdexec::set_value(
      std::move(rec),
      std::invoke(static_cast<Func &&>(func), static_cast<Args &&>(args)...));
  }
};

template <bool returns_void>
struct then_completions;

template <>
struct then_completions<true> {
  template <typename ret_t, typename... Args>
  using completions =
    stdexec::completion_signatures<stdexec::set_value_t(),
                                   stdexec::set_error_t(std::exception_ptr)>;
};

template <>
struct then_completions<false> {
  template <typename ret_t, typename... Args>
  using completions =
    stdexec::completion_signatures<stdexec::set_value_t(ret_t),
                                   stdexec::set_error_t(std::exception_ptr)>;
};

// Sender and receiver types for our customization of stdexec::then.
// Adapted from the then implementation in their examples directory.
template <class R, class F>
class qthreads_then_receiver :
  public stdexec::receiver_adaptor<qthreads_then_receiver<R, F>, R> {
  template <class... As>
  using ret_t =
    std::invoke_result_t<decltype(std::move(std::declval<F>())), As...>;
  template <typename... As>
  using _completions = then_completions<std::is_same_v<ret_t<As...>, void>>::
    template completions<ret_t<As...>, As...>;
public:
  qthreads_then_receiver(R r, F f_):
    stdexec::receiver_adaptor<qthreads_then_receiver, R>{std::move(r)},
    f(std::move(f_)) {}

  // Customize set_value by invoking the callable and passing the result to the
  // inner receiver
  template <class... As>
    requires stdexec::receiver_of<R, _completions<As...>>
  void set_value(As &&...as) && noexcept {
    try {
      set_value_impl<std::is_same_v<ret_t<As...>, void>>::impl(
        std::move(*this).base(), std::move(f), static_cast<As &&>(as)...);
    } catch (...) {
      stdexec::set_error(std::move(*this).base(), std::current_exception());
    }
  }
private:
  F f;
};

template <stdexec::sender S, typename F>
struct qthreads_then_sender : qthreads_base_sender<qthreads_then_sender<S, F>> {
  S s;
  F f;

  template <typename... Args>
  using ret_t = std::invoke_result_t<F, Args...>;

  // The idiom used in the existing stdexec then example algorithm
  // doesn't handle the void case in how it calls in to
  // transform_completion_signatures_of. This is a workaround for that.
  // TODO: is there a more graceful way to do this?
  template <bool is_void, typename... Args>
  struct set_value_signatures;

  template <typename... Args>
  struct set_value_signatures<true, Args...> {
    using type = stdexec::completion_signatures<stdexec::set_value_t()>;
  };

  template <typename... Args>
  struct set_value_signatures<false, Args...> {
    using type =
      stdexec::completion_signatures<stdexec::set_value_t(ret_t<Args...>)>;
  };

  template <typename... Args>
  using set_value_t =
    set_value_signatures<std::is_same_v<ret_t<Args...>, void>, Args...>::type;

  template <class Env>
  using completions_t = stdexec::transform_completion_signatures_of<
    S,
    Env,
    stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>,
    set_value_t>;

  template <class Env>
  auto get_completion_signatures(Env &&) && -> completions_t<Env> {
    return {};
  }

  // Connect:
  template <stdexec::receiver R>
    requires stdexec::sender_to<S, qthreads_then_receiver<R, F>>
  auto connect(R r) && {
    // No additional data needed in the operation state, so just
    // connect the wrapped sender to the qthreads_then_receiver which
    // actually wraps the provided function.
    return stdexec::connect(
      std::move(s),
      qthreads_then_receiver<R, F>{static_cast<R &&>(r), static_cast<F &&>(f)});
  }
};

template <stdexec::sender S, class F>
auto qthreads_then(S s, F f) -> stdexec::sender auto {
  return qthreads_then_sender<S, F>{
    {}, static_cast<S &&>(s), static_cast<F &&>(f)};
}

template <>
struct transform_sender_for<stdexec::then_t> {
  template <class Fn, class /*qthreads sender concept needed here?*/ Sender>
  auto operator()(stdexec::__ignore, Fn fun, Sender &&sndr) const {
    // fun is already the invocable we want to wrap.
    // It's already been extracted from inside the default "then".
    return qthreads_then(sndr, fun);
  }
};

template <>
struct apply_sender_for<stdexec::sync_wait_t> {
  template <typename S>
  auto operator()(S &&sn);

  template <typename Sn>
    requires is_qthreads_sender<Sn>
  auto operator()(Sn &&sn) {
    stdexec::__sync_wait::__state __local_state{};
    std::optional<stdexec::__sync_wait::__sync_wait_result_t<Sn>> result{};

    // Launch the sender with a continuation that will fill in the __result
    // optional or set the exception_ptr in __local_state.
    [[maybe_unused]]
    auto op = stdexec::connect(
      std::move(sn),
      stdexec::__sync_wait::__receiver_t<Sn>{&__local_state, &result});
    stdexec::start(op);

    aligned_t r;
    qthread_readFF(&r, &op.feb);
    return result;
  }
};

template <typename Sn, typename... Env>
  requires is_qthreads_sender<Sn>
auto &&qthreads_domain::transform_sender(Sn &&sndr,
                                         Env const &...env) const noexcept {
  return std::move(sndr);
}

// For some reason get_domain can currently only be specialized this way.
// TODO: fix and/or report this upstream.
// In theory adding get_domain as a method or using the query interface
// should work, but neither actually do.
qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_scheduler const &) noexcept {
  return {};
}

qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_env const &) noexcept {
  return {};
}

template <typename Der>
qthreads_domain tag_invoke(stdexec::get_domain_t const,
                           qthreads_base_sender<Der> const &) noexcept {
  return {};
}

} // namespace stdexx

