#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
//----------------------------------------------------------------------------------------------------------------------
#include <queue>
#include <mutex>
#include <sys/eventfd.h>
#include <unistd.h>
//----------------------------------------------------------------------------------------------------------------------
template <class T>
class MessageQueue {
public:
    int size_limit = 1000;
protected:
    int evFd=-1;
    std::queue<T> itemsQ;
    std::mutex mtx;
    bool stopped;
    uint64_t rejected=0;
public:
    MessageQueue(bool blocking = true);
    virtual ~MessageQueue() {if(evFd >= 0) close(evFd);}
    int push(T item, bool forceStart = false);
    T pop();
    int enqueued();
    uint64_t GetRejected(){return rejected;}
    bool empty() {return itemsQ.empty();}
    int fd() { return evFd;}
    void stop_and_clear();
    void start() {stopped = false;}
    operator int() const {return evFd;}
};
//----------------------------------------------------------------------------------------------------------------------
template <class T>
int MessageQueue<T>::enqueued()
{
    std::lock_guard<std::mutex> lock(mtx);
    return itemsQ.size();
}
//----------------------------------------------------------------------------------------------------------------------
template <class T>
MessageQueue<T>::MessageQueue(bool blocking)
        : stopped(true)
{
    int flags = EFD_SEMAPHORE;
    if(!blocking)
        flags |= EFD_NONBLOCK;
    evFd = eventfd(0, flags);
}
//----------------------------------------------------------------------------------------------------------------------
template <class T>
int MessageQueue<T>::push(T item, bool forceStart)
{
    std::lock_guard<std::mutex> lock(mtx);
    if(size_limit > 0 && itemsQ.size() >= size_limit)
    {
        rejected++;
        return -1;
    }
    if(!stopped || forceStart) 
    {
        stopped = false;
        itemsQ.push(std::move(item));
        uint64_t val = 1;
        write(evFd, &val, sizeof(val));
    }
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
template <class T>
T MessageQueue<T>::pop()
{
    uint64_t val;
    ssize_t res = read(evFd, &val, sizeof(val));
    if(res > 0) {
        std::lock_guard<std::mutex> lock(mtx);
        if (itemsQ.size()) {
            T item = std::move(itemsQ.front());
            itemsQ.pop();
            return item;
        }
    }
    return nullptr;
}
//----------------------------------------------------------------------------------------------------------------------
template <class T>
void MessageQueue<T>::stop_and_clear()
{
    std::lock_guard<std::mutex> lock(mtx);
    stopped = true;
    while(!itemsQ.empty())
    {
        uint64_t val;
        ssize_t res = read(evFd, &val, sizeof(val));
        itemsQ.pop();
    }
}
//----------------------------------------------------------------------------------------------------------------------
#endif //MESSAGE_QUEUE_H
