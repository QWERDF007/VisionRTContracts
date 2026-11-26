#include "visionrt/contracts/ZipArchive.h"

#include "visionrt/contracts/Value.h"

#include <miniz.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace visionrt {
namespace {

void validateEntryName(std::string_view name)
{
    validateUtf8(name);
    if (name.empty())
        throw ContractError("ZIP 条目名称不能为空");
    if (name.front() == '/' || name.front() == '\\' || name.back() == '/')
        throw ContractError("ZIP 条目路径非法：" + std::string(name));
    if (name.find('\\') != std::string_view::npos || name.find(':') != std::string_view::npos)
        throw ContractError("ZIP 条目路径包含非法分隔符或盘符：" + std::string(name));
    std::size_t start = 0;
    while (start <= name.size())
    {
        const auto slash   = name.find('/', start);
        const auto end     = slash == std::string_view::npos ? name.size() : slash;
        const auto segment = name.substr(start, end - start);
        if (segment.empty() || segment == "." || segment == "..")
            throw ContractError("ZIP 条目路径存在越界或空段：" + std::string(name));
        if (slash == std::string_view::npos)
            break;
        start = slash + 1;
    }
}

std::filesystem::path temporarySiblingPath(const std::filesystem::path &target)
{
    static std::atomic<std::uint64_t> counter{0};
    const auto                        seq = counter.fetch_add(1, std::memory_order_relaxed);
#ifdef _WIN32
    const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    const auto pid = static_cast<std::uint64_t>(getpid());
#endif
    auto       name = target.filename().native();
    const auto tag  = ".tmp." + std::to_string(pid) + "." + std::to_string(seq);
    name.append(tag.begin(), tag.end());
    return target.parent_path() / name;
}

void writeAtomicBytes(const std::filesystem::path &target, const void *data, std::size_t size)
{
    const auto absolute = std::filesystem::absolute(target).lexically_normal();
    if (!absolute.parent_path().empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(absolute.parent_path(), ec);
        if (ec)
            throw ContractError("无法创建 ZIP 目标目录：" + ec.message());
    }
    const auto temp = temporarySiblingPath(absolute);
    const auto cleanup = [&temp]() noexcept
    {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
    };
    try
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw ContractError("无法创建临时 ZIP 文件：" + absolute.string());
        if (size > 0 && !stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size)))
            throw ContractError("写入临时 ZIP 文件失败：" + absolute.string());
        stream.flush();
        if (!stream.good())
            throw ContractError("刷新临时 ZIP 文件失败：" + absolute.string());
        stream.close();
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), absolute.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const auto err = GetLastError();
            throw ContractError("原子提交 ZIP 文件失败（Win32 错误 " + std::to_string(err) + "）");
        }
#else
        std::error_code ec;
        std::filesystem::rename(temp, absolute, ec);
        if (ec)
            throw ContractError("原子提交 ZIP 文件失败：" + ec.message());
#endif
    }
    catch (...)
    {
        cleanup();
        throw;
    }
}

struct ReaderGuard
{
    std::ifstream  stream;
    mz_zip_archive archive{};
    bool           open = false;

    static size_t readCallback(void *opaque, mz_uint64 offset, void *buffer, size_t count)
    {
        auto &in = *static_cast<std::ifstream *>(opaque);
        in.clear();
        in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!in)
            return 0;
        in.read(static_cast<char *>(buffer), static_cast<std::streamsize>(count));
        const auto got = in.gcount();
        return got > 0 ? static_cast<size_t>(got) : 0;
    }

    ~ReaderGuard()
    {
        if (open)
            mz_zip_reader_end(&archive);
    }
};

struct WriterGuard
{
    mz_zip_archive archive{};
    bool           open = false;

    ~WriterGuard()
    {
        if (open)
            mz_zip_writer_end(&archive);
    }
};

void openReader(ReaderGuard &guard, const std::filesystem::path &path)
{
    guard.stream.open(path, std::ios::binary | std::ios::ate);
    if (!guard.stream)
        throw ContractError("无法打开 ZIP 文件：" + path.string());
    const auto size = guard.stream.tellg();
    if (size <= 0)
        throw ContractError("ZIP 归档无效或已损坏：" + path.string());
    guard.archive.m_pRead      = &ReaderGuard::readCallback;
    guard.archive.m_pIO_opaque = &guard.stream;
    if (!mz_zip_reader_init(&guard.archive, static_cast<mz_uint64>(size), 0))
        throw ContractError("ZIP 归档无效或已损坏：" + path.string());
    guard.open = true;
}

std::vector<std::uint8_t> extractEntry(mz_zip_archive &archive, mz_uint index, std::string_view name)
{
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&archive, index, &stat))
        throw ContractError("无法读取 ZIP 条目元数据：" + std::string(name));
    std::vector<std::uint8_t> output(static_cast<std::size_t>(stat.m_uncomp_size));
    if (!output.empty()
        && !mz_zip_reader_extract_to_mem(&archive, index, output.data(), output.size(), 0))
        throw ContractError("解压 ZIP 条目失败：" + std::string(name));
    return output;
}

} // namespace

std::map<std::string, std::vector<std::uint8_t>> ZipArchive::readAll(const std::filesystem::path &path)
{
    ReaderGuard guard;
    openReader(guard, path);
    const mz_uint count = mz_zip_reader_get_num_files(&guard.archive);

    std::map<std::string, std::vector<std::uint8_t>> result;
    for (mz_uint index = 0; index < count; ++index)
    {
        if (mz_zip_reader_is_file_a_directory(&guard.archive, index))
            continue;
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&guard.archive, index, &stat))
            throw ContractError("无法读取 ZIP 目录条目");
        const std::string name(stat.m_filename);
        validateEntryName(name);
        if (result.contains(name))
            throw ContractError("ZIP 归档包含重复条目：" + name);
        result.emplace(name, extractEntry(guard.archive, index, name));
    }
    return result;
}

std::vector<std::uint8_t> ZipArchive::readEntry(const std::filesystem::path &path, std::string_view entryName)
{
    validateEntryName(entryName);
    ReaderGuard guard;
    openReader(guard, path);
    const mz_uint count = mz_zip_reader_get_num_files(&guard.archive);

    std::set<std::string> seen;
    mz_uint               matchIndex = count;
    for (mz_uint index = 0; index < count; ++index)
    {
        if (mz_zip_reader_is_file_a_directory(&guard.archive, index))
            continue;
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&guard.archive, index, &stat))
            throw ContractError("无法读取 ZIP 目录条目");
        const std::string name(stat.m_filename);
        validateEntryName(name);
        if (!seen.insert(name).second)
            throw ContractError("ZIP 归档包含重复条目：" + name);
        if (name == entryName)
            matchIndex = index;
    }
    if (matchIndex == count)
        throw ContractError("ZIP 归档缺少条目：" + std::string(entryName));
    return extractEntry(guard.archive, matchIndex, entryName);
}

void ZipArchive::write(const std::filesystem::path &path, const std::vector<ZipEntry> &entries)
{
    std::set<std::string> seen;
    for (const auto &entry : entries)
    {
        validateEntryName(entry.name);
        if (!seen.insert(entry.name).second)
            throw ContractError("ZIP 归档包含重复条目：" + entry.name);
    }

    WriterGuard guard;
    if (!mz_zip_writer_init_heap(&guard.archive, 0, 0))
        throw ContractError("初始化 ZIP 写入器失败");
    guard.open = true;

    for (const auto &entry : entries)
    {
        const void *data = entry.bytes.empty() ? static_cast<const void *>("") : entry.bytes.data();
        if (!mz_zip_writer_add_mem(&guard.archive, entry.name.c_str(), data, entry.bytes.size(), MZ_DEFAULT_LEVEL))
            throw ContractError("写入 ZIP 条目失败：" + entry.name);
    }

    void  *buffer     = nullptr;
    size_t bufferSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(&guard.archive, &buffer, &bufferSize))
        throw ContractError("完成 ZIP 归档打包失败");
    struct HeapFree
    {
        void *ptr = nullptr;
        ~HeapFree()
        {
            if (ptr)
                mz_free(ptr);
        }
    } heapFree{buffer};
    mz_zip_writer_end(&guard.archive);
    guard.open = false;

    writeAtomicBytes(path, buffer, bufferSize);
}

} // namespace visionrt
