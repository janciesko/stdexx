#define once

#if (STDEXX_QTHREADS)
// ULT backend
#include <qthreads/stdexec.hpp>
#elif(STDEXX_REFERENCE)
// stdexec backend
#include <stdexec/execution.hpp>
namespace stdexx = stdexec;
#else
error "Not implemented."
#endif
