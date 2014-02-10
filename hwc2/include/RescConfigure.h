#ifndef _RESCCONFIGURE_H
#define _RESCCONFIGURE_H

namespace android {

class RescConfigure
{
public:
    virtual ~RescConfigure() {
    }

    virtual int configure(int srcWidth, int srcHeight, int dstWidth, int dstHeight, uint32_t format, int scaleFactor);
};

}; // namespace

#endif
