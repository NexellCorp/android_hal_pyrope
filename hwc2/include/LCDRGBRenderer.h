#ifndef _LCDRGBRENDERER_H
#define _LCDRGBRENDERER_H

class HWCRenderer;
struct fb_var_screeninfo;

namespace android {

class LCDRGBRenderer: public HWCRenderer
{
public:
    LCDRGBRenderer(int id);

    virtual ~LCDRGBRenderer();

    virtual int render(int *fenceFd = NULL);

private:
    int mFBFd;
    int mFBLineLength;
    struct fb_var_screeninfo mFBVarInfo;
};

}; // namespace

#endif
