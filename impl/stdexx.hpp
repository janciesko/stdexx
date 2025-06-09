#define once

#if (STDEXX_QTHREADS)
// ULT backend
#include <qthreads/algorithms.hpp>
#include <qthreads/stdexec.hpp>
#include <qthreads/stdexec_v2.hpp>
#elif (STDEXX_REFERENCE)
// stdexec backend
#include <reference/algorithms.hpp>
#include <stdexec/execution.hpp>
namespace stdexx = stdexec;
#else
error "Not implemented."
#endif
