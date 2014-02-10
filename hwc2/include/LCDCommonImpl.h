#ifndef _LCDCOMMONIMPL_H
#define _LCDCOMMONIMPL_H

class HWCImpl;

namespace android {

class LCDCommonImpl: public HWCImpl
{
public:
    LCDCommonImpl();
    LCDCommonImpl(int rgbID, int videoID);
    LCDCommonImpl(int rgbID, int videoID, int width, int height);
    virtual ~LCDCommonImpl();

    virtual int enable();
    virtual int disable();

protected:
    virtual void init();

private:
    int mFBFd;

};

}; // namespace
#endif
