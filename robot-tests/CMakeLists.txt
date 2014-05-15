# vim:set ts=2 sw=2 sts=2 et:

add_definitions(-DHAVE_CONFIG_H) 
 
# Locate GTest
find_package(GTest REQUIRED)
include_directories(${LIBRABBITMQ_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})

add_executable(connect_disconnect_test connect_disconnect_test.cpp)
target_link_libraries(connect_disconnect_test ${RMQ_LIBRARY_TARGET} gtest gtest_main pthread)
add_test(connect_disconnect_test connect_disconnect_test)

add_executable(amqp_channel_test amqp_channel_test.cpp)
target_link_libraries(amqp_channel_test ${RMQ_LIBRARY_TARGET} gtest gtest_main pthread)
add_test(amqp_channel_test amqp_channel_test)

add_executable(basic_publish_consume_test basic_publish_consume_test.cpp)
target_link_libraries(basic_publish_consume_test ${RMQ_LIBRARY_TARGET} gtest gtest_main pthread)
add_test(basic_publish_consume_test basic_publish_consume_test)