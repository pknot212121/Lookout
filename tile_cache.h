#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ratio>
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>
#include "gpu_utils.h"
#include "webgpu/webgpu_cpp.h"
#include <stb_image.h>
#include <iostream>
#include "lookout_struct.h"

// A way of hashing coordinates of a tile onto a one uint32_t
// | 6 bits - z |    13 bits x    |    13 bits  y   |
struct TileKey
{
    uint32_t z = 0, x = 0, y = 0;
    
    TileKey(uint32_t z, uint32_t x, uint32_t y): z(z), x(x), y(y) {}

    static constexpr uint32_t COORD_BITS = 13;
    static constexpr uint32_t COORD_MASK = (1u << COORD_BITS) - 1;

    uint32_t pack() const { return (z << (2 * COORD_BITS)) | (x << COORD_BITS) | y; }

    static TileKey unpack(uint32_t p)
    {
        return { p >> (2 * COORD_BITS), (p >> COORD_BITS) & COORD_MASK, p & COORD_MASK };
    }
};

class TileCache
{
    public:

    using FetchResult = std::function<void(bool ok, int status, std::vector<uint8_t> bytes)>;
    using FetchFn = std::function<void(const TileKey&, FetchResult)>;

    enum class State
    {
        Queued,
        Loading,
        Downloaded,
        Ready,
        Empty,
        Failed
    };

    TileCache() = default;

    TileCache(
        // Device object is small enough to be passed by value
        // It is only basic device data
        wgpu::Device device,
        wgpu::Texture texture,
        uint32_t slotCount,
        uint32_t texSize,
        FetchFn fetch,
        uint32_t maxInFlight = 8
    ) :
    device_(std::move(device)),
    texture_(std::move(texture)),
    texSize_(texSize),
    fetch_(std::move(fetch)),
    maxInFlight_(maxInFlight)
    {
        std::cout << "SLOTCOUNT IS: " << slotCount << std::endl;
        freeSlots_.reserve(slotCount);
        for (uint32_t i = slotCount; i > 0; i--)
        {
            freeSlots_.push_back(i-1);
        }
    }

    void beginFrame() {++frame_;}

    void request(const TileKey& k, float priority)
    {
        auto [it, inserted] = entries_.try_emplace(k.pack());
        Entry& e = it->second;
        if (inserted)
        {
            e.state = State::Queued;
        }
        e.requestedFrame = frame_;
        e.lastUsed = frame_;
        e.priority = priority;
        if (e.state == State::Failed && nowMs() >= e.retryAt)
        {
            e.state = State::Queued;
        }
    }

    std::optional<uint32_t> slotOf(const TileKey& k)
    {
        auto it = entries_.find(k.pack());
        if (it == entries_.end() || it->second.state != State::Ready)
        {
            return std::nullopt;
        }
        it->second.lastUsed = frame_;
        return it->second.slot;
    }

    bool isEmpty(const TileKey& k) const
    {
        auto it = entries_.find(k.pack());
        return it != entries_.end() && it->second.state == State::Empty;
    }

    void update(double uploadBudgetMs = 2.0)
    {
        startFetches();
        uploadDownloaded(uploadBudgetMs);
    }

    std::vector<TileInstanceAttributes> getInstances(size_t maxCount) const
    {
        std::vector<TileInstanceAttributes> instances;
        for (const auto& [p, e] : entries_)
        {
            if (e.state != State::Ready || e.requestedFrame != frame_) continue;
            if (instances.size() >= maxCount) break;
            TileKey key = TileKey::unpack(p);
            instances.push_back({key.z, key.x, key.y, e.slot});
        }
        return instances;
    }

    uint32_t inFlight() const {return inFlight_;}
    size_t freeSlotsCount() const {return freeSlots_.size();}

    private:

    struct Entry
    {
        State state = State::Queued;
        uint32_t slot = UINT32_MAX;
        uint64_t lastUsed = 0;
        uint64_t requestedFrame = 0;
        float priority = 0.0f;
        uint32_t retries = 0;
        double retryAt = 0.0;
        std::vector<uint8_t> bytes;
    };

    static constexpr uint32_t MAX_RETRIES = 3;
    static constexpr uint32_t STALE_FRAMES = 120;

    static double nowMs()
    {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    void startFetches()
    {
        std::vector<uint32_t> candidates;

        for (auto it = entries_.begin(); it != entries_.end();)
        {
            const Entry& e = it->second;
            const bool requestedNow = (e.requestedFrame == frame_);

            const bool dropQueued = !requestedNow && e.state == State::Queued;
            const bool dropStaleBytes = !requestedNow
                                        && e.state == State::Downloaded
                                        && frame_ - e.requestedFrame > STALE_FRAMES;
            if (dropQueued || dropStaleBytes)
            {
                it = entries_.erase(it);
                continue;
            }

            if (e.state == State::Queued)
            {
                candidates.push_back(it->first);
            }
            ++it;
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            [this](uint32_t a, uint32_t b)
            {
                return entries_.at(a).priority < entries_.at(b).priority;
            }
        );

        for (uint32_t k : candidates)
        {
            if (inFlight_ >= maxInFlight_)
            {
                break;
            }
            startFetch(k);
        }
    }

    void startFetch(uint32_t packed)
    {
        entries_.at(packed).state = State::Loading;
        ++inFlight_;
        fetch_(TileKey::unpack(packed),
            [this, packed](bool ok, int status, std::vector<uint8_t> bytes)
            {
                onFetched(packed, ok, status, std::move(bytes));
            }
        );
    }

    void onFetched(uint32_t packed, bool ok, int status, std::vector<uint8_t> bytes)
    {
        --inFlight_;
        auto it = entries_.find(packed);
        if (it == entries_.end())
        {
            return;
        }
        Entry& e = it->second;

        if (ok)
        {
            e.bytes = std::move(bytes);
            e.state = State::Downloaded;
        }
        else if (status == 404 || ++e.retries > MAX_RETRIES)
        {
            e.state = State::Empty;
        }
        else
        {
            e.state = State::Failed;
            e.retryAt = nowMs() + 1000.0 * double(1u << (e.retries - 1));
        }
    }

    void uploadDownloaded(double budgetMs)
    {
        std::vector<uint32_t> ready;
        for (auto& [k, e] : entries_)
        {
            if (e.state == State::Downloaded && e.requestedFrame == frame_)
            {
                ready.push_back(k);
            }
        }

        std::sort(
            ready.begin(),
            ready.end(),
            [this](uint32_t a, uint32_t b)
            {
                return entries_.at(a).priority < entries_.at(b).priority;
            }
        );

        const double t0 = nowMs();
        bool first = true;
        for (uint32_t k : ready)
        {
            if (!first && nowMs() - t0 > budgetMs)
            {
                break;
            }
            first = false;

            auto slot = acquireSlot();
            if (!slot)
            {
                break;
            }
            
            Entry& e = entries_.at(k);
            int w=0, h=0, ch=0;
            stbi_uc* px = stbi_load_from_memory(e.bytes.data(), int(e.bytes.size()), &w, &h, &ch, 4);

            if (!px || uint32_t(w) != texSize_ || uint32_t(h) != texSize_)
            {
                const TileKey key = TileKey::unpack(k);
                std::cerr << "[TileCache] bad tile " << key.z << "/" << key.x << "/" << key.y << (px ? " (size " + std::to_string(w) + "x" + std::to_string(h) + ")" : " (decode)") << "\n";

                if (px) stbi_image_free(px);
                e.state = State::Empty;
                e.bytes = {};
                freeSlots_.push_back(*slot);
                continue;
            }

            gpuUtils::writeTextureLayer(device_, texture_, *slot, px, uint32_t(w), uint32_t(h));
            stbi_image_free(px);

            e.slot = *slot;
            e.state = State::Ready;
            e.bytes = {};
        }
    }


    std::optional<uint32_t> acquireSlot()
    {
        if (!freeSlots_.empty())
        {
            uint32_t s = freeSlots_.back();
            freeSlots_.pop_back();
            return s;
        }

        auto victim = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            const Entry& e = it->second;
            if (e.state != State::Ready || e.lastUsed >= frame_)
            {
                continue;
            }

            if (victim == entries_.end() || e.lastUsed < victim->second.lastUsed)
            {
                victim = it;
            }
        }

        if (victim == entries_.end())
        {
            return std::nullopt;
        }

        uint32_t s = victim->second.slot;
        entries_.erase(victim);
        return s;
    }

    wgpu::Device device_;
    wgpu::Texture texture_;
    uint32_t texSize_;
    FetchFn fetch_;
    uint32_t maxInFlight_;

    std::unordered_map<uint32_t, Entry> entries_;
    std::vector<uint32_t> freeSlots_;
    uint32_t inFlight_ = 0;
    uint64_t frame_ = 0;
};