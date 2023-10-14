#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <map>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
    
    struct Bucket {
        std::map<Key, Value> values;
        std::mutex m;
    };
    
    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count = 100): buckets_(bucket_count) {
        bucket_count_ = bucket_count;
    }

    Access operator[](const Key& key) {
        uint64_t index = key % bucket_count_;
        return {std::lock_guard(buckets_[index].m), buckets_[index].values[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        Bucket result;
        for_each(buckets_.begin(), buckets_.end(),
        [&result](auto& bucket) {
            std::lock_guard b_guard(bucket.m);
            result.values.insert(bucket.values.begin(), bucket.values.end());
        });
        return result.values;
    }
    
    size_t Erase(const Key& key) {
        uint64_t index = key % bucket_count_;
        std::lock_guard guard(buckets_[index].m);
        return buckets_[index].values.erase(key);
    }

private:
    size_t bucket_count_;
    std::vector<Bucket> buckets_;
};