#ifndef LOGGER_H
#define LOGGER_H
//日志(Logger)类：全局单例，提供统一的三级日志接口(Debug/Warning/Critical)
//供所有模块复用，便于后续扩展(日志级别过滤、写入文件、UI面板等)

#include <QString>

class Logger
{
public:
    //日志级别枚举
    enum Level {
        Debug,      //调试信息
        Warning,    //警告信息
        Critical    //严重错误信息
    };

    //获取单例实例
    static Logger* instance();

    //统一日志接口(内部根据 level 选择输出通道)
    void log(Level level, const QString &module, const QString &msg);

    //便捷接口：三级日志，需传入模块名(如 "Parser"、"Queue"、"FFmpeg")
    void debug(const QString &module, const QString &msg)    { log(Debug, module, msg); }
    void warning(const QString &module, const QString &msg)  { log(Warning, module, msg); }
    void critical(const QString &module, const QString &msg) { log(Critical, module, msg); }

    //设置最低输出级别(低于此级别的日志将被丢弃)，默认为 Debug(全输出)
    void setMinLevel(Level level);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Level m_minLevel = Debug;   //最低输出级别
};

#endif // LOGGER_H
