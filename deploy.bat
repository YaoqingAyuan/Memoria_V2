@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM  Memoria_N_V2 发行版部署脚本 (deploy.bat)
REM ============================================================
REM  用法:
REM    1. Release 模式编译项目, 得到 Memoria_N_V2.exe
REM    2. 新建一个空文件夹作为发布目录
REM    3. 将 Memoria_N_V2.exe 和本脚本放入该文件夹
REM    4. 双击运行本脚本
REM    5. 完成后压缩该文件夹为 .zip 即可发行
REM ============================================================

REM ====== 可配置项 (路径有变动时修改这里) ======
set "QT_BIN=D:\Qt\6.11.1\mingw_64\bin"
set "MINGW_BIN=D:\Qt\Tools\mingw1310_64\bin"
set "PROJECT_ROOT=D:\Github Clone\Memoria_V2"
set "EXE_NAME=Memoria_N_V2.exe"
REM ==============================================

set "DEPLOY_DIR=%~dp0"
cd /d "%DEPLOY_DIR%"

REM 将 Qt / MinGW bin 加入 PATH (windeployqt 运行依赖)
set "PATH=%QT_BIN%;%MINGW_BIN%;%PATH%"

echo ============================================================
echo   Memoria_N_V2 发行版部署脚本
echo ============================================================
echo   部署目录: %DEPLOY_DIR%
echo   目标程序: %EXE_NAME%
echo.

REM --- 前置检查: exe 是否存在 ---
if not exist "%DEPLOY_DIR%%EXE_NAME%" (
    echo [X] 错误: 当前目录下未找到 %EXE_NAME%
    echo     请将 Release 编译产物与本脚本放在同一目录
    goto :failed
)

REM --- 前置检查: windeployqt 是否可用 ---
where windeployqt >nul 2>&1
if errorlevel 1 (
    echo [X] 错误: 未找到 windeployqt
    echo     请检查 QT_BIN 路径是否正确: %QT_BIN%
    goto :failed
)

REM ===== 步骤 1/3: windeployqt 收集 Qt 依赖 =====
echo [1/3] 运行 windeployqt, 收集 Qt 运行时依赖...
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%EXE_NAME%"
if errorlevel 1 (
    echo [X] 错误: windeployqt 执行失败
    goto :failed
)
echo     [OK] Qt DLL 与平台插件已就位
echo.

REM ===== 步骤 2/3: 拷贝 MinGW 运行时 DLL =====
REM windeployqt 可能已拷贝部分, 这里强制覆盖确保齐全
echo [2/3] 拷贝 MinGW 运行时 DLL...
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_BIN%\%%F" (
        copy /Y "%MINGW_BIN%\%%F" "%DEPLOY_DIR%" >nul 2>&1
        echo     [OK] %%F
    ) else (
        echo     [!] 警告: 未找到 %%F
    )
)
echo.

REM ===== 步骤 3/3: 拷贝捆绑工具 (FFmpeg / ADB) =====
REM selfCheck() 在 exe 同级查找 FFmpeg_tools/bin/ 和 Adb_tools/bin/
echo [3/3] 拷贝捆绑工具...

if exist "%PROJECT_ROOT%\FFmpeg_tools\bin" (
    xcopy /E /I /Q /Y "%PROJECT_ROOT%\FFmpeg_tools\bin" "%DEPLOY_DIR%FFmpeg_tools\bin" >nul 2>&1
    if errorlevel 1 (
        echo     [X] 错误: FFmpeg_tools 拷贝失败
        goto :failed
    )
    echo     [OK] FFmpeg_tools\bin ^(ffmpeg.exe, ffprobe.exe 等^)
) else (
    echo     [X] 错误: 源路径不存在: %PROJECT_ROOT%\FFmpeg_tools\bin
    goto :failed
)

if exist "%PROJECT_ROOT%\Adb_tools\bin" (
    xcopy /E /I /Q /Y "%PROJECT_ROOT%\Adb_tools\bin" "%DEPLOY_DIR%Adb_tools\bin" >nul 2>&1
    if errorlevel 1 (
        echo [X] 错误: Adb_tools 拷贝失败
        goto :failed
    )
    echo     [OK] Adb_tools\bin ^(adb.exe, AdbWinApi.dll 等^)
) else (
    echo     [X] 错误: 源路径不存在: %PROJECT_ROOT%\Adb_tools\bin
    goto :failed
)

echo.
echo ============================================================
echo   部署完成!
echo   %DEPLOY_DIR% 已是可独立运行的完整包
echo   直接压缩此文件夹为 .zip 即可发行
echo ============================================================
echo.
pause
exit /b 0

:failed
echo.
echo 部署失败, 请按上述提示排查后重试
pause
exit /b 1
