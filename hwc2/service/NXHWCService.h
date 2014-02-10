#ifndef _NXHWC_SERVICE_H
#define _NXHWC_SERVICE_H

#include <utils/RefBase.h>
#include <utils/Log.h>

#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

namespace android {

// define interface
class INXHWCService : public IInterface
{
public:
    enum {
        HWC_SCENARIO_PROPERTY_CHANGED = IBinder::FIRST_CALL_TRANSACTION,
        HWC_RESOLUTION_CHANGED,
        HWC_RESC_SCALE_FACTOR_CHANGED
    };

    virtual void hwcScenarioChanged(int32_t scenario) = 0;
    virtual void hwcResolutionChanged(int32_t resolution) = 0;
    virtual void hwcRescScaleFactorChanged(int32_t factor) = 0;

    DECLARE_META_INTERFACE(NXHWCService);
};

// client
class BpNXHWCService : public BpInterface<INXHWCService>
{
public:
    BpNXHWCService(const sp<IBinder> &impl) : BpInterface<INXHWCService>(impl) {
        ALOGD("BpNXHWCService::BpNXHWCService");
    }

    virtual void hwcScenarioChanged(int32_t scenario) {
        Parcel data;
        data.writeInterfaceToken(INXHWCService::getInterfaceDescriptor());
        data.writeInt32(scenario);

        ALOGD("transact %d", scenario);
        remote()->transact(HWC_SCENARIO_PROPERTY_CHANGED, data, NULL);
    }

    virtual void hwcRescScaleFactorChanged(int32_t factor) {
        Parcel data;
        data.writeInterfaceToken(INXHWCService::getInterfaceDescriptor());
        data.writeInt32(factor);

        ALOGD("transact %d", factor);
        remote()->transact(HWC_RESC_SCALE_FACTOR_CHANGED, data, NULL);
    }

    virtual void hwcResolutionChanged(int32_t resolution) {
        Parcel data;
        data.writeInterfaceToken(INXHWCService::getInterfaceDescriptor());
        data.writeInt32(resolution);

        ALOGD("transact %d", resolution);
        remote()->transact(HWC_RESOLUTION_CHANGED, data, NULL);
    }
};

IMPLEMENT_META_INTERFACE(NXHWCService, "NXHWCService");

// server
class BnNXHWCService : public BnInterface<INXHWCService>
{
    virtual status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags = 0);
};

class NXHWCService : public BnNXHWCService
{
public:
    NXHWCService() {
    }
    virtual ~NXHWCService() {
    }

    struct PropertyChangeListener: virtual public RefBase {
        virtual void onPropertyChanged(int code, int val) = 0;
    };

    status_t registerListener(sp<PropertyChangeListener> listener);

    virtual void hwcScenarioChanged(int32_t scenario);
    virtual void hwcRescScaleFactorChanged(int32_t factor);
    virtual void hwcResolutionChanged(int32_t resolution);

private:
    sp<PropertyChangeListener> mListener;
};

sp<INXHWCService> getNXHWCService();
NXHWCService *startNXHWCService();

}; // namespace

#endif
