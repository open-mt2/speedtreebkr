#include "speedtree_baker/platform.h"

// The tool is linked without a CRT. Clang can still lower large initializer
// loops to memset/memcpy, so provide the small subset of libc it may emit.
extern "C" __attribute__((noinline, optnone)) void* memset(void* pDst, int value, size_t nSize)
{
    volatile unsigned char* p = static_cast<volatile unsigned char*>(pDst);
    for (size_t i = 0; i < nSize; ++i)
        p[i] = static_cast<unsigned char>(value);
    return pDst;
}

extern "C" __attribute__((noinline, optnone)) void* memcpy(void* pDst, const void* pSrc, size_t nSize)
{
    volatile unsigned char* pOut = static_cast<volatile unsigned char*>(pDst);
    const volatile unsigned char* pIn = static_cast<const volatile unsigned char*>(pSrc);
    for (size_t i = 0; i < nSize; ++i)
        pOut[i] = pIn[i];
    return pDst;
}

extern "C" __attribute__((noinline, optnone)) void* memmove(void* pDst, const void* pSrc, size_t nSize)
{
    unsigned char* pOut = static_cast<unsigned char*>(pDst);
    const unsigned char* pIn = static_cast<const unsigned char*>(pSrc);
    if (pOut < pIn)
    {
        for (size_t i = 0; i < nSize; ++i)
            pOut[i] = pIn[i];
    }
    else if (pOut > pIn)
    {
        for (size_t i = nSize; i > 0; --i)
            pOut[i - 1] = pIn[i - 1];
    }
    return pDst;
}

extern "C" __attribute__((noinline, optnone)) int memcmp(const void* pA, const void* pB, size_t nSize)
{
    const unsigned char* pLeft = static_cast<const unsigned char*>(pA);
    const unsigned char* pRight = static_cast<const unsigned char*>(pB);
    for (size_t i = 0; i < nSize; ++i)
    {
        if (pLeft[i] != pRight[i])
            return pLeft[i] < pRight[i] ? -1 : 1;
    }
    return 0;
}

extern "C" __attribute__((noinline, optnone)) size_t strlen(const char* pText)
{
    size_t n = 0;
    while (pText[n])
        ++n;
    return n;
}

// Minimal kernel32 surface used by the baker. The tool is linked with
// -nostdlib, so it must not call anything from the C runtime.
extern "C"
{
__declspec(dllimport) void* __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int __stdcall WriteFile(void* hFile, const void* pBuffer, unsigned long nBytes,
                                              unsigned long* pWritten, void* pOverlapped);
__declspec(dllimport) void* __stdcall CreateFileA(const char* pFileName, unsigned long nAccess,
                                                  unsigned long nShareMode, void* pSecurity,
                                                  unsigned long nCreation, unsigned long nFlags,
                                                  void* hTemplate);
__declspec(dllimport) int __stdcall ReadFile(void* hFile, void* pBuffer, unsigned long nBytes,
                                             unsigned long* pRead, void* pOverlapped);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
__declspec(dllimport) unsigned long __stdcall GetFileSize(void* hFile, unsigned long* pHigh);
__declspec(dllimport) void* __stdcall VirtualAlloc(void* pAddress, unsigned long nSize,
                                                   unsigned long nAllocationType, unsigned long nProtect);
__declspec(dllimport) int __stdcall VirtualFree(void* pAddress, unsigned long nSize, unsigned long nFreeType);
__declspec(dllimport) int __stdcall MoveFileExA(const char* pExisting, const char* pNew, unsigned long nFlags);
__declspec(dllimport) int __stdcall DeleteFileA(const char* pFileName);
}

namespace stbaker
{

namespace
{

constexpr unsigned long kGenericRead = 0x80000000u;
constexpr unsigned long kGenericWrite = 0x40000000u;
constexpr unsigned long kFileShareRead = 0x00000001u;
constexpr unsigned long kCreateAlways = 2u;
constexpr unsigned long kOpenExisting = 3u;
constexpr unsigned long kFileAttributeNormal = 0x00000080u;
constexpr unsigned long kMemCommit = 0x00001000u;
constexpr unsigned long kMemReserve = 0x00002000u;
constexpr unsigned long kMemRelease = 0x00008000u;
constexpr unsigned long kPageReadWrite = 0x04u;
constexpr unsigned long kMoveFileReplaceExisting = 0x1u;
constexpr unsigned long kStdOutputHandle = static_cast<unsigned long>(-11);

bool IsInvalidHandle(void* pHandle)
{
    return !pHandle || pHandle == reinterpret_cast<void*>(static_cast<intptr_t>(-1));
}

LogLevel g_logLevel = LogLevel::Normal;

void WriteStdout(const char* pData, size_t nSize)
{
    if (!pData || nSize == 0)
        return;
    void* hStdout = GetStdHandle(kStdOutputHandle);
    if (IsInvalidHandle(hStdout))
        return;
    unsigned long nWritten = 0;
    WriteFile(hStdout, pData, static_cast<unsigned long>(nSize), &nWritten, nullptr);
}

char* AppendUnsigned(char* pDst, unsigned long long value)
{
    char digits[24];
    int count = 0;
    do
    {
        digits[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0);

    while (count > 0)
        *pDst++ = digits[--count];
    return pDst;
}

void WriteLine(const char* pText, bool bForce)
{
    if (!bForce && g_logLevel == LogLevel::Quiet)
        return;
    if (!pText)
        pText = "(null)";
    WriteStdout(pText, StrLen(pText));
    WriteStdout("\n", 1);
}

} // namespace

void SetLogLevel(LogLevel level)
{
    g_logLevel = level;
}

void* AllocateZeroed(size_t nSize)
{
    if (nSize == 0)
        return nullptr;
    return VirtualAlloc(nullptr, static_cast<unsigned long>(nSize),
                        kMemCommit | kMemReserve, kPageReadWrite);
}

void FreeAllocated(void* pMemory)
{
    if (pMemory)
        VirtualFree(pMemory, 0, kMemRelease);
}

bool IsVerbose()
{
    return g_logLevel == LogLevel::Verbose;
}

void Log(const char* pText)
{
    if (g_logLevel == LogLevel::Quiet || !pText)
        return;
    WriteStdout(pText, StrLen(pText));
}

void LogLine(const char* pText)
{
    WriteLine(pText, false);
}

void ErrorLine(const char* pText)
{
    WriteLine(pText, true);
}

void LogNumber(long long value)
{
    if (g_logLevel == LogLevel::Quiet)
        return;

    char digits[24];
    char* pEnd = digits;
    if (value < 0)
    {
        *pEnd++ = '-';
        pEnd = AppendUnsigned(pEnd, 0ull - static_cast<unsigned long long>(value));
    }
    else
    {
        pEnd = AppendUnsigned(pEnd, static_cast<unsigned long long>(value));
    }
    *pEnd = '\0';
    WriteStdout(digits, StrLen(digits));
}

void LogValue(const char* pLabel, long long value)
{
    if (g_logLevel == LogLevel::Quiet)
        return;

    Log(pLabel ? pLabel : "");
    LogNumber(value);
    LogLine("");
}

// --- Buffer ------------------------------------------------------------------

Buffer::~Buffer()
{
    if (m_pData)
        VirtualFree(m_pData, 0, kMemRelease);
    m_pData = nullptr;
    m_nSize = 0;
}

bool Buffer::Read(const char* pPath)
{
    if (!pPath || !pPath[0])
        return false;

    if (m_pData)
    {
        VirtualFree(m_pData, 0, kMemRelease);
        m_pData = nullptr;
        m_nSize = 0;
    }

    void* hFile = CreateFileA(pPath, kGenericRead, kFileShareRead, nullptr,
                              kOpenExisting, kFileAttributeNormal, nullptr);
    if (IsInvalidHandle(hFile))
        return false;

    unsigned long nHigh = 0;
    const unsigned long nLow = GetFileSize(hFile, &nHigh);
    if (nLow == 0xFFFFFFFFu || nHigh != 0)
    {
        CloseHandle(hFile);
        return false;
    }

    if (nLow == 0)
    {
        CloseHandle(hFile);
        m_pData = nullptr;
        m_nSize = 0;
        return true;
    }

    void* pData = VirtualAlloc(nullptr, nLow, kMemCommit | kMemReserve, kPageReadWrite);
    if (!pData)
    {
        CloseHandle(hFile);
        return false;
    }

    unsigned long nTotalRead = 0;
    bool bOk = true;
    while (nTotalRead < nLow)
    {
        unsigned long nRead = 0;
        if (!ReadFile(hFile, static_cast<unsigned char*>(pData) + nTotalRead,
                      nLow - nTotalRead, &nRead, nullptr) ||
            nRead == 0)
        {
            bOk = false;
            break;
        }
        nTotalRead += nRead;
    }
    CloseHandle(hFile);
    if (!bOk)
    {
        VirtualFree(pData, 0, kMemRelease);
        return false;
    }

    m_pData = static_cast<unsigned char*>(pData);
    m_nSize = static_cast<size_t>(nTotalRead);
    return true;
}

// --- FileSink ----------------------------------------------------------------

FileSink::~FileSink()
{
    if (!m_bCommitted)
        Discard();
}

bool FileSink::Open(const char* pFinalPath)
{
    if (!pFinalPath || !pFinalPath[0])
        return false;
    if (StrLen(pFinalPath) + 5 >= sizeof(m_szTempPath))
        return false;

    StrCopy(m_szFinalPath, sizeof(m_szFinalPath), pFinalPath);
    StrCopy(m_szTempPath, sizeof(m_szTempPath), pFinalPath);
    StrAppend(m_szTempPath, sizeof(m_szTempPath), ".tmp");

    m_hFile = CreateFileA(m_szTempPath, kGenericWrite, 0, nullptr,
                          kCreateAlways, kFileAttributeNormal, nullptr);
    m_bOk = !IsInvalidHandle(m_hFile);
    m_bCommitted = false;
    if (!m_bOk)
        m_hFile = nullptr;
    return m_bOk;
}

bool FileSink::Write(const void* pData, size_t nSize)
{
    if (!m_bOk || !m_hFile)
        return false;
    if (nSize == 0)
        return true;

    unsigned long nWritten = 0;
    if (!WriteFile(m_hFile, pData, static_cast<unsigned long>(nSize), &nWritten, nullptr) ||
        nWritten != static_cast<unsigned long>(nSize))
    {
        m_bOk = false;
        return false;
    }
    return true;
}

bool FileSink::WriteThunk(void* pContext, const void* pData, size_t nSize)
{
    return static_cast<FileSink*>(pContext)->Write(pData, nSize);
}

void FileSink::CloseFile()
{
    if (!IsInvalidHandle(m_hFile))
        CloseHandle(m_hFile);
    m_hFile = nullptr;
}

void FileSink::Discard()
{
    CloseFile();
    if (m_szTempPath[0])
        DeleteFileA(m_szTempPath);
    m_bOk = false;
    m_bCommitted = false;
}

bool FileSink::Commit()
{
    if (!m_bOk || !m_hFile)
    {
        Discard();
        return false;
    }

    CloseFile();
    if (!MoveFileExA(m_szTempPath, m_szFinalPath, kMoveFileReplaceExisting))
    {
        Discard();
        return false;
    }

    m_bCommitted = true;
    m_bOk = true;
    return true;
}

// --- Strings -----------------------------------------------------------------

size_t StrLen(const char* pText)
{
    size_t n = 0;
    if (pText)
    {
        while (pText[n])
            ++n;
    }
    return n;
}

bool StrEqual(const char* pA, const char* pB)
{
    if (!pA || !pB)
        return pA == pB;
    while (*pA && *pA == *pB)
    {
        ++pA;
        ++pB;
    }
    return *pA == *pB;
}

bool StartsWith(const char* pText, const char* pPrefix)
{
    if (!pText || !pPrefix)
        return false;
    while (*pPrefix)
    {
        if (*pText++ != *pPrefix++)
            return false;
    }
    return true;
}

void StrCopy(char* pDst, size_t nCapacity, const char* pSrc)
{
    if (!pDst || nCapacity == 0)
        return;
    size_t i = 0;
    if (pSrc)
    {
        while (pSrc[i] && i + 1 < nCapacity)
        {
            pDst[i] = pSrc[i];
            ++i;
        }
    }
    pDst[i] = '\0';
}

void StrAppend(char* pDst, size_t nCapacity, const char* pSrc)
{
    if (!pDst || nCapacity == 0 || !pSrc)
        return;
    size_t nLength = StrLen(pDst);
    if (nLength >= nCapacity - 1)
        return;
    StrCopy(pDst + nLength, nCapacity - nLength, pSrc);
}

} // namespace stbaker
