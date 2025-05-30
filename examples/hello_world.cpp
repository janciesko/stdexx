#include <cstdlib>
#include <iostream>

#include <stdexx.hpp>

#if (STDEXX_QTHREADS)

auto main() -> int {
  stdexx::init();
  stdexec::sender auto my_sender = stdexec::schedule(stdexx::qthreads_scheduler{});

  /*----------*/
  //ISSUE: Sync hangs
  //stdexec::sender auto new_task = stdexec::then(my_sender, [](int arg) {return 1;}); 
  /*----------*/

  auto val =
    stdexec::sync_wait(new_task).value();
  std::cout << std::get<0>(val) << std::endl;
  stdexx::finalize();
  return 0;
}

#elif(STDEXX_REFERENCE)

#include "exec/static_thread_pool.hpp"

int main()
{  
  using namespace stdexx;
  exec::static_thread_pool ctx{8};
  scheduler auto sch = ctx.get_scheduler(); 
                                            
  sender auto begin = schedule(sch);        
  sender auto hi_again = then(                                          
    begin,                                                               
    [] {                                                              
      std::cout << "Hello world! Have an int.\n";                   
      return 13;                                                
    });                                                          

  sender auto add_42 = then(hi_again, [](int arg) { return arg + 42; }); 
  auto [i] = sync_wait(std::move(add_42)).value();                      
  std::cout << "Result: " << i << std::endl;

  // Sync_wait provides a run_loop scheduler
  std::tuple<run_loop::__scheduler> t = sync_wait(get_scheduler()).value();
  (void) t;

  auto y = let_value(get_scheduler(), [](auto sched) {
    return starts_on(sched, then(just(), [] {
                       std::cout << "from run_loop\n";
                       return 42;
                     }));
  });
  sync_wait(std::move(y));
  sync_wait(when_all(just(42), get_scheduler(), get_stop_token()));
}
#else
error "Not implemented."
#endif
