#define LOG_TAG "NXHWCService"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "NXHWCService.h"

// using namespace android;

namespace android {

status_t BnNXHWCService::onTransact(uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags)
{
    data.checkInterface(this);
    int32_t val = data.readInt32();

    ALOGV("BnNXHWCService::onTransact(%i) %i", code, val);

    switch (code) {
    case HWC_SCENARIO_PROPERTY_CHANGED:
        hwcScenarioChanged(val);
        break;

    case HWC_RESC_SCALE_FACTOR_CHANGED:
        hwcRescScaleFactorChanged(val);
        break;

    case HWC_RESOLUTION_CHANGED:
        hwcResolutionChanged(val);
        break;

    case HWC_SCREEN_DOWNSIZING_CHANGED:
        hwcScreenDownSizingChanged(val);
        break;

    default:
        return BBinder::onTransact(code, data, reply, flags);
    }

    return NO_ERROR;
}

status_t NXHWCService::registerListener(sp<PropertyChangeListener> listener)
{
    mListener = listener;
    return NO_ERROR;
}

void NXHWCService::hwcScenarioChanged(int32_t scenario)
{
    if (mListener != NULL)
        mListener->onPropertyChanged(HWC_SCENARIO_PROPERTY_CHANGED, scenario);
}

void NXHWCService::hwcRescScaleFactorChanged(int32_t factor)
{
    if (mListener != NULL)
        mListener->onPropertyChanged(HWC_RESC_SCALE_FACTOR_CHANGED, factor);
}

void NXHWCService::hwcResolutionChanged(int32_t resolution)
{
    if (mListener != NULL)
        mListener->onPropertyChanged(HWC_RESOLUTION_CHANGED, resolution);
}

void NXHWCService::hwcScreenDownSizingChanged(int32_t downsizing)
{
    if (mListener != NULL)
        mListener->onPropertyChanged(HWC_SCREEN_DOWNSIZING_CHANGED, downsizing);
}

sp<INXHWCService> getNXHWCService()
{
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == NULL) {
        ALOGE("can't get service manager!!!");
        return NULL;
    }
    sp<IBinder> binder = sm->getService(String16("NXHWCService"));
    if (binder == NULL) {
        ALOGE("can't get binder for NXHWCService");
        return NULL;
    }
    sp<INXHWCService> service = interface_cast<INXHWCService>(binder);
    if (service == NULL) {
        ALOGE("can't get NXHWCService");
        return NULL;
    }
    return service;
}

static void *_startNXHWCServiceFunc(void *data)
{
    ALOGD("_startNXHWCServiceFunc...");
    NXHWCService *service = static_cast<NXHWCService *>(data);
    defaultServiceManager()->addService(String16("NXHWCService"), service);
    android::ProcessState::self()->startThreadPool();
    ALOGD("NXHWCService is now ready");
    IPCThreadState::self()->joinThreadPool();
    ALOGD("NXHWCService thread joined");
    return NULL;
}

NXHWCService *startNXHWCService()
{
    NXHWCService *service = new NXHWCService();
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t threadId;
    int ret = pthread_create(&threadId, &attr, _startNXHWCServiceFunc, service);
    pthread_attr_destroy(&attr);
    if (ret < 0) {
        ALOGE("can't create NXHWCService thread...");
        return NULL;
    }

    return service;
}

}; // namespace
