#pragma once
#include "callback_vector.h"
#include "ipc_codec.h"
#include "ipc_header.h"
#include "libipc/ipc.h"
#include "threadpool.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
namespace ipcbus
{

namespace
{
constexpr char const ipc_bus_name_[] = "ipc-bus";
}

class IpcBus
{
  public:
    static IpcBus &instance()
    {
        static IpcBus _instance;
        return _instance;
    }
    ~IpcBus()
    {
        for (auto &th : serviceManager_)
        {
            th.second = true;
        }
        {
            std::lock_guard<std::mutex> guard{callbacksMutex_};
            callbacks_.clear();
        }
    }
    // nocopyable
    IpcBus(const IpcBus &) = delete;
    IpcBus(IpcBus &&) = delete;

    IpcBus &operator=(IpcBus &&) = delete;
    IpcBus &operator=(const IpcBus &) = delete;

    template <typename T> int subscribe(std::function<void(const T &)> callback)
    {
        auto token = tokener_++;
        subscribe<T>(token, std::move(callback));
        return token;
    }

    void unsubscribe(const int token)
    {
        Threadpool::instance().commit([this, token]() {
            std::lock_guard<std::mutex> guard{callbacksMutex_};
            for (auto &element : callbacks_)
            {
                element.second->remove(token);
            }
        });
    }

    template <typename T> bool publish(T event)
    {
        static_assert(internal::validateEvent<T>(), "Invalid event");
        msgpack_codec codec;
        auto body = codec.pack_args(typeid(event).hash_code(), event);
        IpcHeader_t header = {MAGIC_NUM, 0, RequestType_t::SUB_PUB, typeid(event).hash_code(), (uint32_t)body.size()};
        buffer_type buffer(msgpack_codec::init_size);
        buffer.write((const char *)&header, sizeof(IpcHeader_t));
        buffer.write(body.data(), body.size());
        ipc::channel ipc_sender{ipc_bus_name_, ipc::sender};
        auto ret = ipc_sender.send(buffer.data(), buffer.size());
        // ipc_sender.disconnect();
        return ret;
    }

    template <typename T> bool publish(const std::string &service_name, T event)
    {
        static_assert(internal::validateEvent<T>(), "Invalid event");
        msgpack_codec codec;
        auto body = codec.pack_args(typeid(event).hash_code(), event);
        IpcHeader_t header = {MAGIC_NUM, 0, RequestType_t::SUB_PUB, typeid(event).hash_code(), (uint32_t)body.size()};
        buffer_type buffer(msgpack_codec::init_size);
        buffer.write((const char *)&header, sizeof(IpcHeader_t));
        buffer.write(body.data(), body.size());
        ipc::channel ipc_sender{service_name.c_str(), ipc::sender};
        auto ret = ipc_sender.send(buffer.data(), buffer.size());
        // ipc_sender.disconnect();
        return ret;
    }

    void add_service(const std::string &service_name)
    {
        if (serviceManager_.size() >= 2)
        {
            std::cout << "just support one service, too much is no sence in one process" << std::endl;
            return;
        }
        if (serviceManager_.count(service_name) == 0)
        {
            start(service_name);
        }
    }
    void stop()
    {
        for (auto &th : serviceManager_)
        {
            th.second = true;
        }
        {
            std::lock_guard<std::mutex> guard{callbacksMutex_};
            callbacks_.clear();
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

  private:
    IpcBus() : tokener_(0) { start(ipc_bus_name_); }
    static bool isValidMsg(ipc::buff_t *buf)
    {
        if (buf->empty())
        {
            std::cerr << "receive buffer empty" << std::endl;
            return false;
        }
        if (buf->size() < sizeof(IpcHeader_t))
        {
            std::cerr << "Invalid buffer size: " << buf->size() << std::endl;
            return false;
        }
        IpcHeader_t *header = (IpcHeader_t *)(buf->data());
        if (header->magic != MAGIC_NUM)
        {
            std::cerr << "Invalid MAGIC_NUM: " << header->magic << std::endl;
            return false;
        }
        if (header->data_len >= IPC_MAX_BUF_LEN)
        {
            std::cerr << "Invalid request len: " << header->data_len << std::endl;
            return false;
        }
        return true;
    }
    void start(const std::string &service_name)
    {
        std::thread thd([this, service_name]() {
            ipc::channel ipc_receiver{service_name.c_str(), ipc::receiver};
            serviceManager_.emplace(service_name, false);
            while (!serviceManager_.at(service_name))
            {
                ipc::buff_t buf = ipc_receiver.recv();
                if (!isValidMsg(&buf))
                {
                    continue;
                }
                IpcHeader_t *header = (IpcHeader_t *)(buf.data());
                auto type_id = header->data_type_id;
                size_t body_len = header->data_len;
                ipc::buff_t body{((char *)buf.data() + IPC_HEAD_LEN), body_len};
                post_handle(type_id, std::move(body));
            }
            // ipc_receiver.disconnect();
            std::cout << "=======ipc service:" << service_name << "stop" << std::endl;
        });
        thd.detach();
    }

    void stop(const std::string &service_name)
    {
        try
        {
            serviceManager_.at(service_name) = true;
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }
    }
    template <typename T> void post_handle(uint64_t reqId, T buf)
    {
        static_assert(internal::validateEvent<T>(), "Invalid event");

        Threadpool::instance().commit([this, reqId, capbuf = std::move(buf)]() {
            std::lock_guard<std::mutex> guard{callbacksMutex_};

            using Vector = internal::AsyncCallbackVector<T>;
            auto found = callbacks_.find(reqId);
            if (found == callbacks_.end())
            {
                return; // no such notifications
            }

            std::unique_ptr<internal::CallbackVector> &vector = found->second;
            assert(dynamic_cast<Vector *>(vector.get()));
            Vector *callbacks = static_cast<Vector *>(vector.get());
            for (const auto &element : callbacks->container)
            {
                try
                {
                    element.second(capbuf);
                }
                catch (const std::exception &e)
                {
                    std::cerr << e.what() << '\n';
                }
            }
        });
    }
    template <typename T> void subscribe(const int token, std::function<void(const T &)> cb)
    {
        auto eventHandler = [cb](const ipc::buff_t &buf) {
            try
            {
                msgpack_codec codec;
                auto tp = codec.unpack<std::tuple<size_t, T>>((const char *)buf.data(), buf.size());
                auto event = std::get<1>(tp);
                cb(event);
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }
        };
        std::lock_guard<std::mutex> guard{callbacksMutex_};
        using Vector = internal::AsyncCallbackVector<ipc::buff_t>;
        assert(cb && "callback should be valid");
        std::unique_ptr<internal::CallbackVector> &vector = callbacks_[internal::type_id<T>()];
        if (vector == nullptr)
        {
            vector.reset(new Vector{});
        }
        assert(dynamic_cast<Vector *>(vector.get()));
        Vector *callbacks = static_cast<Vector *>(vector.get());
        callbacks->add(token, eventHandler);
    }

    template <typename T> bool doRequest(const std::string &service_name, T event)
    {
        static_assert(internal::validateEvent<T>(), "Invalid event");
        msgpack_codec codec;
        auto reqId = reqseq_++;
        auto body = codec.pack_args(typeid(event).hash_code(), event);
        IpcHeader_t header = {MAGIC_NUM, reqId, RequestType_t::REQ_RESP, typeid(event).hash_code(), (uint32_t)body.size()};
        buffer_type buffer(msgpack_codec::init_size);
        buffer.write((const char *)&header, sizeof(IpcHeader_t));
        buffer.write(body.data(), body.size());
        ipc::channel ipc_sender{service_name.c_str(), ipc::sender};
        auto ret = ipc_sender.send(buffer.data(), buffer.size());
        // ipc_sender.disconnect();
        return reqId;
    }

  private:
    std::atomic<uint64_t> tokener_;
    std::atomic<uint64_t> reqseq_;
    std::unordered_map<internal::type_id_t, std::unique_ptr<internal::CallbackVector>> callbacks_;
    std::mutex callbacksMutex_;
    std::unordered_map<std::string, bool> serviceManager_;
};
} // namespace ipcbus