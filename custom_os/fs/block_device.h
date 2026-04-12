#pragma once

#include <memory>
#include <string_view>

#include "mm/page_alloc.h"
#include "lib/flags.h"
#include "lib/list.h"
#include "lib/spinlock.h"
#include "kernel/error.h"

class BlockDevice;

struct IoBuf {
    void* data;
    size_t size;
};

class BlockDevice {
protected:
    SpinLock lock_;
    size_t sector_size_ = 0;
    const char* name_ = nullptr;

public:
    enum class RequestFlag {
        Write = 1 << 0,
        Error = 1 << 1,
    };
    using RequestFlags = BitFlags<RequestFlag>;

    struct BaseRequest : RefCounted {
        BlockDevice* dev = nullptr;
        RequestFlags flags;
        IoBuf buf;
        size_t sector = 0;
        ListNode list;

        virtual void OnEnd() noexcept {}
        virtual ~BaseRequest() {}
    };

    template <typename Cb>
    struct Request : BaseRequest {
    private:
        Cb cb;

    public:
        Request(Cb cb) : cb(std::move(cb)) {}

        virtual void OnEnd() noexcept override {
            cb(*this);
        }
    };

    using RequestPtr = IntrusiveSharedPtr<BaseRequest>;

public:
    ListNode dev_list_;

public:
    static kern::Errno Register(BlockDevice& dev, std::string_view name) noexcept;
    static BlockDevice* ByName(std::string_view name) noexcept;
    void EndRequest(RequestPtr req) noexcept;

    size_t SectorSize() const noexcept {
        return sector_size_;
    }

    virtual kern::Errno Submit(RequestPtr req) noexcept = 0;
};
