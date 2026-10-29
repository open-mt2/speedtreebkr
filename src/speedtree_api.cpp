#include "speedtree_baker/speedtree_api.h"

#include "speedtree_baker/platform.h"

extern "C"
{
__declspec(dllimport) void* __stdcall LoadLibraryA(const char* pFileName);
__declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* pProcName);
}

namespace stbaker
{

template <typename T>
T SpeedTreeApi::Resolve(const char* pName)
{
    if (!m_pDll)
        return nullptr;
    return reinterpret_cast<T>(GetProcAddress(m_pDll, pName));
}

bool SpeedTreeApi::Load(const char* pDllPath)
{
    m_pDll = LoadLibraryA(pDllPath);
    if (!m_pDll)
    {
        ErrorLine("error: cannot load SpeedTreeRT library");
        ErrorLine(pDllPath ? pDllPath : "(null)");
        return false;
    }

    m_pfnCtor = Resolve<PFN_Ctor>("??0CSpeedTreeRT@@QAE@XZ");
    m_pfnDtor = Resolve<PFN_Dtor>("??1CSpeedTreeRT@@QAE@XZ");
    m_pfnLoadTree = Resolve<PFN_LoadTree>("?LoadTree@CSpeedTreeRT@@QAE_NPBEI@Z");
    m_pfnCompute = Resolve<PFN_Compute>("?Compute@CSpeedTreeRT@@QAE_NPBMI_N@Z");
    m_pfnGetGeometry = Resolve<PFN_GetGeometry>("?GetGeometry@CSpeedTreeRT@@QAEXAAUSGeometry@1@KFFF@Z");
    m_pfnGetBBox = Resolve<PFN_GetBBox>("?GetBoundingBox@CSpeedTreeRT@@QBEXPAM@Z");
    m_pfnGetTextures = Resolve<PFN_GetTextures>("?GetTextures@CSpeedTreeRT@@QBEXAAUSTextures@1@@Z");
    m_pfnGetCollisionObjectCount = Resolve<PFN_GetCollisionObjectCount>("?GetCollisionObjectCount@CSpeedTreeRT@@QAEIXZ");
    m_pfnGetCollisionObject = Resolve<PFN_GetCollisionObject>("?GetCollisionObject@CSpeedTreeRT@@QAEXIAAW4ECollisionObjectType@1@PAM1@Z");
    m_pfnSetTreeSize = Resolve<PFN_SetTreeSize>("?SetTreeSize@CSpeedTreeRT@@QAEXMM@Z");
    m_pfnSetTreePosition = Resolve<PFN_SetTreePosition>("?SetTreePosition@CSpeedTreeRT@@QAEXMMM@Z");
    m_pfnIsAuthorized = Resolve<PFN_IsAuthorized>("?IsAuthorized@CSpeedTreeRT@@SA_NXZ");

    struct RequiredExport
    {
        const void* pointer;
        const char* name;
    };
    const RequiredExport required[] = {
        {reinterpret_cast<const void*>(m_pfnCtor), "??0CSpeedTreeRT"},
        {reinterpret_cast<const void*>(m_pfnDtor), "??1CSpeedTreeRT"},
        {reinterpret_cast<const void*>(m_pfnLoadTree), "LoadTree"},
        {reinterpret_cast<const void*>(m_pfnCompute), "Compute"},
        {reinterpret_cast<const void*>(m_pfnGetGeometry), "GetGeometry"},
    };

    for (const RequiredExport& entry : required)
    {
        if (!entry.pointer)
        {
            ErrorLine("error: SpeedTreeRT is missing a required export:");
            ErrorLine(entry.name);
            return false;
        }
    }

    // The SDK accepts an empty key in trial mode; the result is reported in
    // verbose mode for diagnostics but does not abort the bake.
    PFN_Authorize pfnAuthorize = Resolve<PFN_Authorize>("?Authorize@CSpeedTreeRT@@SAXPBD@Z");
    if (pfnAuthorize)
        pfnAuthorize("");

    // SpeedTree computes camera-facing leaf billboard corners inside
    // GetGeometry(). Without SetCamera the camera sits at the tree origin and
    // the cards get degenerate orientations/sizes.
    PFN_SetCamera pfnSetCamera = Resolve<PFN_SetCamera>("?SetCamera@CSpeedTreeRT@@SAXPBM0@Z");
    if (pfnSetCamera)
    {
        const float cameraEye[3] = {0.0f, -10000.0f, 1000.0f};
        const float cameraDir[3] = {0.0f, 1.0f, 0.0f};
        pfnSetCamera(cameraEye, cameraDir);
    }

    // The client wrapper enables T-flip for Direct3D (WRAPPER_FLIP_T_TEXCOORD).
    PFN_SetTextureFlip pfnSetTextureFlip = Resolve<PFN_SetTextureFlip>("?SetTextureFlip@CSpeedTreeRT@@SAX_N@Z");
    if (pfnSetTextureFlip)
        pfnSetTextureFlip(true);

    m_bAuthorized = m_pfnIsAuthorized ? m_pfnIsAuthorized() : false;
    if (IsVerbose())
    {
        LogValue("authorized: ", m_bAuthorized ? 1 : 0);
        LogValue("getBoundingBox: ", m_pfnGetBBox ? 1 : 0);
        LogValue("getTextures: ", m_pfnGetTextures ? 1 : 0);
        LogValue("getCollisionObjectCount: ", m_pfnGetCollisionObjectCount ? 1 : 0);
        LogValue("getCollisionObject: ", m_pfnGetCollisionObject ? 1 : 0);
    }
    return true;
}

} // namespace stbaker
