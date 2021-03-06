cmake_minimum_required(VERSION 2.8.11)

project(xbash)

add_executable(xbash
               xbash.cpp
               xbash.rc)

add_definitions(/D_UNICODE /DUNICODE /DNOMINMAX)

if(MSVC)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_target_properties(xbash PROPERTIES COMPILE_FLAGS "/Wall -Wextra")
    else()
        # Wall is basically unusable as is with MSVC, because it needlessly chokes
        # on crt and system headers.
        set_target_properties(xbash PROPERTIES COMPILE_FLAGS "/W4 /D_CRT_SECURE_NO_WARNINGS")
    endif()

    set(variables
      CMAKE_C_FLAGS_DEBUG
      CMAKE_C_FLAGS_MINSIZEREL
      CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_RELWITHDEBINFO
    )
    foreach(variable ${variables})
        # in general, statically link (target_link_libraries below are an exception for the ucrt):
        if(${variable} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
        endif()
        # remove NDEBUG to keep assertions:
        if(${variable} MATCHES "/D NDEBUG")
            string(REGEX REPLACE "/D NDEBUG" "" ${variable} "${${variable}}")
        endif()
    endforeach()

    target_link_libraries(xbash debug -NODEFAULTLIB:libucrtd.lib debug ucrtd.lib Shlwapi.lib Ws2_32.lib)
    target_link_libraries(xbash optimized -NODEFAULTLIB:libucrt.lib optimized ucrt.lib Shlwapi.lib Ws2_32.lib)
endif()
