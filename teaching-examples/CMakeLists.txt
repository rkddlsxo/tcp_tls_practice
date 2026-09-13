cmake_minimum_required(VERSION 3.16)
project(TcpTlsTeachingExamples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

foreach(target tcp_server tcp_client tls_server tls_client)
    add_executable(${target} ${target}.cpp)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

target_link_libraries(tls_server PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(tls_client PRIVATE OpenSSL::SSL OpenSSL::Crypto)
