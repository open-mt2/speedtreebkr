#pragma once

#include <stddef.h>
#include <stdint.h>

namespace stbaker
{

enum class LogLevel
{
    Quiet,
    Normal,
    Verbose
};

void SetLogLevel(LogLevel level);
bool IsVerbose();

// Zero-filled, page-aligned allocation. Used for large scratch blocks so the
// CLI's stack frame stays below the Windows stack-probe threshold.
void* AllocateZeroed(size_t size);
void FreeAllocated(void* memory);

void Log(const char* text);
void LogLine(const char* text);
void ErrorLine(const char* text);
void LogNumber(long long value);
void LogValue(const char* label, long long value);

// Owns a block allocated with VirtualAlloc. Used for file contents so the tool
// is not limited to fixed-size scratch buffers.
class Buffer
{
public:
    Buffer() = default;
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool Read(const char* path);

    unsigned char* Data() const { return m_pData; }
    size_t Size() const { return m_nSize; }
    bool Empty() const { return m_nSize == 0; }

private:
    unsigned char* m_pData = nullptr;
    size_t m_nSize = 0;
};

// Writes to "<path>.tmp" and atomically renames on Commit(), so an interrupted
// or failed bake never leaves a truncated .tree file behind.
class FileSink
{
public:
    FileSink() = default;
    ~FileSink();
    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    bool Open(const char* finalPath);
    bool Commit();

    static bool WriteThunk(void* pContext, const void* pData, size_t nSize);

private:
    bool Write(const void* pData, size_t nSize);
    void CloseFile();
    void Discard();

    void* m_hFile = nullptr;
    bool m_bOk = false;
    bool m_bCommitted = false;
    char m_szFinalPath[1024] = {0};
    char m_szTempPath[1028] = {0};
};

size_t StrLen(const char* text);
bool StrEqual(const char* a, const char* b);
bool StartsWith(const char* text, const char* prefix);
void StrCopy(char* dst, size_t capacity, const char* src);
void StrAppend(char* dst, size_t capacity, const char* src);

} // namespace stbaker
