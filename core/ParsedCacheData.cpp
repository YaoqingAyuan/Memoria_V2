#include "ParsedCacheData.h"
//Parsed(解析的)Cache(缓存)Data(数据)

ParsedCacheData::ParsedCacheData()
    : rowStatus(Empty)
{
    //VideoInfo中新增的videoType默认为Unknown
    videoInfo.videoType = Unknown;
}
