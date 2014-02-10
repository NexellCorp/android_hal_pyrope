#ifndef _NX_QUEUE_H
#define _NX_QUEUE_H

#include <utils/Vector.h>

namespace android {

template <class T>
class NXQueue
{
public:
    NXQueue() {
    };

    virtual ~NXQueue() {
    }

    void queue(const T& item) {
        vector.push_front(item);
    }

    const T& dequeue() {
        //const T& item = vector[vector.size() - 1];
        const T& item = vector.top();
        vector.pop();
        return item;
    }

    const T& getHead() {
        return vector[vector.size() - 1];
    }

    bool isEmpty() {
        return vector.empty();
    }

    size_t size() {
        return vector.size();
    }

    void clear() {
        return vector.clear();
    }

    const T& getItem(int index) {
        return vector[index];
    }

private:
    Vector<T> vector;
};

}; // namespace

#endif
