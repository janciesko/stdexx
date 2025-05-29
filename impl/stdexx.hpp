#define once

#if (STDEXX_QTHREADS)
// ULT backend
#include <qthreads/stdexec.hpp>
#include <qthreads/algorithms.hpp>
#elif(STDEXX_REFERENCE)
// stdexec backend
#include <stdexec/execution.hpp>
#include <reference/algorithms.hpp>
namespace stdexx = stdexec;
#else
error "Not implemented."
#endif
