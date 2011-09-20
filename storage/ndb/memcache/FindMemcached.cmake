
SET(MEMCACHED_HOME "" CACHE PATH "Path to installed Memcached 1.6")

# unset(MEMCACHED_FOUND CACHE)
# unset(MEMCACHED_ROOT_DIR CACHE)
# unset(MEMCACHED_INCLUDE_DIR CACHE)
# unset(MEMCACHED_UTILITIES_LIBRARY CACHE)

if(WITH_MEMCACHE) 
  # Use bundled memcached
  set(MEMCACHED_ROOT_DIR ${CMAKE_SOURCE_DIR}/extra/memcached)
  set(MEMCACHED_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extra/memcached/include)
  set(MEMCACHED_UTILITIES_LIBRARY memcached_utilities)
else()   

  # Find an installed memcached
  find_path(MEMCACHED_ROOT_DIR bin/engine_testapp
    HINTS 
      $ENV{MEMCACHED_HOME}
      ${MEMCACHED_HOME}
      ${CMAKE_INSTALL_PREFIX}
    PATHS
      /usr/local  /usr
      /opt/local  /opt
      ~/Library/Frameworks  /Library/Frameworks
  )  

  find_path(MEMCACHED_INCLUDE_DIR memcached/engine_testapp.h
    HINTS  ${MEMCACHED_ROOT_DIR}
    PATH_SUFFIXES include
  )

  find_library(MEMCACHED_UTILITIES_LIBRARY
    NAMES memcached_utilities
    HINTS  ${MEMCACHED_ROOT_DIR}
    PATH_SUFFIXES lib/memcached lib memcached/lib
  )
endif()

if(MEMCACHED_ROOT_DIR AND MEMCACHED_INCLUDE_DIR AND MEMCACHED_UTILITIES_LIBRARY) 
  set(MEMCACHED_FOUND TRUE)
else()
  set(MEMCACHED_FOUND FALSE)
endif()

mark_as_advanced(MEMCACHED_ROOT_DIR 
                 MEMCACHED_INCLUDE_DIR 
                 MEMCACHED_UTILITIES_LIBRARY)