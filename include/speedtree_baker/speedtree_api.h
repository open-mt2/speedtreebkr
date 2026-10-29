#pragma once

#include <stdint.h>

namespace stbaker
{

// --- SpeedTreeRT 1.6 public structures (from the original SDK header) --------
// Field types use fixed-width integers; on the 32-bit Windows target they match
// the SDK's `unsigned long` / `unsigned short` exactly.
namespace st
{

struct SIndexed
{
    int m_nDiscreteLodLevel;
    uint16_t m_usNumStrips;
    const uint16_t* m_pStripLengths;
    const uint16_t* const* m_pStrips;
    uint16_t m_usVertexCount;
    const uint32_t* m_pColors;
    const float* m_pNormals;
    const float* m_pBinormals;
    const float* m_pTangents;
    const float* m_pCoords;
    const float* m_pTexCoords0;
    const float* m_pTexCoords1;
    const float* m_pWindWeights;
    const unsigned char* m_pWindMatrixIndices;
};

struct SLeaf
{
    bool m_bIsActive;
    float m_fAlphaTestValue;
    int m_nDiscreteLodLevel;
    uint16_t m_usLeafCount;
    const unsigned char* m_pLeafMapIndices;
    const unsigned char* m_pLeafClusterIndices;
    const float* m_pCenterCoords;
    const float* const* m_pLeafMapTexCoords;
    const float* const* m_pLeafMapCoords;
    const uint32_t* m_pColors;
    const float* m_pNormals;
    const float* m_pBinormals;
    const float* m_pTangents;
    const float* m_pWindWeights;
    const unsigned char* m_pWindMatrixIndices;
};

struct SBillboard
{
    bool m_bIsActive;
    const float* m_pTexCoords;
    const float* m_pCoords;
    float m_fAlphaTestValue;
};

struct SGeometry
{
    SIndexed m_sBranches;
    float m_fBranchAlphaTestValue;
    SIndexed m_sFronds;
    float m_fFrondAlphaTestValue;
    SLeaf m_sLeaves0;
    SLeaf m_sLeaves1;
    SBillboard m_sBillboard0;
    SBillboard m_sBillboard1;
    SBillboard m_sHorizontalBillboard;
};

struct STextures
{
    const char* m_pBranchTextureFilename;
    uint32_t m_uiLeafTextureCount;
    const char* const* m_pLeafTextureFilenames;
    uint32_t m_uiFrondTextureCount;
    const char* const* m_pFrondTextureFilenames;
    const char* m_pCompositeFilename;
    const char* m_pSelfShadowFilename;
};

} // namespace st

// --- Dynamically resolved SpeedTreeRT.dll exports ---------------------------
struct SpeedTreeApi
{
    typedef void(__attribute__((thiscall)) * PFN_Ctor)(void*);
    typedef void(__attribute__((thiscall)) * PFN_Dtor)(void*);
    typedef int(__attribute__((thiscall)) * PFN_LoadTree)(void*, const unsigned char*, unsigned int);
    typedef int(__attribute__((thiscall)) * PFN_Compute)(void*, const float*, unsigned int, bool);
    typedef void(__attribute__((thiscall)) * PFN_GetGeometry)(void*, st::SGeometry*, unsigned long, short, short, short);
    typedef void(__attribute__((thiscall)) * PFN_GetBBox)(const void*, float*);
    typedef void(__attribute__((thiscall)) * PFN_GetTextures)(const void*, st::STextures*);
    typedef unsigned int(__attribute__((thiscall)) * PFN_GetCollisionObjectCount)(void*);
    typedef void(__attribute__((thiscall)) * PFN_GetCollisionObject)(void*, unsigned int, int*, float*, float*);
    typedef void(__attribute__((thiscall)) * PFN_SetTreeSize)(void*, float, float);
    typedef void(__attribute__((thiscall)) * PFN_SetTreePosition)(void*, float, float, float);

    typedef void(__cdecl* PFN_Authorize)(const char*);
    typedef void(__cdecl* PFN_SetCamera)(const float*, const float*);
    typedef void(__cdecl* PFN_SetTextureFlip)(bool);
    typedef bool(__cdecl* PFN_IsAuthorized)();
    void* m_pDll = nullptr;
    PFN_Ctor m_pfnCtor = nullptr;
    PFN_Dtor m_pfnDtor = nullptr;
    PFN_LoadTree m_pfnLoadTree = nullptr;
    PFN_Compute m_pfnCompute = nullptr;
    PFN_GetGeometry m_pfnGetGeometry = nullptr;
    PFN_GetBBox m_pfnGetBBox = nullptr;
    PFN_GetTextures m_pfnGetTextures = nullptr;
    PFN_GetCollisionObjectCount m_pfnGetCollisionObjectCount = nullptr;
    PFN_GetCollisionObject m_pfnGetCollisionObject = nullptr;
    PFN_SetTreeSize m_pfnSetTreeSize = nullptr;
    PFN_SetTreePosition m_pfnSetTreePosition = nullptr;
    PFN_IsAuthorized m_pfnIsAuthorized = nullptr;
    bool m_bAuthorized = false;

    // Loads the DLL, resolves the ABI, and applies the global configuration
    // (authorization, camera, texture flip) expected by the client wrapper.
    bool Load(const char* dllPath);

private:
    template <typename T>
    T Resolve(const char* name);
};

} // namespace stbaker
