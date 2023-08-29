cmake_minimum_required(VERSION 3.26)
project(pbt)

set(CMAKE_CXX_STANDARD 23)

add_executable(pbt main.cpp
        pbt.h
        chunk.h
        gen_result.h
        generator.h
        rand_source.h
        random_run.h
        shrink.h
        shrink_cmd.h
        test_exception.h
        test_result.h
        )
