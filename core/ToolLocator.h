#ifndef TOOLLOCATOR_H
#define TOOLLOCATOR_H

#include <QString>

//工具定位器：检测系统PATH中的外部工具(ffmpeg/adb)，回退到软件自带工具
class ToolLocator
{
public:
    //查找工具路径
    //toolName: 工具名(如"ffmpeg"、"adb")
    //bundledSubpath: 自带工具的相对路径(如"FFmpeg_tools/bin/ffmpeg.exe")
    //logTag: 日志标签(如"FFmpeg"、"ADB")
    //返回工具完整路径，未找到返回空字符串
    static QString locate(const QString &toolName, const QString &bundledSubpath, const QString &logTag);
};

#endif // TOOLLOCATOR_H
