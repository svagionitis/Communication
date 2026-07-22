add_executable(protocol_gateway
    DuplexBridge.cpp
    main.cpp
)

target_link_libraries(protocol_gateway PRIVATE Communication glog::glog Threads::Threads)
apply_compiler_flags(protocol_gateway)
