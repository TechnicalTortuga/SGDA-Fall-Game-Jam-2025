#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include "../utils/Logger.h"

/**
 * @brief Generic Cache System - Flyweight Pattern Implementation
 * 
 * This system provides generic caching functionality with deduplication,
 * reference counting, and automatic cleanup for any type of cached object.
 * 
 * Template Parameters:
 * - TKey: The key type for lookups (must be hashable)
 * - TData: The data type being cached
 * - TProperties: The properties type used to create data
 */
template<typename TKey, typename TData, typename TProperties>
class CacheSystem {
public:
    using DataPtr = std::unique_ptr<TData>;
    using KeyGenerator = std::function<TKey(const TProperties&)>;
    using DataFactory = std::function<DataPtr(const TProperties&)>;

    struct CacheStats {
        size_t totalItems = 0;
        size_t totalRequests = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t memoryUsed = 0;
        size_t cleanupRuns = 0;
        size_t itemsRemoved = 0;

        float GetHitRate() const {
            return totalRequests > 0 ? (float)cacheHits / totalRequests : 0.0f;
        }
    };

    /**
     * @brief Constructor
     * @param keyGen Function to generate cache keys from properties
     * @param dataFactory Function to create new data from properties
     * @param name Cache name for logging
     */
    CacheSystem(KeyGenerator keyGen, DataFactory dataFactory, const std::string& name = "Cache")
        : keyGenerator_(keyGen)
        , dataFactory_(dataFactory)
        , cacheName_(name)
        , nextId_(1) {
        LOG_DEBUG("Created " + cacheName_ + " cache system");
    }

    ~CacheSystem() {
        Clear();
        LOG_DEBUG("Destroyed " + cacheName_ + " cache system");
    }

    /**
     * @brief Get or create cached item (main interface)
     * @param properties Properties to match/create
     * @return Item ID for accessing the cached data
     */
    uint32_t GetOrCreate(const TProperties& properties) {
        stats_.totalRequests++;

        // Generate lookup key
        TKey key = keyGenerator_(properties);

        // Check if item already exists
        auto it = lookupMap_.find(key);
        if (it != lookupMap_.end()) {
            // Cache hit - increment reference count
            uint32_t id = it->second;
            AddReference(id);
            stats_.cacheHits++;
            LOG_DEBUG(cacheName_ + " cache HIT for ID " + std::to_string(id));
            return id;
        }

        // Cache miss - create new item
        stats_.cacheMisses++;
        uint32_t newId = CreateNewItem(properties, key);
        LOG_DEBUG(cacheName_ + " cache MISS - created new item ID " + std::to_string(newId));
        return newId;
    }

    /**
     * @brief Get cached data by ID
     * @param id Item ID
     * @return Pointer to data, or nullptr if invalid
     */
    const TData* Get(uint32_t id) const {
        if (id == 0 || id >= data_.size() || !data_[id]) {
            return nullptr;
        }
        return data_[id].get();
    }

    /**
     * @brief Check if ID is valid
     * @param id Item ID to check
     * @return true if valid, false otherwise
     */
    bool IsValid(uint32_t id) const {
        return id > 0 && id < data_.size() && data_[id] != nullptr;
    }

    /**
     * @brief Add reference to cached item
     * @param id Item ID
     */
    void AddReference(uint32_t id) {
        if (IsValid(id)) {
            refCounts_[id]++;
        }
    }

    /**
     * @brief Remove reference from cached item
     * @param id Item ID
     * @return true if item was removed (ref count reached 0)
     */
    bool RemoveReference(uint32_t id) {
        if (!IsValid(id)) return false;

        if (refCounts_[id] > 0) {
            refCounts_[id]--;
        }

        // If reference count reaches 0, mark for cleanup
        if (refCounts_[id] == 0) {
            LOG_DEBUG(cacheName_ + " item ID " + std::to_string(id) + " marked for cleanup (ref count = 0)");
            return true;
        }

        return false;
    }

    /**
     * @brief Get reference count for item
     * @param id Item ID
     * @return Reference count
     */
    uint32_t GetRefCount(uint32_t id) const {
        return IsValid(id) ? refCounts_[id] : 0;
    }

    /**
     * @brief Clean up unused items (ref count = 0)
     * @return Number of items removed
     */
    size_t CleanupUnused() {
        size_t removed = 0;
        stats_.cleanupRuns++;

        for (uint32_t id = 1; id < data_.size(); ++id) {
            if (data_[id] && refCounts_[id] == 0) {
                // Remove from lookup map
                for (auto it = lookupMap_.begin(); it != lookupMap_.end(); ++it) {
                    if (it->second == id) {
                        lookupMap_.erase(it);
                        break;
                    }
                }

                // Clear data
                data_[id].reset();
                removed++;
            }
        }

        if (removed > 0) {
            stats_.itemsRemoved += removed;
            stats_.totalItems -= removed;
            LOG_DEBUG(cacheName_ + " cleanup: removed " + std::to_string(removed) + " unused items");
        }

        return removed;
    }

    /**
     * @brief Clear all cached items
     */
    void Clear() {
        data_.clear();
        refCounts_.clear();
        lookupMap_.clear();
        stats_.totalItems = 0;
        nextId_ = 1;
        LOG_INFO("Cleared " + cacheName_ + " cache");
    }

    /**
     * @brief Get cache statistics
     */
    const CacheStats& GetStats() const { return stats_; }

    /**
     * @brief Reset cache statistics
     */
    void ResetStats() {
        stats_ = CacheStats{};
        stats_.totalItems = data_.size();
    }

    /**
     * @brief Get total number of cached items
     */
    size_t Size() const { return stats_.totalItems; }

private:
    uint32_t CreateNewItem(const TProperties& properties, const TKey& key) {
        // Create new data using factory
        auto newData = dataFactory_(properties);
        if (!newData) {
            LOG_ERROR(cacheName_ + " factory failed to create data");
            return 0;
        }

        // Assign new ID
        uint32_t newId = nextId_++;

        // Ensure vectors are large enough
        if (newId >= data_.size()) {
            data_.resize(newId + 1);
            refCounts_.resize(newId + 1);
        }

        // Store data
        data_[newId] = std::move(newData);
        refCounts_[newId] = 1; // Start with 1 reference

        // Add to lookup map
        lookupMap_[key] = newId;

        stats_.totalItems++;
        return newId;
    }

    // Function objects for key generation and data creation
    KeyGenerator keyGenerator_;
    DataFactory dataFactory_;

    // Cache name for logging
    std::string cacheName_;

    // Data storage
    std::vector<DataPtr> data_;
    std::vector<uint32_t> refCounts_;
    std::unordered_map<TKey, uint32_t> lookupMap_;

    // ID generation
    uint32_t nextId_;

    // Statistics
    CacheStats stats_;
};
