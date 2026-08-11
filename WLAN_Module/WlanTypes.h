#ifndef WLANTYPES_H
#define WLANTYPES_H
//WLAN模块跨层共享的数据结构：设备信息、文件元数据、帧类型枚举
//对应LocalSend的 model/discovery.rs(设备信息) + model/transfer.rs(文件元数据)
//经过确认WLAN模块几乎只能用于PC-PC，还都是同时运行Memoria的场景
//这个场景想想都觉得用处太少，看看后面测试这个功能还用得上不？用不上的话就去了

#include <QString>
#include <QHostAddress>

//对端设备信息（UDP多播发现得到）
struct PeerInfo {
    QString alias;          //设备显示名
    QString fingerprint;    //设备唯一标识(SHA-256)
    QHostAddress ip;        //设备可达IP（多播数据报的发送方地址）
    quint16 port = 0;       //对端TCP传输端口
};

//单次传输中的文件元数据
struct TransferFileInfo {
    QString id;             //文件在本次会话中的唯一id
    QString relativePath;   //相对路径，如 "视频A/Video.m4s"，用于保持目录结构
    qint64 size = 0;        //文件字节数
};

//自定义TCP帧类型（JSON头中"type"字段取值）
enum class FrameType {
    PrepareImport,  //请求导入，含文件清单
    Accept,         //接受，含sessionId + acceptedIds
    FileData,       //文件数据，含fileId/path/size，其后紧跟size字节原始数据
    Complete,       //全部完成
    Cancel          //取消会话
};

#endif // WLANTYPES_H
