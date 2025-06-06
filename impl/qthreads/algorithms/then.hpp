

#pragma once

namespace test {

using namespace stdexec::tags;

// then algorithm:
template <class R, class F>
class _then_receiver :
  public stdexec::receiver_adaptor<_then_receiver<R, F>, R> {
  template <class... As>
  using _completions = //
    stdexec::completion_signatures<stdexec::set_value_t(
                                     std::invoke_result_t<F, As...>),
                                   stdexec::set_error_t(std::exception_ptr)>;
public:
  _then_receiver(R r, F f):
    stdexec::receiver_adaptor<_then_receiver, R>{std::move(r)},
    f_(std::move(f)) {}

  // Customize set_value by invoking the callable and passing the result to the
  // inner receiver
  template <class... As>
    requires stdexec::receiver_of<R, _completions<As...>>
  void set_value(As &&...as) && noexcept {
    try {
      stdexec::set_value(
        std::move(*this).base(),
        std::invoke(static_cast<F &&>(f_), static_cast<As &&>(as)...));
    } catch (...) {
      stdexec::set_error(std::move(*this).base(), std::current_exception());
    }
  }
private:
  F f_;
};

template <stdexec::sender S, class F>
struct _then_sender {
  using sender_concept = stdexec::sender_t;

  S s_;
  F f_;

  // Compute the completion signatures
  template <class... Args>
  using _set_value_t = stdexec::completion_signatures<stdexec::set_value_t(
    std::invoke_result_t<F, Args...>)>;

  template <class Env>
  using _completions_t = //
    stdexec::transform_completion_signatures_of<
      S,
      Env,
      stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>,
      _set_value_t>;

  template <class Env>
  auto get_completion_signatures(Env &&) && -> _completions_t<Env> {
    return {};
  }

  // Connect:
  template <stdexec::receiver R>
    requires stdexec::sender_to<S, _then_receiver<R, F>>
  auto connect(R r) && {
    return stdexec::connect(
      static_cast<S &&>(s_),
      _then_receiver<R, F>{static_cast<R &&>(r), static_cast<F &&>(f_)});
  }

  auto get_env() const noexcept -> decltype(auto) {
    return stdexec::get_env(s_);
  }
};

template <stdexec::sender S, class F>
auto then(S s, F f) -> stdexec::sender auto {
  return _then_sender<S, F>{static_cast<S &&>(s), static_cast<F &&>(f)};
}

} // namespace test
