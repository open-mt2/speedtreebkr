// SpeedTree geometry dumper.
//
// Drives the original 32-bit SpeedTreeRT.dll under Wine (or on Windows) and
// writes one baked cache file per .spt listed in a text file. The .spt path
// list may contain comments (#) and blank lines.

#include <stddef.h>
#include <stdint.h>

#include <SpeedTreeLib/TreeCacheFormat.h>

#include "speedtree_baker/dumper.h"
#include "speedtree_baker/platform.h"
#include "speedtree_baker/speedtree_api.h"
#include "speedtree_baker/version.h"

using namespace stbaker;

extern "C"
{
__declspec(dllimport) const char* __stdcall GetCommandLineA();
__declspec(dllimport) void __stdcall ExitProcess(unsigned int nExitCode);
}

namespace
{

constexpr int kMaxArguments = 32;
constexpr int kMaxTreePaths = 100000;

struct Options
{
    char listPath[1024] = {0};
    char outDir[1024] = {0};
    char dllPath[1024] = {0};
    int seed = 1;
    bool help = false;
    bool version = false;
    bool quiet = false;
    bool verbose = false;
};

void PrintUsage()
{
    ErrorLine("usage: speedtree_dumper.exe --list <listfile> --out <outdir> [options]");
    ErrorLine("");
    ErrorLine("Bake SpeedTreeRT geometry for every .spt path listed in <listfile>.");
    ErrorLine("Output files are named <fnv1a32>.tree in <outdir>.");
    ErrorLine("");
    ErrorLine("options:");
    ErrorLine("  -l, --list <file>   text file with one .spt path per line");
    ErrorLine("  -o, --out <dir>     directory receiving the .tree files");
    ErrorLine("  -d, --dll <file>    SpeedTreeRT.dll (default: SpeedTreeRT.dll)");
    ErrorLine("      --seed <n>      geometry seed passed to Compute (default: 1)");
    ErrorLine("  -q, --quiet         only report errors");
    ErrorLine("  -v, --verbose       log every SpeedTree stage");
    ErrorLine("  -V, --version       print version and exit");
    ErrorLine("  -h, --help          show this help");
    ErrorLine("");
    ErrorLine("positional form: speedtree_dumper.exe <listfile> <outdir>");
}

int ParseCommandLine(char* pCommandLine, char* pArguments[], int nMaxArguments)
{
    int nCount = 0;
    char* p = pCommandLine;

    while (*p)
    {
        while (*p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        if (nCount >= nMaxArguments)
            return -1;

        char* pOut = p;
        pArguments[nCount++] = pOut;
        bool bQuoted = false;
        while (*p)
        {
            const char c = *p;
            if (c == '"')
            {
                bQuoted = !bQuoted;
                ++p;
                continue;
            }
            if (!bQuoted && (c == ' ' || c == '\t'))
                break;
            *pOut++ = c;
            ++p;
        }

        const bool bAtSeparator = (*p == ' ' || *p == '\t');
        *pOut = '\0';
        if (bAtSeparator)
            ++p;
    }
    return nCount;
}

bool ParseSeed(const char* pText, int& seed)
{
    if (!pText || !pText[0])
        return false;

    long long value = 0;
    while (*pText)
    {
        if (*pText < '0' || *pText > '9')
            return false;
        value = value * 10 + (*pText - '0');
        if (value > 0x7FFFFFFF)
            return false;
        ++pText;
    }
    seed = static_cast<int>(value);
    return true;
}

bool CopyOption(char* pDestination, size_t nCapacity, const char* pValue)
{
    if (!pValue || StrLen(pValue) >= nCapacity)
    {
        ErrorLine("error: argument is too long");
        return false;
    }
    StrCopy(pDestination, nCapacity, pValue);
    return true;
}

bool ParseOptions(int argc, char* argv[], Options& options)
{
    StrCopy(options.dllPath, sizeof(options.dllPath), "SpeedTreeRT.dll");

    bool bHaveList = false;
    bool bHaveOut = false;

    for (int i = 1; i < argc; ++i)
    {
        const char* pArg = argv[i];
        if (StrEqual(pArg, "-h") || StrEqual(pArg, "--help"))
        {
            options.help = true;
        }
        else if (StrEqual(pArg, "-V") || StrEqual(pArg, "--version"))
        {
            options.version = true;
        }
        else if (StrEqual(pArg, "-q") || StrEqual(pArg, "--quiet"))
        {
            options.quiet = true;
        }
        else if (StrEqual(pArg, "-v") || StrEqual(pArg, "--verbose"))
        {
            options.verbose = true;
        }
        else if (StrEqual(pArg, "-l") || StrEqual(pArg, "--list"))
        {
            if (++i >= argc || !CopyOption(options.listPath, sizeof(options.listPath), argv[i]))
                return false;
            bHaveList = true;
        }
        else if (StrEqual(pArg, "-o") || StrEqual(pArg, "--out"))
        {
            if (++i >= argc || !CopyOption(options.outDir, sizeof(options.outDir), argv[i]))
                return false;
            bHaveOut = true;
        }
        else if (StrEqual(pArg, "-d") || StrEqual(pArg, "--dll"))
        {
            if (++i >= argc || !CopyOption(options.dllPath, sizeof(options.dllPath), argv[i]))
                return false;
        }
        else if (StrEqual(pArg, "--seed"))
        {
            if (++i >= argc || !ParseSeed(argv[i], options.seed))
                return false;
        }
        else if (pArg[0] == '-')
        {
            ErrorLine("error: unknown option");
            ErrorLine(pArg);
            return false;
        }
        else if (!bHaveList)
        {
            if (!CopyOption(options.listPath, sizeof(options.listPath), pArg))
                return false;
            bHaveList = true;
        }
        else if (!bHaveOut)
        {
            if (!CopyOption(options.outDir, sizeof(options.outDir), pArg))
                return false;
            bHaveOut = true;
        }
        else
        {
            ErrorLine("error: unexpected argument");
            ErrorLine(pArg);
            return false;
        }
    }

    if (options.quiet && options.verbose)
    {
        ErrorLine("error: --quiet and --verbose are mutually exclusive");
        return false;
    }
    return true;
}

bool MakeOutputPath(const char* pOutDir, uint32_t nHash, char* pOut, size_t nCapacity)
{
    char name[TreeCache::kHashHexDigits + 1];
    TreeCache::FormatHash(nHash, name);

    const size_t nDirLength = StrLen(pOutDir);
    if (nDirLength == 0)
        return false;

    const bool bHasSeparator = pOutDir[nDirLength - 1] == '\\' || pOutDir[nDirLength - 1] == '/';
    const size_t nNeeded = nDirLength + (bHasSeparator ? 0 : 1) + TreeCache::kHashHexDigits + 5 + 1;
    if (nNeeded > nCapacity)
        return false;

    StrCopy(pOut, nCapacity, pOutDir);
    if (!bHasSeparator)
        StrAppend(pOut, nCapacity, "\\");
    StrAppend(pOut, nCapacity, name);
    StrAppend(pOut, nCapacity, ".tree");
    return true;
}

} // namespace

extern "C" void __stdcall mainCRTStartup(void)
{
    static char commandLine[32768];
    static char pathBuffer[1024];
    static char outputPath[1200];

    StrCopy(commandLine, sizeof(commandLine), GetCommandLineA());

    char* argv[kMaxArguments] = {nullptr};
    const int argc = ParseCommandLine(commandLine, argv, kMaxArguments);
    if (argc < 0)
    {
        ErrorLine("error: too many arguments");
        ExitProcess(1);
    }

    static Options options;
    if (!ParseOptions(argc, argv, options))
    {
        PrintUsage();
        ExitProcess(1);
    }
    if (options.help)
    {
        PrintUsage();
        ExitProcess(0);
    }
    if (options.version)
    {
        Log("speedtree_dumper " SPEEDTREE_BAKER_VERSION " (tree cache format ");
        LogNumber(TreeCache::kVersion);
        LogLine(")");
        ExitProcess(0);
    }
    if (!options.listPath[0] || !options.outDir[0])
    {
        ErrorLine("error: both a list file and an output directory are required");
        PrintUsage();
        ExitProcess(1);
    }

    SetLogLevel(options.quiet ? LogLevel::Quiet
                              : (options.verbose ? LogLevel::Verbose : LogLevel::Normal));

    SpeedTreeApi api;
    if (!api.Load(options.dllPath))
        ExitProcess(4);

    Buffer listFile;
    if (!listFile.Read(options.listPath))
    {
        ErrorLine("error: cannot read list file");
        ErrorLine(options.listPath);
        ExitProcess(1);
    }
    if (IsVerbose())
    {
        Log("list: ");
        LogLine(options.listPath);
        LogValue("list bytes: ", static_cast<long long>(listFile.Size()));
        LogValue("arguments: ", argc);
    }

    Dumper dumper(api);
    if (!dumper.IsReady())
    {
        ErrorLine("error: cannot allocate SpeedTree scratch buffers");
        ExitProcess(5);
    }
    BakeOptions bakeOptions;
    bakeOptions.seed = options.seed;

    int nTotal = 0;
    int nBaked = 0;
    int nFailed = 0;
    size_t nPosition = 0;

    while (nPosition < listFile.Size())
    {
        const unsigned char* pData = listFile.Data();
        size_t nStart = nPosition;
        while (nPosition < listFile.Size() && pData[nPosition] != '\n' && pData[nPosition] != '\r')
            ++nPosition;

        size_t nEnd = nPosition;
        while (nPosition < listFile.Size() && (pData[nPosition] == '\n' || pData[nPosition] == '\r'))
            ++nPosition;

        while (nStart < nEnd && (pData[nStart] == ' ' || pData[nStart] == '\t'))
            ++nStart;
        while (nEnd > nStart && (pData[nEnd - 1] == ' ' || pData[nEnd - 1] == '\t'))
            --nEnd;

        if (nStart == nEnd || pData[nStart] == '#')
            continue;
        if (nEnd - nStart >= sizeof(pathBuffer))
        {
            ErrorLine("error: path is too long, skipping:");
            ErrorLine("(path truncated)");
            ++nTotal;
            ++nFailed;
            continue;
        }

        for (size_t i = 0; i < nEnd - nStart; ++i)
            pathBuffer[i] = static_cast<char>(pData[nStart + i]);
        pathBuffer[nEnd - nStart] = '\0';

        ++nTotal;
        if (nTotal > kMaxTreePaths)
        {
            ErrorLine("error: too many trees in list file");
            ++nFailed;
            break;
        }

        Buffer spt;
        if (!spt.Read(pathBuffer))
        {
            ErrorLine("failed: cannot read .spt");
            ErrorLine(pathBuffer);
            ++nFailed;
            continue;
        }

        const uint32_t nHash = TreeCache::Fnv1a32(spt.Data(), spt.Size());
        if (!MakeOutputPath(options.outDir, nHash, outputPath, sizeof(outputPath)))
        {
            ErrorLine("failed: cannot build output path for");
            ErrorLine(pathBuffer);
            ++nFailed;
            continue;
        }

        BakeStats stats;
        const BakeStatus status = dumper.Bake(spt.Data(), spt.Size(), outputPath,
                                              bakeOptions, stats);
        if (status != BakeStatus::Ok)
        {
            ErrorLine("failed:");
            ErrorLine(BakeStatusName(status));
            ErrorLine(pathBuffer);
            ++nFailed;
            continue;
        }

        ++nBaked;
        char hashName[TreeCache::kHashHexDigits + 1];
        TreeCache::FormatHash(stats.hash, hashName);

        Log("[");
        LogNumber(nTotal);
        Log("] ");
        Log(hashName);
        if (options.verbose)
        {
            Log("  branches=");
            LogNumber(stats.branchVertices);
            Log("  fronds=");
            LogNumber(stats.frondVertices);
            Log("  leaves=");
            LogNumber(stats.leafCards);
        }
        Log("  ");
        LogLine(pathBuffer);
    }

    if (nTotal == 0)
    {
        ErrorLine("error: list file contains no .spt paths");
        ExitProcess(2);
    }

    LogValue("baked: ", nBaked);
    LogValue("failed: ", nFailed);
    ExitProcess(nFailed == 0 ? 0 : 3);
}
