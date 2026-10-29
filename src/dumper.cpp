#include "speedtree_baker/dumper.h"

#include "speedtree_baker/platform.h"
#include "speedtree_baker/tree_cache_writer.h"

namespace stbaker
{

namespace
{

constexpr uint32_t kMaxCollisionObjects = 32;

void ZeroBytes(void* pData, size_t nSize)
{
    unsigned char* pBytes = static_cast<unsigned char*>(pData);
    for (size_t i = 0; i < nSize; ++i)
        pBytes[i] = 0;
}

IndexedGeometry MakeIndexed(const st::SIndexed& source, float alphaTest)
{
    IndexedGeometry geometry = {};
    geometry.vertexCount = source.m_usVertexCount;
    geometry.stripCount = source.m_usNumStrips;
    geometry.lodLevel = static_cast<int16_t>(source.m_nDiscreteLodLevel);
    geometry.alphaTest = alphaTest;
    geometry.stripLengths = source.m_pStripLengths;
    geometry.strips = source.m_pStrips;
    geometry.coords = source.m_pCoords;
    geometry.colors = source.m_pColors;
    geometry.texCoords0 = source.m_pTexCoords0;
    geometry.texCoords1 = source.m_pTexCoords1;
    geometry.normals = source.m_pNormals;
    return geometry;
}

LeafSet MakeLeafSet(const st::SLeaf& source)
{
    LeafSet leafSet = {};
    leafSet.active = source.m_bIsActive;
    leafSet.lodLevel = source.m_nDiscreteLodLevel;
    leafSet.alphaTest = source.m_fAlphaTestValue;
    leafSet.leafCount = source.m_usLeafCount;
    leafSet.centers = source.m_pCenterCoords;
    leafSet.colors = source.m_pColors;
    leafSet.corners = source.m_pLeafMapCoords;
    leafSet.texCoords = source.m_pLeafMapTexCoords;
    leafSet.mapIndices = source.m_pLeafMapIndices;
    leafSet.clusterIndices = source.m_pLeafClusterIndices;
    return leafSet;
}

Billboard MakeBillboard(const st::SBillboard& source)
{
    Billboard billboard = {};
    billboard.active = source.m_bIsActive;
    billboard.alphaTest = source.m_fAlphaTestValue;
    billboard.coords = source.m_pCoords;
    billboard.texCoords = source.m_pTexCoords;
    return billboard;
}

} // namespace

const char* BakeStatusName(BakeStatus status)
{
    switch (status)
    {
    case BakeStatus::Ok: return "ok";
    case BakeStatus::InvalidSpt: return "invalid-spt";
    case BakeStatus::OutOfMemory: return "out-of-memory";
    case BakeStatus::LoadFailed: return "load-failed";
    case BakeStatus::ComputeFailed: return "compute-failed";
    case BakeStatus::WriteFailed: return "write-failed";
    }
    return "unknown";
}

Dumper::Dumper(const SpeedTreeApi& api)
    : m_api(api),
      m_pObject(static_cast<unsigned char*>(AllocateZeroed(kObjectBufferSize))),
      m_pGeometry(static_cast<st::SGeometry*>(AllocateZeroed(sizeof(st::SGeometry)))),
      m_pTextures(static_cast<st::STextures*>(AllocateZeroed(sizeof(st::STextures))))
{
}

Dumper::~Dumper()
{
    FreeAllocated(m_pObject);
    FreeAllocated(m_pGeometry);
    FreeAllocated(m_pTextures);
}

bool Dumper::IsReady() const
{
    return m_pObject && m_pGeometry && m_pTextures;
}

BakeStatus Dumper::Bake(const unsigned char* pSpt, size_t nSptSize,
                        const char* outputPath, const BakeOptions& options,
                        BakeStats& stats)
{
    stats = {};
    if (!pSpt || nSptSize == 0)
        return BakeStatus::InvalidSpt;
    if (!IsReady())
        return BakeStatus::OutOfMemory;

    ZeroBytes(m_pObject, kObjectBufferSize);
    ZeroBytes(m_pGeometry, sizeof(st::SGeometry));
    ZeroBytes(m_pTextures, sizeof(st::STextures));

    if (IsVerbose())
        LogLine("  constructing SpeedTreeRT object");
    m_api.m_pfnCtor(m_pObject);

    if (m_api.m_pfnSetTreePosition)
        m_api.m_pfnSetTreePosition(m_pObject, 0.0f, 0.0f, 0.0f);

    if (IsVerbose())
        LogLine("  loading .spt");
    if (!m_api.m_pfnLoadTree(m_pObject, pSpt, static_cast<unsigned int>(nSptSize)))
    {
        m_api.m_pfnDtor(m_pObject);
        return BakeStatus::LoadFailed;
    }

    if (IsVerbose())
    {
        Log("  computing geometry (seed ");
        LogNumber(options.seed);
        LogLine(")");
    }
    if (!m_api.m_pfnCompute(m_pObject, nullptr, static_cast<unsigned int>(options.seed), true))
    {
        m_api.m_pfnDtor(m_pObject);
        return BakeStatus::ComputeFailed;
    }

    m_api.m_pfnGetGeometry(m_pObject, m_pGeometry, TreeCache::kAllParts, -1, -1, -1);
    if (m_api.m_pfnGetTextures)
        m_api.m_pfnGetTextures(m_pObject, m_pTextures);

    float bbox[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (m_api.m_pfnGetBBox)
        m_api.m_pfnGetBBox(m_pObject, bbox);

    CollisionObject collisions[kMaxCollisionObjects] = {};
    uint32_t nCollisionCount = m_api.m_pfnGetCollisionObjectCount
                                   ? m_api.m_pfnGetCollisionObjectCount(m_pObject)
                                   : 0;
    if (nCollisionCount > kMaxCollisionObjects)
        nCollisionCount = kMaxCollisionObjects;

    if (m_api.m_pfnGetCollisionObject)
    {
        for (uint32_t i = 0; i < nCollisionCount; ++i)
        {
            m_api.m_pfnGetCollisionObject(m_pObject, i, &collisions[i].type,
                                          collisions[i].position, collisions[i].dimensions);
        }
    }

    const uint32_t nHash = TreeCache::Fnv1a32(pSpt, nSptSize);
    const IndexedGeometry branches = MakeIndexed(m_pGeometry->m_sBranches,
                                                 m_pGeometry->m_fBranchAlphaTestValue);
    const IndexedGeometry fronds = MakeIndexed(m_pGeometry->m_sFronds,
                                               m_pGeometry->m_fFrondAlphaTestValue);
    const LeafSet leaves0 = MakeLeafSet(m_pGeometry->m_sLeaves0);
    const LeafSet leaves1 = MakeLeafSet(m_pGeometry->m_sLeaves1);
    const Billboard billboard0 = MakeBillboard(m_pGeometry->m_sBillboard0);
    const Billboard billboard1 = MakeBillboard(m_pGeometry->m_sBillboard1);
    const TextureNames textureNames = {
        m_pTextures->m_pBranchTextureFilename,
        m_pTextures->m_pCompositeFilename,
        m_pTextures->m_pSelfShadowFilename};

    FileSink sink;
    if (!sink.Open(outputPath))
    {
        m_api.m_pfnDtor(m_pObject);
        return BakeStatus::WriteFailed;
    }

    TreeCacheWriter writer(&FileSink::WriteThunk, &sink);
    writer.WriteHeader(nHash, bbox, collisions, nCollisionCount);
    writer.WriteIndexed(branches);
    writer.WriteIndexed(fronds);
    writer.WriteLeafSet(leaves0);
    writer.WriteLeafSet(leaves1);
    writer.WriteBillboard(billboard0);
    writer.WriteBillboard(billboard1);
    writer.WriteTextureNames(textureNames);

    const bool bWritten = writer.Ok() && sink.Commit();
    m_api.m_pfnDtor(m_pObject);

    if (!bWritten)
        return BakeStatus::WriteFailed;

    stats.hash = nHash;
    stats.branchVertices = branches.vertexCount;
    stats.frondVertices = fronds.vertexCount;
    stats.leafCards = leaves0.active ? leaves0.leafCount : 0;
    stats.collisionObjects = nCollisionCount;
    return BakeStatus::Ok;
}

} // namespace stbaker
