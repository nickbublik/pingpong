#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>
#include <openssl/sha.h>

namespace PingPong
{
namespace Common
{

inline std::array<uint8_t, SHA256_DIGEST_LENGTH> sha256_chunk(const void *data, size_t size)
{
    std::array<uint8_t, SHA256_DIGEST_LENGTH> hash{};

    EVP_MD_CTX *context = EVP_MD_CTX_new();

    if (!context)
        throw std::runtime_error("EVP_MD_CTX_new() failed");

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1)
    {
        EVP_MD_CTX_free(context);
        throw std::runtime_error("EVP_DigestInit_ex() failed");
    }

    if (size > 0)
    {
        if (EVP_DigestUpdate(context, data, size) != 1)
        {
            EVP_MD_CTX_free(context);
            throw std::runtime_error("EVP_DigestUpdate() failed");
        }
    }

    unsigned int out_length = 0;

    if (EVP_DigestFinal_ex(context, hash.data(), &out_length) != 1)
    {
        EVP_MD_CTX_free(context);
        throw std::runtime_error("EVP_DigestFinal_ex() failed");
    }

    EVP_MD_CTX_free(context);

    if (out_length != hash.size())
        throw std::runtime_error("Unexpexted hash size");

    return hash;
}

inline std::array<uint8_t, SHA256_DIGEST_LENGTH> sha256_chunk(const std::vector<uint8_t> &buf)
{
    return sha256_chunk(buf.data(), buf.size());
}

} // namespace Common
} // namespace PingPong
