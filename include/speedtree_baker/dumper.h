#pragma once

#include <stddef.h>
#include <stdint.h>

#include "speedtree_api.h"

namespace stbaker
{

struct BakeOptions
{
    int seed = 1;
};

struct BakeStats
{
    uint32_t hash = 0;
    uint32_t branchVertices = 0;
    uint32_t frondVertices = 0;
    uint32_t leafCards = 0;
    uint32_t collisionObjects = 0;
};

enum class BakeStatus
{
    Ok,
    InvalidSpt,
    OutOfMemory,
    LoadFailed,
    ComputeFailed,
    WriteFailed
};

const char* BakeStatusName(BakeStatus status);

// Reuses the SpeedTreeRT object/geometry scratch buffers across trees.
class Dumper
{
public:
    explicit Dumper(const SpeedTreeApi& api);
    ~Dumper();
    Dumper(const Dumper&) = delete;
    Dumper& operator=(const Dumper&) = delete;

    bool IsReady() const;

    BakeStatus Bake(const unsigned char* pSpt, size_t nSptSize,
                    const char* outputPath, const BakeOptions& options,
                    BakeStats& stats);

private:
    static const size_t kObjectBufferSize = 8192;

    const SpeedTreeApi& m_api;
    unsigned char* m_pObject;
    st::SGeometry* m_pGeometry;
    st::STextures* m_pTextures;
};

} // namespace stbaker
