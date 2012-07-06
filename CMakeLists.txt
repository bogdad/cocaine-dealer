CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(Dealer)

SET(DEALER_VERSION 0.8)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#SET(CMAKE_INSTALL_PREFIX /usr)

MESSAGE("-- prefix='${CMAKE_INSTALL_PREFIX}'")

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/config.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/cocaine/dealer/config.hpp")

FIND_PACKAGE(Boost 1.34.1 REQUIRED
    COMPONENTS
        program_options
        thread)

FIND_PACKAGE(OpenSSL REQUIRED)
FIND_PACKAGE(CURL REQUIRED)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBZMQ "zmq.hpp" "zmq")

IF(NOT APPLE)
    LOCATE_LIBRARY(LIBUUID "uuid/uuid.h" "uuid")
    SET(LIBUUID_LIBRARY "uuid")
ENDIF()

IF(Boost_MINOR_VERSION GREATER 34)
    SET(Boost_SYSTEM_LIBRARY "boost_system-mt")
ENDIF()

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBZMQ_INCLUDE_DIRS})

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBZMQ_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/foreign/jsoncpp-0.6.0-rc2/include)
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)

ADD_LIBRARY(json
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_value
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_reader
    foreign/jsoncpp-0.6.0-rc2/src/lib_json/json_writer)

SET_TARGET_PROPERTIES(json PROPERTIES
    COMPILE_FLAGS -fPIC)


FILE(GLOB DEALER_SOURCES
    "src/*.cpp")

ADD_LIBRARY(cocaine-dealer SHARED
    ${DEALER_SOURCES})

TARGET_LINK_LIBRARIES(cocaine-dealer
    boost_thread-mt
    crypto
    curl
    json
    msgpack
    zmq
    eblob_cpp
    eblob
    ${Boost_SYSTEM_LIBRARY}
    ${LIBUUID_LIBRARY})

SET_TARGET_PROPERTIES(cocaine-dealer PROPERTIES
    VERSION 1)

ADD_EXECUTABLE(dealer_test_app
    tests/dealer_test_app.cpp)

TARGET_LINK_LIBRARIES(dealer_test_app
    boost_program_options-mt
    cocaine-dealer
    zmq)

ADD_EXECUTABLE(overseer
    utils/overseer.cpp
    src/file_hosts_fetcher.cpp
    src/cocaine_node_info.cpp
    src/cocaine_node_app_info.cpp
    src/cocaine_node_task_info.cpp)

TARGET_LINK_LIBRARIES(overseer
    boost_program_options-mt
    json
    zmq)

ADD_SUBDIRECTORY(bindings)

INSTALL(
    TARGETS
        cocaine-dealer
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

FILE(GLOB DEALER_DEV_HEADERS
    "include/cocaine/dealer/*.hpp")

INSTALL(
    FILES
        ${DEALER_DEV_HEADERS}
    DESTINATION include/cocaine/dealer
    COMPONENT development)

INSTALL(
    DIRECTORY
        include/cocaine/dealer/utils
    DESTINATION include/cocaine/dealer
    COMPONENT development)

INSTALL(
    DIRECTORY
        foreign/jsoncpp-0.6.0-rc2/include/
    DESTINATION include
    COMPONENT development)

INSTALL(
    DIRECTORY
        bindings/perl/blib
    DESTINATION /tmp/perl/modules/cocaine_dealer
    COMPONENT development)
