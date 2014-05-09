# vim:set ts=2 sw=2 sts=2 et:

add_definitions(-DHAVE_CONFIG_H) 
 
# Locate GTest
find_package(GTest REQUIRED)
include_directories(${LIBRABBITMQ_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})

add_executable(connect_test connect_test.cpp)

# Standard linking to gtest stuff.
target_link_libraries(connect_test ${RMQ_LIBRARY_TARGET} gtest gtest_main pthread)
add_test(connect_test connect_test)