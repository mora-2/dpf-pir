#pragma once

#include <vector>
#include <cstdint>
#include <x86intrin.h>

#include "alignment_allocator.h"
#include <string>
#include <cassert>

class hashdatastore
{
public:
    typedef __m256i hash_type;
    enum KeywordType
    {
        STRING,
        HASH
    };

    hashdatastore() = default;

    void reserve(size_t n) { data_.reserve(n); }
    void push_back(std::string keyword_str, std::string data_str, KeywordType keyword_type)
    {
        if (keyword_type == hashdatastore::KeywordType::STRING)
        {
            hash_type data = string2m256i(data_str);
            data_.push_back(data);
            size_t keyword = string2uint64(keyword_str);
            keyword_.push_back(keyword);
        }
        else if (keyword_type == hashdatastore::KeywordType::HASH)
        {
            hash_type data = string2m256i(data_str);
            data_.push_back(data);
            size_t HashValue = hashFunction_(keyword_str) & this->HASH_MASK; // 48 bits
            hashs_.push_back(HashValue);
        }
    }
    void push_back(const hash_type &data) { data_.push_back(data); }
    void push_back(hash_type &&data) { data_.push_back(data); }

    void dummy(size_t n) { data_.resize(n, _mm256_set_epi64x(1, 2, 3, 4)); }

    size_t size() const { return data_.size(); }

    hash_type answer_pir1(const std::vector<uint8_t> &indexing) const;
    hash_type answer_pir2(const std::vector<uint8_t> &indexing) const;
    hash_type answer_pir3(const std::vector<uint8_t> &indexing) const;
    hash_type answer_pir4(const std::vector<uint8_t> &indexing) const;
    hash_type answer_pir5(const std::vector<uint8_t> &indexing) const;
    hash_type answer_pir_idea_speed_comparison(const std::vector<uint8_t> &indexing) const;

private:
    hash_type string2m256i(std::string data_str)
    {
        assert(data_str.size() <= 32);
        size_t fin[4] = {};
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 8 && (i * 8 + j) < data_str.size(); j++)
            {
                fin[i] = fin[i] + static_cast<uint8_t>(data_str[i * 8 + j]) * (1ULL << 8 * j);
            }
        }
        return _mm256_set_epi64x(fin[0], fin[1], fin[2], fin[3]);
    }
    size_t string2uint64(std::string keyword_str)
    {
        size_t result = 0;
        for (int i = 0; i < 8 && i < keyword_str.size(); i++)
        {
            result |= static_cast<size_t>(keyword_str[i]) << (8 * i);
        }
        return result;
    }

public:
    std::vector<size_t> keyword_;
    std::vector<size_t> hashs_;
    uint64_t HASH_MASK = 0xFFFFFFFFFFFF;

private:
    std::vector<hash_type, AlignmentAllocator<hash_type, sizeof(hash_type)>> data_;
    std::hash<std::string> hashFunction_;
};
