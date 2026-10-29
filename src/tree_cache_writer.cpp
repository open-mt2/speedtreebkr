#include "speedtree_baker/tree_cache_writer.h"

namespace stbaker
{

namespace
{

size_t TextLength(const char* pText)
{
    size_t n = 0;
    if (pText)
    {
        while (pText[n])
            ++n;
    }
    return n;
}

} // namespace

TreeCacheWriter::TreeCacheWriter(WriteCallback pCallback, void* pContext)
    : m_pCallback(pCallback),
      m_pContext(pContext),
      m_bOk(pCallback != nullptr)
{
}

void TreeCacheWriter::Raw(const void* pData, size_t nSize)
{
    if (!m_bOk || nSize == 0)
        return;
    m_bOk = m_pCallback(m_pContext, pData, nSize);
}

void TreeCacheWriter::U8(uint8_t value)
{
    Raw(&value, sizeof(value));
}

void TreeCacheWriter::U16(uint16_t value)
{
    Raw(&value, sizeof(value));
}

void TreeCacheWriter::U32(uint32_t value)
{
    Raw(&value, sizeof(value));
}

void TreeCacheWriter::I32(int32_t value)
{
    Raw(&value, sizeof(value));
}

void TreeCacheWriter::F32(float value)
{
    Raw(&value, sizeof(value));
}

void TreeCacheWriter::Floats(const float* pValues, size_t nCount)
{
    if (pValues)
    {
        Raw(pValues, nCount * sizeof(float));
        return;
    }
    const float zero = 0.0f;
    for (size_t i = 0; i < nCount && m_bOk; ++i)
        Raw(&zero, sizeof(zero));
}

void TreeCacheWriter::Colors(const uint32_t* pValues, size_t nCount)
{
    if (pValues)
    {
        Raw(pValues, nCount * sizeof(uint32_t));
        return;
    }
    for (size_t i = 0; i < nCount && m_bOk; ++i)
        U32(TreeCache::kInvalidColor);
}

void TreeCacheWriter::U16s(const uint16_t* pValues, size_t nCount)
{
    if (pValues)
    {
        Raw(pValues, nCount * sizeof(uint16_t));
        return;
    }
    for (size_t i = 0; i < nCount && m_bOk; ++i)
        U16(0);
}

void TreeCacheWriter::U8s(const uint8_t* pValues, size_t nCount)
{
    if (pValues)
    {
        Raw(pValues, nCount);
        return;
    }
    for (size_t i = 0; i < nCount && m_bOk; ++i)
        U8(0);
}

void TreeCacheWriter::String(const char* pText)
{
    const uint32_t nLength = static_cast<uint32_t>(TextLength(pText));
    U32(nLength);
    if (nLength)
        Raw(pText, nLength);
}

void TreeCacheWriter::WriteHeader(uint32_t nHash, const float bbox[6],
                                  const CollisionObject* pCollisions,
                                  uint32_t nCollisionCount)
{
    if (!pCollisions)
        nCollisionCount = 0;

    U32(TreeCache::kMagic);
    U32(nHash);
    Floats(bbox, 6);
    U32(nCollisionCount);

    for (uint32_t i = 0; i < nCollisionCount && m_bOk; ++i)
    {
        I32(pCollisions[i].type);
        Floats(pCollisions[i].position, 3);
        Floats(pCollisions[i].dimensions, 3);
    }
}

void TreeCacheWriter::WriteIndexed(const IndexedGeometry& geometry)
{
    U32(geometry.vertexCount);
    U32(geometry.stripCount);
    U16(static_cast<uint16_t>(geometry.lodLevel));
    F32(geometry.alphaTest);

    uint32_t nIndexCount = 0;
    if (geometry.stripLengths)
    {
        for (uint16_t s = 0; s < geometry.stripCount; ++s)
            nIndexCount += geometry.stripLengths[s];
    }
    U32(nIndexCount);

    for (uint16_t s = 0; s < geometry.stripCount && m_bOk; ++s)
    {
        const uint16_t nLength = geometry.stripLengths ? geometry.stripLengths[s] : 0;
        const uint16_t* pStrip = geometry.strips ? geometry.strips[s] : nullptr;
        U16s(pStrip, nLength);
    }

    Floats(geometry.coords, static_cast<size_t>(geometry.vertexCount) * 3);
    Colors(geometry.colors, geometry.vertexCount);
    Floats(geometry.texCoords0, static_cast<size_t>(geometry.vertexCount) * 2);
    Floats(geometry.texCoords1, static_cast<size_t>(geometry.vertexCount) * 2);
    Floats(geometry.normals, static_cast<size_t>(geometry.vertexCount) * 3);
}

void TreeCacheWriter::WriteLeafSet(const LeafSet& leafSet)
{
    U8(leafSet.active ? 1 : 0);
    I32(leafSet.lodLevel);
    F32(leafSet.alphaTest);
    U32(leafSet.leafCount);

    Floats(leafSet.centers, static_cast<size_t>(leafSet.leafCount) * 3);
    Colors(leafSet.colors, leafSet.leafCount);

    for (uint32_t i = 0; i < leafSet.leafCount && m_bOk; ++i)
    {
        Floats(leafSet.corners ? leafSet.corners[i] : nullptr, 16);
        Floats(leafSet.texCoords ? leafSet.texCoords[i] : nullptr, 8);
        U8(leafSet.mapIndices ? leafSet.mapIndices[i] : 0);
        U8(leafSet.clusterIndices ? leafSet.clusterIndices[i] : 0);
    }
}

void TreeCacheWriter::WriteBillboard(const Billboard& billboard)
{
    U8(billboard.active ? 1 : 0);
    F32(billboard.alphaTest);
    Floats(billboard.coords, 12);
    Floats(billboard.texCoords, 8);
}

void TreeCacheWriter::WriteTextureNames(const TextureNames& names)
{
    String(names.branch);
    String(names.composite);
    String(names.selfShadow);
}

} // namespace stbaker
