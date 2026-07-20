#include "logger.h"
#include <QDebug>

//获取单例实例
Logger* Logger::instance() {
    static Logger instance;
    return &instance;
}

//设置最低输出级别
void Logger::setMinLevel(Level level) {
    m_minLevel = level;
}

//统一日志接口：根据级别输出带模块前缀的消息
void Logger::log(Level level, const QString &module, const QString &msg) {
    //级别过滤：低于最低级别的直接丢弃
    if (level < m_minLevel) {
        return;
    }

    //格式化为 [模块名] 消息内容
    QString formatted = QString("[%1] %2").arg(module, msg);

    switch (level) {
    case Debug:
        qDebug() << formatted;
        break;
    case Warning:
        qWarning() << formatted;
        break;
    case Critical:
        qCritical() << formatted;
        break;
    }
}
