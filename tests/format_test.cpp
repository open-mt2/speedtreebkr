// Host-side unit test for the freestanding tree cache writer.
//
// It serializes a synthetic tree, parses it back with a minimal reader, and
// checks every field. Run with tools/speedtree_baker/tests/run.sh.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "speedtree_baker/tree_cache_writer.h"

namespace
{

int g_failures = 0;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #condition);                                          \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

struct MemorySink
{
    unsigned char data[65536] = {};
    size_t size = 0;
    bool ok = true;
};

bool WriteToMemory(void* pContext, const void* pData, size_t nSize)
{
    MemorySink* pSink = static_cast<MemorySink*>(pContext);
    if (!pSink->ok || pSink->size + nSize > sizeof(pSink->data))
        return false;
    std::memcpy(pSink->data + pSink->size, pData, nSize);
    pSink->size += nSize;
    return true;
}

struct Reader
{
    const unsigned char* pData;
    size_t nSize;
    size_t nPosition = 0;
    bool ok = true;

    template <typename T>
    T Read()
    {
        T value = {};
        if (nPosition + sizeof(T) > nSize)
        {
            ok = false;
            return value;
        }
        std::memcpy(&value, pData + nPosition, sizeof(T));
        nPosition += sizeof(T);
        return value;
    }

    uint8_t U8() { return Read<uint8_t>(); }
    uint16_t U16() { return Read<uint16_t>(); }
    uint32_t U32() { return Read<uint32_t>(); }
    int32_t I32() { return Read<int32_t>(); }
    float F32() { return Read<float>(); }
};

bool NearlyEqual(float a, float b)
{
    return std::fabs(a - b) < 1.0e-6f;
}

} // namespace

int main()
{
    using namespace stbaker;

    char hashName[TreeCache::kHashHexDigits + 1];
    TreeCache::FormatHash(0x7FCB5F45u, hashName);
    CHECK(std::strcmp(hashName, "7fcb5f45") == 0);
    CHECK(TreeCache::Fnv1a32(reinterpret_cast<const unsigned char*>("hello"), 5) == 0x4F9F2CABu);

    const uint32_t hash = 0x12345678u;
    const float bbox[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    const CollisionObject collisions[1] = {{2, {1.5f, 2.5f, 3.5f}, {4.5f, 5.5f, 6.5f}}};

    const uint16_t branchLengths[1] = {4};
    const uint16_t branchIndices[4] = {0, 1, 2, 0};
    const uint16_t* branchStrips[1] = {branchIndices};
    const float branchCoords[9] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    const uint32_t branchColors[3] = {0x11223344u, 0x55667788u, 0x99AABBCCu};
    const float branchTex0[6] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    const float branchTex1[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    const float branchNormals[9] = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    IndexedGeometry branches = {};
    branches.vertexCount = 3;
    branches.stripCount = 1;
    branches.lodLevel = -1;
    branches.alphaTest = 0.5f;
    branches.stripLengths = branchLengths;
    branches.strips = branchStrips;
    branches.coords = branchCoords;
    branches.colors = branchColors;
    branches.texCoords0 = branchTex0;
    branches.texCoords1 = branchTex1;
    branches.normals = branchNormals;

    IndexedGeometry fronds = {};
    fronds.vertexCount = 0;
    fronds.stripCount = 0;
    fronds.lodLevel = -1;
    fronds.alphaTest = 0.0f;

    float leafCenters[3] = {10.0f, 20.0f, 30.0f};
    uint32_t leafColors[1] = {0xAABBCCDDu};
    float leafCorners[16] = {1.0f, 2.0f, 3.0f, 0.0f, 4.0f, 5.0f, 6.0f, 0.0f,
                             7.0f, 8.0f, 9.0f, 0.0f, 10.0f, 11.0f, 12.0f, 0.0f};
    float leafTexCoords[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    const float* leafCornerPtrs[1] = {leafCorners};
    const float* leafTexCoordPtrs[1] = {leafTexCoords};
    uint8_t leafMap[1] = {7};
    uint8_t leafCluster[1] = {8};

    LeafSet leaves0 = {};
    leaves0.active = true;
    leaves0.lodLevel = -1;
    leaves0.alphaTest = 0.25f;
    leaves0.leafCount = 1;
    leaves0.centers = leafCenters;
    leaves0.colors = leafColors;
    leaves0.corners = leafCornerPtrs;
    leaves0.texCoords = leafTexCoordPtrs;
    leaves0.mapIndices = leafMap;
    leaves0.clusterIndices = leafCluster;

    // Inactive set with null arrays exercises the writer's zero/NaN defaults.
    LeafSet leaves1 = {};
    leaves1.active = false;
    leaves1.lodLevel = -1;
    leaves1.alphaTest = 0.0f;
    leaves1.leafCount = 1;
    leaves1.centers = leafCenters;

    float billboardCoords[12] = {1.0f, 2.0f, 3.0f, 0.0f, 4.0f, 5.0f, 6.0f, 0.0f,
                                 7.0f, 8.0f, 9.0f, 0.0f};
    float billboardTexCoords[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

    Billboard billboard0 = {};
    billboard0.active = true;
    billboard0.alphaTest = 0.75f;
    billboard0.coords = billboardCoords;
    billboard0.texCoords = billboardTexCoords;

    Billboard billboard1 = {};
    billboard1.active = false;

    TextureNames names = {"branch.tga", "comp.tga", nullptr};

    MemorySink sink;
    TreeCacheWriter writer(&WriteToMemory, &sink);
    CHECK(writer.Ok());

    writer.WriteHeader(hash, bbox, collisions, 1);
    writer.WriteIndexed(branches);
    writer.WriteIndexed(fronds);
    writer.WriteLeafSet(leaves0);
    writer.WriteLeafSet(leaves1);
    writer.WriteBillboard(billboard0);
    writer.WriteBillboard(billboard1);
    writer.WriteTextureNames(names);
    CHECK(writer.Ok());

    Reader reader{sink.data, sink.size};

    CHECK(reader.U32() == TreeCache::kMagic);
    CHECK(reader.U32() == hash);
    for (float value : bbox)
        CHECK(NearlyEqual(reader.F32(), value));
    CHECK(reader.U32() == 1);
    CHECK(reader.I32() == 2);
    for (float value : collisions[0].position)
        CHECK(NearlyEqual(reader.F32(), value));
    for (float value : collisions[0].dimensions)
        CHECK(NearlyEqual(reader.F32(), value));

    // Branches.
    CHECK(reader.U32() == 3);
    CHECK(reader.U32() == 1);
    CHECK(reader.U16() == 0xFFFFu);
    CHECK(NearlyEqual(reader.F32(), 0.5f));
    CHECK(reader.U32() == 4);
    for (uint16_t value : branchIndices)
        CHECK(reader.U16() == value);
    for (float value : branchCoords)
        CHECK(NearlyEqual(reader.F32(), value));
    for (uint32_t value : branchColors)
        CHECK(reader.U32() == value);
    for (float value : branchTex0)
        CHECK(NearlyEqual(reader.F32(), value));
    for (float value : branchTex1)
        CHECK(NearlyEqual(reader.F32(), value));
    for (float value : branchNormals)
        CHECK(NearlyEqual(reader.F32(), value));

    // Fronds (empty).
    CHECK(reader.U32() == 0);
    CHECK(reader.U32() == 0);
    CHECK(reader.U16() == 0xFFFFu);
    CHECK(NearlyEqual(reader.F32(), 0.0f));
    CHECK(reader.U32() == 0);

    // Leaf set 0.
    CHECK(reader.U8() == 1);
    CHECK(reader.I32() == -1);
    CHECK(NearlyEqual(reader.F32(), 0.25f));
    CHECK(reader.U32() == 1);
    for (float value : leafCenters)
        CHECK(NearlyEqual(reader.F32(), value));
    CHECK(reader.U32() == 0xAABBCCDDu);
    for (float value : leafCorners)
        CHECK(NearlyEqual(reader.F32(), value));
    for (float value : leafTexCoords)
        CHECK(NearlyEqual(reader.F32(), value));
    CHECK(reader.U8() == 7);
    CHECK(reader.U8() == 8);

    // Leaf set 1: null colors must become kInvalidColor, null arrays zero.
    CHECK(reader.U8() == 0);
    CHECK(reader.I32() == -1);
    CHECK(NearlyEqual(reader.F32(), 0.0f));
    CHECK(reader.U32() == 1);
    for (float value : leafCenters)
        CHECK(NearlyEqual(reader.F32(), value));
    CHECK(reader.U32() == TreeCache::kInvalidColor);
    for (int i = 0; i < 16; ++i)
        CHECK(NearlyEqual(reader.F32(), 0.0f));
    for (int i = 0; i < 8; ++i)
        CHECK(NearlyEqual(reader.F32(), 0.0f));
    CHECK(reader.U8() == 0);
    CHECK(reader.U8() == 0);

    // Billboards.
    CHECK(reader.U8() == 1);
    CHECK(NearlyEqual(reader.F32(), 0.75f));
    for (float value : billboardCoords)
        CHECK(NearlyEqual(reader.F32(), value));
    for (float value : billboardTexCoords)
        CHECK(NearlyEqual(reader.F32(), value));

    CHECK(reader.U8() == 0);
    CHECK(NearlyEqual(reader.F32(), 0.0f));
    for (int i = 0; i < 12; ++i)
        CHECK(NearlyEqual(reader.F32(), 0.0f));
    for (int i = 0; i < 8; ++i)
        CHECK(NearlyEqual(reader.F32(), 0.0f));

    // Texture names.
    CHECK(reader.U32() == 10);
    CHECK(std::memcmp(sink.data + reader.nPosition, "branch.tga", 10) == 0);
    reader.nPosition += 10;
    CHECK(reader.U32() == 8);
    CHECK(std::memcmp(sink.data + reader.nPosition, "comp.tga", 8) == 0);
    reader.nPosition += 8;
    CHECK(reader.U32() == 0);

    CHECK(reader.ok);
    CHECK(reader.nPosition == sink.size);

    if (g_failures)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("format_test: all checks passed (%zu bytes)\n", sink.size);
    return 0;
}
