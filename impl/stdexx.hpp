#ifndef STDEXX_STDEXX_H
#define STDEXX_STDEXX_H

// std::execution with ULT backend (X)

#if defined(STDEXX_ENABLE_QTHREADS)
#include <qthreads/stdexec.hpp>
#else
#error "Not implemented."
#endif

#endif
