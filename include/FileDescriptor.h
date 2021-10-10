// Header guard
#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

class FileDescriptor
{
public:
    void setSocketFd(int fd)
    {
        mSocketFd = fd;
    }
    int getSocketFd() const
    {
        return mSocketFd;
    }
private:
    int mSocketFd = 0;
};
#endif
