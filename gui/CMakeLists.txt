find_package(Qt6 REQUIRED COMPONENTS Quick Qml)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(GUI_SOURCES
    main_gui.cpp
    CommunicationBridge.cpp
    CommunicationBridge.h
)

qt_add_executable(communication_gui ${GUI_SOURCES})
qt_add_qml_module(communication_gui
    URI CommunicationGui
    VERSION 1.0
    QML_FILES Main.qml
)

target_include_directories(communication_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(communication_gui PRIVATE Communication Qt6::Quick Qt6::Qml glog::glog)
apply_compiler_flags(communication_gui)
