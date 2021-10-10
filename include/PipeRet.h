// Header guard
#ifndef PIPE_RET_H
#define PIPE_RET_H

class PipeRet
{
public:
    PipeRet() = default;
    PipeRet(bool successFlag, const std::string &msg) :
        mSuccessFlag{successFlag},
        mMsg{msg}
    {
    }

    std::string message() const
    {
        return mMsg;
    }
    bool isSuccessful() const
    {
        return mSuccessFlag;
    }

    static PipeRet failure(const std::string & msg);
    static PipeRet success(const std::string &msg = "");

private:
    bool mSuccessFlag = false;
    std::string mMsg = "";
};
#endif
