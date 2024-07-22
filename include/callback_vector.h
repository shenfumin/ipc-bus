#pragma once
#include <cstddef>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <vector>
namespace ipcbus
{
namespace internal
{
using type_id_t = std::size_t;

template <typename T> type_id_t type_id() { return typeid(T).hash_code(); }

template <class Event> constexpr bool validateEvent()
{
    static_assert(std::is_const<Event>::value == false, "Struct must be without const");
    static_assert(std::is_volatile<Event>::value == false, "Struct must be without volatile");
    static_assert(std::is_reference<Event>::value == false, "Struct must be without reference");
    static_assert(std::is_pointer<Event>::value == false, "Struct must be without pointer");
    return true;
}

struct CallbackVector
{
    virtual ~CallbackVector() = default;

    virtual void remove(const int token) = 0;
};

template <typename Event> struct AsyncCallbackVector : public CallbackVector
{
    using CallbackType = std::function<void(const Event &)>;
    using ContainerElement = std::pair<int, CallbackType>;
    std::vector<ContainerElement> container;

    virtual void remove(const int token) override
    {
        auto removeFrom = std::remove_if(container.begin(), container.end(), [token](const ContainerElement &element) { return element.first == token; });
        if (removeFrom != container.end())
        {
            container.erase(removeFrom, container.end());
        }
    }

    void add(const int token, CallbackType callback) { container.emplace_back(token, std::move(callback)); }
};

} // namespace internal
} // namespace ipcbus
