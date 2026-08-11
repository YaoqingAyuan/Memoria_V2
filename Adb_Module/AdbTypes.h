#ifndef ADBTYPES_H
#define ADBTYPES_H
//ADB模块数据结构：设备信息与目录条目

#include <QString>

//设备信息：adb devices -l 输出的一行解析结果
struct AdbDeviceInfo {
    QString serial;     //设备标识，如 "192.168.1.8:5555" 或 USB序列号
    QString state;      //状态: "device"(就绪) / "offline" / "unauthorized"
    QString model;      //设备型号，如 "Pixel 7 Pro"（无则空）
};

//目录条目：adb shell ls -la 输出的一行解析结果
struct AdbDirEntry {
    QString name;       //文件或文件夹名
    bool isDir = false;      //是否为目录
    qint64 size = 0;         //字节大小（目录为0）
    QString date;       //修改日期字符串
};

#endif // ADBTYPES_H
