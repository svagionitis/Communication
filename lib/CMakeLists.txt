set(LIBRARY_SOURCES
    Platform/SocketUtils.cpp
    TcpClient.cpp
    TcpServer.cpp
    UdpSocket.cpp
    SerialPort.cpp
)

set(LIBRARY_HEADERS
    ICommunication.h
    Types.h
    ThreadSafeQueue.h
    LockFreeRingBuffer.h
    TcpClient.h
    TcpServer.h
    UdpSocket.h
    SerialPort.h
    Platform/SocketUtils.h
)

add_library(Communication ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

target_include_directories(Communication
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Communication
    PUBLIC
        glog::glog
)

if(WIN32)
    target_link_libraries(Communication PRIVATE ws2_32)
endif()

apply_compiler_flags(Communication)
