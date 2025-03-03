find_library(qthreads_lib_found qthread PATHS ${Qthreads_ROOT} SUFFIXES lib lib64 NO_DEFAULT_PATHS)
find_path(qthreads_headers_found qthread.h PATHS ${Qthreads_ROOT}/include NO_DEFAULT_PATHS)

find_package_handle_standard_args(Qthreads REQUIRED_VARS qthreads_lib_found qthreads_headers_found)

if (qthreads_lib_found AND qthreads_headers_found)
  add_library(Qthreads INTERFACE)
  set_target_properties(Qthreads PROPERTIES
    INTERFACE_LINK_LIBRARIES ${qthreads_lib_found}
    INTERFACE_INCLUDE_DIRECTORIES ${qthreads_headers_found}
  )
endif()