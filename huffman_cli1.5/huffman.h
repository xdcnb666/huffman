#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <filesystem>

namespace HuffmanLib {
// 常量定义
constexpr uint32_t ARCHIVE_MAGIC = 0x48434648;  // "HCFH"

// 码长结构
struct CodeLengths {
    uint8_t lengths[256] = {0};
    bool valid = false;
};

// 解码节点
struct DecodeNode {
    DecodeNode* children[2] = {nullptr, nullptr};
    int sym = -1;
    ~DecodeNode() { delete children[0]; delete children[1]; }
};

// 进度回调
using ProgressCallback = std::function<void(double)>;

// ---------- 单文件 ----------
bool CompressFile(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outputPath,
                  ProgressCallback progress = nullptr);
bool DecompressFile(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath,
                    ProgressCallback progress = nullptr);

// ---------- 多文件归档 ----------
bool CompressFiles(const std::vector<std::filesystem::path>& filePaths,
                   const std::filesystem::path& archivePath,
                   const std::filesystem::path& rootDir,
                   ProgressCallback progress = nullptr,
                   bool solid = false);
bool DecompressArchive(const std::filesystem::path& archivePath,
                       const std::filesystem::path& outputDir,
                       ProgressCallback progress = nullptr);

// 辅助函数（用于生成规范哈夫曼码等）
void GenerateCanonicalCodes(const std::vector<uint64_t>& freq,
                            std::string codes[256],
                            CodeLengths& lengths);
void WriteCodeTable(std::ofstream& out, const CodeLengths& lengths);
bool ReadCodeTable(std::ifstream& in, CodeLengths& lengths);
DecodeNode* BuildCanonicalTree(const CodeLengths& lengths);
void DestroyTree(DecodeNode* node);
void CollectFilesRecursive(const std::filesystem::path& dir,
                           const std::filesystem::path& rootDir,
                           std::vector<std::filesystem::path>& fullPaths,
                           std::vector<std::filesystem::path>& relativePaths);
struct FileEntry {
    std::filesystem::path relativePath;
    uint64_t originalSize = 0;
    uint64_t dataOffset = 0;
    uint64_t compressedSize = 0;
};
struct ArchiveInfo {
    uint32_t fileCount = 0;
    uint64_t totalOriginal = 0;
    uint64_t totalCompressed = 0;
};
ArchiveInfo GetArchiveInfo(const std::filesystem::path& archivePath);
}

#endif // HUFFMAN_H