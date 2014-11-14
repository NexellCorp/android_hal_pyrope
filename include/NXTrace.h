#ifndef _NX_TRACE_H
#define _NX_TRACE_H

#ifdef ANDROID
#include <utils/Log.h>
#endif

class NXTrace {
public:
    NXTrace(const char *funcName) {
#ifdef ANDROID
        ALOGD("%s entered", funcName);
#else
        fprintf(stderr, "%s entered\n", funcName);
#endif
        mFuncName = new char[strlen(funcName) + 2];
        strcpy(mFuncName, funcName);
    }

    virtual ~NXTrace() {
#ifdef ANDROID
        ALOGD("%s exit", mFuncName);
#else
        fprintf(stderr, "%s exit\n", mFuncName)
#endif
        delete [] mFuncName;
    }

private:
        char *mFuncName;
};

#endif
