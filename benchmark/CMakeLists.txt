add_executable(lockfree_benchmark
    BenchmarkRunner.cpp
    main.cpp
)

target_link_libraries(lockfree_benchmark PRIVATE Communication glog::glog Threads::Threads)
apply_compiler_flags(lockfree_benchmark)
