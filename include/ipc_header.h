#pragma once
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
namespace ipcbus
{
enum class RequestType_t : uint8_t
{
    REQ_RESP,
    SUB_PUB,
};

static const uint8_t MAGIC_NUM = 39;
#pragma pack(4)
struct IpcHeader_t
{
    uint8_t magic;
    uint64_t req_id;
    RequestType_t req_type;
    uint64_t data_type_id;
    uint32_t data_len;
};
#pragma pack()

static const size_t IPC_MAX_BUF_LEN = 1048576 * 8; // 10M
static const size_t IPC_HEAD_LEN = sizeof(IpcHeader_t);
static const size_t IPC_INIT_BUF_SIZE = 2 * 1024;
} // namespace ipcbus