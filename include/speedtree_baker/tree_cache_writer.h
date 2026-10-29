#pragma once

#include <stddef.h>
#include <stdint.h>

#include <SpeedTreeLib/TreeCacheFormat.h>

namespace stbaker
{

using WriteCallback = bool (*)(void* pContext, const void* pData, size_t nSize);

struct CollisionObject
{
    int32_t type;
    float position[3];
    float dimensions[3];
};

// Mirrors the subset of SpeedTreeRT::SGeometry consumed by the cache.
struct IndexedGeometry
{
    uint16_t vertexCount;
    uint16_t stripCount;
    int16_t lodLevel;
    float alphaTest;
    const uint16_t* stripLengths;
    const uint16_t* const* strips;
    const float* coords;      // vertexCount * 3
    const uint32_t* colors;   // vertexCount
    const float* texCoords0;  // vertexCount * 2
    const float* texCoords1;  // vertexCount * 2
    const float* normals;     // vertexCount * 3
};

struct LeafSet
{
    bool active;
    int32_t lodLevel;
    float alphaTest;
    uint32_t leafCount;
    const float* centers;             // leafCount * 3
    const uint32_t* colors;           // leafCount
    const float* const* corners;      // leafCount * 16
    const float* const* texCoords;    // leafCount * 8
    const uint8_t* mapIndices;        // leafCount
    const uint8_t* clusterIndices;    // leafCount
};

struct Billboard
{
    bool active;
    float alphaTest;
    const float* coords;     // 12 (4 corners x xyz+w)
    const float* texCoords;  // 8  (4 corners x uv)
};

struct TextureNames
{
    const char* branch;
    const char* composite;
    const char* selfShadow;
};

// Serializes TreeCacheFormat.h. The writer is freestanding: it only needs a
// WriteCallback, which makes the format easy to test on the host too.
class TreeCacheWriter
{
public:
    TreeCacheWriter(WriteCallback pCallback, void* pContext);

    bool Ok() const { return m_bOk; }

    void WriteHeader(uint32_t nHash, const float bbox[6],
                     const CollisionObject* pCollisions, uint32_t nCollisionCount);
    void WriteIndexed(const IndexedGeometry& geometry);
    void WriteLeafSet(const LeafSet& leafSet);
    void WriteBillboard(const Billboard& billboard);
    void WriteTextureNames(const TextureNames& names);

private:
    void Raw(const void* pData, size_t nSize);
    void U8(uint8_t value);
    void U16(uint16_t value);
    void U32(uint32_t value);
    void I32(int32_t value);
    void F32(float value);
    void Floats(const float* pValues, size_t nCount);
    void Colors(const uint32_t* pValues, size_t nCount);
    void U16s(const uint16_t* pValues, size_t nCount);
    void U8s(const uint8_t* pValues, size_t nCount);
    void String(const char* text);

    WriteCallback m_pCallback;
    void* m_pContext;
    bool m_bOk;
};

} // namespace stbaker
