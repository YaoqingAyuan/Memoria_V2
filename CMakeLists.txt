cmake_minimum_required(VERSION 3.16)

project(Memoria_N_V2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network LinguistTools)

set(TS_FILES Memoria_N_V2_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Memoria_N_V2
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}

        FFmpeg_Module/FFmpeg_module.h FFmpeg_Module/FFmpeg_module.cpp

        Core/ParsedCacheData.h Core/ParsedCacheData.cpp
        TaskQueue/TaskQueue.h TaskQueue/TaskQueue.cpp
        Core/utils.h
        Parser_Module/CacheFileParser.h Parser_Module/CacheFileParser.cpp
        Core/DataModel.h Core/DataModel.cpp
        Core/CacheManager.h Core/CacheManager.cpp

        Core/logger.h Core/logger.cpp
        Secondary_UI/Output_Setting_Dlog.h Secondary_UI/Output_Setting_Dlog.cpp Secondary_UI/Output_Setting_Dlog.ui
        Secondary_UI/VideoPlayback_Weight.h Secondary_UI/VideoPlayback_Weight.cpp Secondary_UI/VideoPlayback_Weight.ui
        Secondary_UI/Independ_Import_Dialog.h Secondary_UI/Independ_Import_Dialog.cpp Secondary_UI/Independ_Import_Dialog.ui
        Secondary_UI/Link_Input_Weight.h Secondary_UI/Link_Input_Weight.cpp Secondary_UI/Link_Input_Weight.ui
        Secondary_UI/ExterDevice_Input_Weight.h Secondary_UI/ExterDevice_Input_Weight.cpp Secondary_UI/ExterDevice_Input_Weight.ui
        Secondary_UI/Setting_Dialog.h Secondary_UI/Setting_Dialog.cpp Secondary_UI/Setting_Dialog.ui
        WLAN_Module/WlanTypes.h
        WLAN_Module/WlanProtocol.h WLAN_Module/WlanProtocol.cpp
        WLAN_Module/DiscoveryManager.h WLAN_Module/DiscoveryManager.cpp
        WLAN_Module/TransferServer.h WLAN_Module/TransferServer.cpp
        WLAN_Module/TransferClient.h WLAN_Module/TransferClient.cpp
        WLAN_Module/TransferSession.h WLAN_Module/TransferSession.cpp
        WLAN_Module/WlanManager.h WLAN_Module/WlanManager.cpp
        Adb_Module/AdbTypes.h
        Adb_Module/AdbModule.h Adb_Module/AdbModule.cpp


    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Memoria_N_V2 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(Memoria_N_V2 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Memoria_N_V2
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(Memoria_N_V2 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

#将项目根目录及Core子目录加入包含路径，使各子目录中的文件可通过 "Core/xxx.h" 或 "xxx.h" 引用核心头文件
target_include_directories(Memoria_N_V2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Core)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Memoria_N_V2)
endif()
set_target_properties(Memoria_N_V2 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Memoria_N_V2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Memoria_N_V2)
endif()
