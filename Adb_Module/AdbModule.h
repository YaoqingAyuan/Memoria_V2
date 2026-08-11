#ifndef ADBMODULE_H
#define ADBMODULE_H
//ADB模块类
/* 职责：通过 adb.exe 与Android设备通信，完成设备发现/配对/连接/目录浏览/文件拉取
 * 设计依据：与FFmpeg_module一致的QProcess异步模式
 *   1.selfCheck → 2.设备管理(pair/connect/devices) → 3.文件操作(listDir/pull)
 *
 * 单QProcess串行执行：同一时刻只运行一条adb命令
 * Logger在启动前打印完整adb命令（便于错误定位，与FFmpeg_module一致）
 */

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include "AdbTypes.h"

class AdbModule : public QObject
{
    Q_OBJECT
public:
    explicit AdbModule(QObject *parent = nullptr);
    ~AdbModule();

    // ========== 自检验证 ==========
    //检测ADB环境：优先用户环境的adb，回退到随包 ADB_tools/bin/adb.exe
    //返回adb.exe完整路径，失败返回空字符串
    QString selfCheck();
    QString getAdbPath() const { return m_adbPath; }

    // ========== 设备管理 ==========
    //刷新设备列表（异步，结果通过 deviceListChanged 信号返回）
    void refreshDevices();

    //配对新设备（无线调试配对，adb pair ip:port code）
    void pairDevice(const QString &ip, int port, const QString &code);

    //连接已配对设备（adb connect ip:port）
    void connectDevice(const QString &ip, int port);

    //断开设备（adb disconnect serial）
    void disconnectDevice(const QString &serial);

    // ========== 文件操作 ==========
    //列出远程目录内容（异步，结果通过 dirListReady 信号返回）
    void listDir(const QString &serial, const QString &remotePath);

    //拉取远程文件/文件夹到本地（异步，进度通过 pullProgressChanged 信号返回）
    void pullFile(const QString &serial, const QString &remotePath, const QString &localPath);

    //停止当前操作
    void stop();

signals:
    //设备列表更新
    void deviceListChanged(const QList<AdbDeviceInfo> &devices);

    //目录列表就绪
    void dirListReady(const QString &path, const QList<AdbDirEntry> &entries);

    //拉取进度更新（percentage: 0-100）
    void pullProgressChanged(int percentage);

    //拉取完成
    void pullFinished(const QString &localPath, bool success, const QString &message);

    //配对结果
    void pairResult(bool success, const QString &message);

    //连接结果
    void connectResult(bool success, const QString &message);

    //错误
    void errorOccurred(const QString &errorMessage);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    //当前操作类型（决定 onFinished 中如何路由输出）
    enum class Op {
        Idle,
        RefreshDevices,
        Pair,
        Connect,
        Disconnect,
        ListDir,
        Pull
    };

    QProcess *m_process;
    QString m_adbPath;
    Op m_currentOp = Op::Idle;

    //操作上下文（供 onFinished 使用）
    QString m_opSerial;         //当前操作的设备序列号
    QString m_opRemotePath;     //当前操作的远程路径
    QString m_opLocalPath;      //当前操作的本地路径（pull用）

    //输出缓冲（累积 stdout/stderr，进程结束时统一解析）
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;

    //正则：解析 adb pull 的百分比进度 [  45%]
    QRegularExpression m_pullProgressRegex;
    //正则：解析 ls -la 行  drwxrwx--- 3 root root 4096 2024-01-15 10:30 name
    QRegularExpression m_lsLineRegex;

    //执行 adb 命令（统一入口：打印日志、启动进程）
    void executeAdb(const QStringList &args);

    //解析 adb devices -l 输出
    QList<AdbDeviceInfo> parseDevices(const QString &output);

    //解析 adb shell ls -la 输出
    QList<AdbDirEntry> parseDirListing(const QString &output);
};

#endif // ADBMODULE_H
