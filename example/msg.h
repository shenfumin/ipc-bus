#include <msgpack.hpp>
#include <string>
struct Person_t
{
    int id;
    std::string name;
    int age;

    MSGPACK_DEFINE(id, name, age);
};

struct Response_t
{
    int result;
    MSGPACK_DEFINE(result);
};