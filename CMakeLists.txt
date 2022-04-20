cmake_minimum_required(VERSION 3.0.0)
project(libreflect VERSION 0.1.0)

add_compile_options(-Wall -Wextra -Werror)
add_executable(reflect reflect-main.c reflect.c)
target_link_libraries(reflect dw)
