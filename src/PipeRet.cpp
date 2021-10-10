#include <string>
#include "../include/PipeRet.h"

PipeRet PipeRet::failure(const std::string &msg)
{
    return PipeRet(false, msg);
}
//---------------------------------------------------------------------------
PipeRet PipeRet::success(const std::string &msg)
{
    return PipeRet(true, msg);
}
