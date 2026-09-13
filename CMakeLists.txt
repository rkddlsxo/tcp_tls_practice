cmake_minimum_required(VERSION 3.16)

project(SocketPractice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server server.cpp)
add_executable(client client.cpp)

find_package(OpenSSL REQUIRED)

add_executable(tls_server tls_server.cpp)
target_link_libraries(tls_server PRIVATE OpenSSL::SSL OpenSSL::Crypto)

add_executable(tls_client tls_client.cpp)
target_link_libraries(tls_client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
