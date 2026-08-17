cmake_minimum_required(VERSION 3.16)

project(Memoria_N_V2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES Memoria_N_V2_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

# Windows 应用图标资源
if(WIN32)
    # 将 .ico 复制到构建目录，.rc 中用相对路径 "memoria.ico"，
    # 避免 windres 预处理器因项目路径含空格（d:\Github Clone\...）而失败
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/memoria.ico
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/app.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        @ONLY
    )
    get_filename_component(_mingw_bin ${CMAKE_CXX_COMPILER} DIRECTORY)
    find_program(WINDRES_EXECUTABLE windres HINTS ${_mingw_bin})
    set(MEMORIA_RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/app_rc.obj)
    add_custom_command(
        OUTPUT ${MEMORIA_RC_OBJ}
        COMMAND ${WINDRES_EXECUTABLE} app.rc app_rc.obj -O coff
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app.rc
                ${CMAKE_CURRENT_BINARY_DIR}/memoria.ico
        COMMENT "Compiling Windows icon resource"
    )
    set(APP_ICON_RESOURCE ${MEMORIA_RC_OBJ})
else()
    set(APP_ICON_RESOURCE "")
endif()

# Qt 资源文件（窗口图标等）
set(APP_QRC_FILES app.qrc)

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

        Secondary_UI/ExterDevice_Input_Weight.h Secondary_UI/ExterDevice_Input_Weight.cpp Secondary_UI/ExterDevice_Input_Weight.ui
        Secondary_UI/Setting_Dialog.h Secondary_UI/Setting_Dialog.cpp Secondary_UI/Setting_Dialog.ui
        Adb_Module/AdbTypes.h
        Adb_Module/AdbModule.h Adb_Module/AdbModule.cpp

        ${APP_QRC_FILES}
        ${APP_ICON_RESOURCE}
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
            ${APP_QRC_FILES}
            ${APP_ICON_RESOURCE}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Memoria_N_V2
            ${PROJECT_SOURCES}
            ${APP_QRC_FILES}
            ${APP_ICON_RESOURCE}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(Memoria_N_V2 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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
