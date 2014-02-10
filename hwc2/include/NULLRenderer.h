#ifndef _NULLRENDERER_H
#define _NULLRENDERER_H

namespace android {

class HWCRenderer;

class NULLRenderer: public HWCRenderer
{
public:
    NULLRenderer(int id)
        :HWCRenderer(id) {
    }

    virtual ~NULLRenderer() {
    }

    virtual int render() {
        return 0;
    }
};

}; // namespace

#endif
