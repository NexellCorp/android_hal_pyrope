#ifndef _NX_MESSAGE_THREAD_H
#define _NX_MESSAGE_THREAD_H

#include <utils/Thread.h>
#include "NXQueue.h"

namespace android {

template <class T>
class NXMessageThread: public Thread
{
public:
    NXMessageThread()
        :Thread(false)
    {
    }
    virtual ~NXMessageThread() {
    }

    virtual void sendMessage(const T& message) {
        Mutex::Autolock lock(QLock);
        queue.queue(message);
        QCondition.signal();
    }

    virtual void sendMessageDelayed(const T& message, uint32_t delayUs) {
        usleep(delayUs);
        sendMessage(message);
    }

private:
    // virtual bool processMessage(const T& message) = 0;
    virtual bool processMessage(const T message) = 0;
    virtual bool threadLoop() {
        QLock.lock();
        while(queue.isEmpty()) {
            QCondition.wait(QLock);
        }
        const T& msg = queue.dequeue();
        QLock.unlock();
        return processMessage(msg);
    }

protected:
    NXQueue<T> queue;

private:
    Mutex QLock;
    Condition QCondition;
};

}; // namespace

#endif
