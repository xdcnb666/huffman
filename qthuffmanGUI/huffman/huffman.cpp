#include "huffman.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <functional>
#include <bitset>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <chrono>

namespace HuffmanLib {

// 生成规范哈夫曼码
void GenerateCanonicalCodes(const std::vector<uint64_t>& freq,
                            std::string codes[256],
                            CodeLengths& lengths) {
    struct Node {
        uint64_t freq;
        int sym;   // >=0 表示叶子
        Node *left, *right;
        Node(uint64_t f, int s) : freq(f), sym(s), left(nullptr), right(nullptr) {}
        Node(Node* l, Node* r) : freq(l->freq + r->freq), sym(-1), left(l), right(r) {}
    };
    std::vector<Node*> pool;
    auto cmp = [](Node* a, Node* b) { return a->freq > b->freq; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> pq(cmp);

    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            Node* n = new Node(freq[i], i);
            pq.push(n);
            pool.push_back(n);
        }
    }
    if (pq.empty()) {
        lengths.valid = false;
        return;
    }
    while (pq.size() > 1) {
        Node* l = pq.top(); pq.pop();
        Node* r = pq.top(); pq.pop();
        Node* p = new Node(l, r);
        pq.push(p);
        pool.push_back(p);
    }
    Node* root = pq.top();

    std::function<void(Node*, int)> dfs = [&](Node* node, int depth) {
        if (!node) return;
        if (node->sym >= 0) {
            lengths.lengths[node->sym] = static_cast<uint8_t>(depth);
        } else {
            dfs(node->left, depth + 1);
            dfs(node->right, depth + 1);
        }
    };
    dfs(root, 0);
    for (Node* n : pool) delete n;

    // 规范码排序
    std::vector<int> symbols;
    for (int i = 0; i < 256; ++i)
        if (lengths.lengths[i] > 0) symbols.push_back(i);
    std::sort(symbols.begin(), symbols.end(), [&](int a, int b) {
        if (lengths.lengths[a] != lengths.lengths[b])
            return lengths.lengths[a] < lengths.lengths[b];
        return a < b;
    });

    uint32_t code = 0;
    int prevLen = lengths.lengths[symbols[0]];
    for (int sym : symbols) {
        int len = lengths.lengths[sym];
        if (len > prevLen) {
            code <<= (len - prevLen);
            prevLen = len;
        }
        std::string codestr(len, '0');
        for (int i = len - 1; i >= 0; --i)
            codestr[len - 1 - i] = ((code >> i) & 1) ? '1' : '0';
        codes[sym] = codestr;
        code++;
    }
    lengths.valid = true;
}

// 码表读写
void WriteCodeTable(std::ofstream& out, const CodeLengths& lengths) {
    if (!lengths.valid) {
        uint8_t flag = 0;
        out.write(reinterpret_cast<const char*>(&flag), 1);
        return;
    }
    uint8_t flag = 1;
    out.write(reinterpret_cast<const char*>(&flag), 1);
    std::vector<uint8_t> present;
    for (int i = 0; i < 256; ++i)
        if (lengths.lengths[i] > 0) present.push_back(static_cast<uint8_t>(i));
    uint8_t count = static_cast<uint8_t>(present.size());
    out.write(reinterpret_cast<const char*>(&count), 1);
    for (uint8_t sym : present) {
        out.write(reinterpret_cast<const char*>(&sym), 1);
        out.write(reinterpret_cast<const char*>(&lengths.lengths[sym]), 1);
    }
}

bool ReadCodeTable(std::ifstream& in, CodeLengths& lengths) {
    uint8_t flag;
    in.read(reinterpret_cast<char*>(&flag), 1);
    if (!in || flag == 0) {
        lengths.valid = false;
        return true;
    }
    uint8_t count;
    in.read(reinterpret_cast<char*>(&count), 1);
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t sym, len;
        in.read(reinterpret_cast<char*>(&sym), 1);
        in.read(reinterpret_cast<char*>(&len), 1);
        lengths.lengths[sym] = len;
    }
    lengths.valid = true;
    return true;
}

DecodeNode* BuildCanonicalTree(const CodeLengths& lengths) {
    if (!lengths.valid) return nullptr;
    auto* root = new DecodeNode();
    std::vector<int> symbols;
    for (int i = 0; i < 256; ++i)
        if (lengths.lengths[i] > 0) symbols.push_back(i);
    std::sort(symbols.begin(), symbols.end(), [&](int a, int b) {
        if (lengths.lengths[a] != lengths.lengths[b])
            return lengths.lengths[a] < lengths.lengths[b];
        return a < b;
    });

    uint32_t code = 0;
    int prevLen = lengths.lengths[symbols[0]];
    for (int sym : symbols) {
        int len = lengths.lengths[sym];
        if (len > prevLen) {
            code <<= (len - prevLen);
            prevLen = len;
        }
        DecodeNode* node = root;
        for (int bit = len - 1; bit >= 0; --bit) {
            int dir = (code >> bit) & 1;
            if (!node->children[dir])
                node->children[dir] = new DecodeNode();
            node = node->children[dir];
        }
        node->sym = sym;
        code++;
    }
    return root;
}

void DestroyTree(DecodeNode* node) {
    delete node;
}

// ==================== 单文件压缩/解压 ====================
bool CompressFile(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outputPath,
                  ProgressCallback progress) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    std::vector<uint64_t> freq(256, 0);
    uint64_t totalBytes = 0;
    char ch;
    while (in.read(&ch, 1)) {
        freq[static_cast<unsigned char>(ch)]++;
        totalBytes++;
    }
    in.clear();
    in.seekg(0, std::ios::beg);

    std::string codes[256];
    CodeLengths lengths;
    GenerateCanonicalCodes(freq, codes, lengths);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&totalBytes), 8);
    WriteCodeTable(out, lengths);

    std::string bitBuffer;
    auto flushBits = [&](bool final) {
        while (bitBuffer.size() >= 8 || (final && !bitBuffer.empty())) {
            if (bitBuffer.size() < 8) bitBuffer.append(8 - bitBuffer.size(), '0');
            std::bitset<8> byte(bitBuffer.substr(0, 8));
            unsigned char byteVal = static_cast<unsigned char>(byte.to_ulong());
            out.write(reinterpret_cast<const char*>(&byteVal), 1);
            bitBuffer = bitBuffer.substr(8);
        }
    };

    uint64_t processed = 0;
    while (in.read(&ch, 1)) {
        bitBuffer += codes[static_cast<unsigned char>(ch)];
        flushBits(false);
        if (progress && totalBytes > 0 && (processed++ % 1024 == 0))
            progress(static_cast<double>(processed) / totalBytes);
    }
    flushBits(true);
    if (progress) progress(1.0);
    return out.good();
}

bool DecompressFile(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath,
                    ProgressCallback progress) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    uint64_t originalSize;
    in.read(reinterpret_cast<char*>(&originalSize), 8);
    CodeLengths lengths;
    ReadCodeTable(in, lengths);
    if (!lengths.valid) {
        std::ofstream out(outputPath, std::ios::binary);
        return out.good();
    }
    DecodeNode* root = BuildCanonicalTree(lengths);
    if (!root) return false;

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) { DestroyTree(root); return false; }

    uint64_t decoded = 0;
    DecodeNode* node = root;
    char byte;
    while (decoded < originalSize && in.read(&byte, 1)) {
        unsigned char uc = static_cast<unsigned char>(byte);
        for (int i = 7; i >= 0; --i) {
            if (decoded >= originalSize) break;
            int bit = (uc >> i) & 1;
            if (node->children[bit])
                node = node->children[bit];
            if (node->sym >= 0) {
                out.put(static_cast<char>(node->sym));
                decoded++;
                node = root;
            }
        }
        if (progress && originalSize > 0)
            progress(static_cast<double>(decoded) / originalSize);
    }
    DestroyTree(root);
    if (progress) progress(1.0);
    return decoded == originalSize;
}

// ==================== 目录递归收集 ====================
void CollectFilesRecursive(const std::filesystem::path& dir,
                           const std::filesystem::path& rootDir,
                           std::vector<std::filesystem::path>& fullPaths,
                           std::vector<std::filesystem::path>& relativePaths) {
    namespace fs = std::filesystem;
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (fs::is_regular_file(entry.path())) {
            fullPaths.push_back(entry.path());
            fs::path rel = fs::relative(entry.path(), rootDir);
            relativePaths.push_back(rel);
        }
    }
}

// ==================== 多文件归档压缩 ====================
bool CompressFiles(const std::vector<std::filesystem::path>& filePaths,
                   const std::filesystem::path& archivePath,
                   const std::filesystem::path& rootDir,
                   ProgressCallback progress,
                   bool /*solid*/) {
    namespace fs = std::filesystem;
    std::vector<fs::path> allFiles, relatives;
    for (const auto& p : filePaths) {
        if (fs::is_directory(p)) {
            fs::path base = rootDir.empty() ? p : rootDir;
            CollectFilesRecursive(p, base, allFiles, relatives);
        } else if (fs::is_regular_file(p)) {
            allFiles.push_back(p);
            fs::path rel;
            if (!rootDir.empty() && p.string().find(rootDir.string()) == 0) {
                rel = fs::relative(p, rootDir);
            } else {
                rel = p.filename();
            }
            relatives.push_back(rel);
        }
    }
    if (allFiles.empty()) return false;

    std::vector<FileEntry> entries(allFiles.size());
    for (size_t i = 0; i < allFiles.size(); ++i) {
        entries[i].relativePath = relatives[i];
        std::error_code ec;
        entries[i].originalSize = fs::file_size(allFiles[i], ec);
        if (ec) entries[i].originalSize = 0;
    }

    // 计算头大小
    uint32_t headerSize = 8; // magic + count
    for (const auto& e : entries) {
        std::string utf8path = e.relativePath.string();   // 修改：使用 string() 而非 u8string()
        headerSize += 2 + static_cast<uint32_t>(utf8path.size()) + 8 + 8 + 8;
    }

    std::ofstream out(archivePath, std::ios::binary);
    if (!out) return false;
    std::vector<char> dummy(headerSize, 0);
    out.write(dummy.data(), dummy.size());
    uint64_t currentOffset = headerSize;

    for (size_t i = 0; i < allFiles.size(); ++i) {
        std::ifstream inFile(allFiles[i], std::ios::binary);
        if (!inFile) continue;
        std::vector<uint64_t> freq(256, 0);
        uint64_t total = 0;
        char ch;
        while (inFile.read(&ch, 1)) {
            freq[static_cast<unsigned char>(ch)]++;
            total++;
        }
        inFile.clear();
        inFile.seekg(0);
        std::string codes[256];
        CodeLengths lengths;
        GenerateCanonicalCodes(freq, codes, lengths);

        entries[i].dataOffset = currentOffset;
        uint64_t compSizePos = out.tellp();
        uint64_t compSizePlaceholder = 0;
        out.write(reinterpret_cast<const char*>(&compSizePlaceholder), 8);
        out.write(reinterpret_cast<const char*>(&total), 8);
        WriteCodeTable(out, lengths);

        std::string bitBuffer;
        auto flushBits = [&](bool final) {
            while (bitBuffer.size() >= 8 || (final && !bitBuffer.empty())) {
                if (bitBuffer.size() < 8) bitBuffer.append(8 - bitBuffer.size(), '0');
                std::bitset<8> byte(bitBuffer.substr(0, 8));
                unsigned char b = static_cast<unsigned char>(byte.to_ulong());
                out.write(reinterpret_cast<const char*>(&b), 1);
                bitBuffer = bitBuffer.substr(8);
            }
        };
        while (inFile.read(&ch, 1)) {
            bitBuffer += codes[static_cast<unsigned char>(ch)];
            flushBits(false);
        }
        flushBits(true);
        uint64_t dataEnd = out.tellp();
        uint64_t compSize = dataEnd - currentOffset;
        out.seekp(compSizePos);
        out.write(reinterpret_cast<const char*>(&compSize), 8);
        out.seekp(dataEnd);
        entries[i].compressedSize = compSize;
        currentOffset = dataEnd;

        if (progress)
            progress(static_cast<double>(i + 1) / allFiles.size());
    }

    // 回填文件头
    out.seekp(0);
    uint32_t magic = ARCHIVE_MAGIC;
    out.write(reinterpret_cast<const char*>(&magic), 4);
    uint32_t fileCount = static_cast<uint32_t>(entries.size());
    out.write(reinterpret_cast<const char*>(&fileCount), 4);
    for (const auto& e : entries) {
        std::string utf8path = e.relativePath.string();   // 修改：使用 string()
        uint16_t len = static_cast<uint16_t>(utf8path.size());
        out.write(reinterpret_cast<const char*>(&len), 2);
        out.write(utf8path.data(), len);
        out.write(reinterpret_cast<const char*>(&e.originalSize), 8);
        out.write(reinterpret_cast<const char*>(&e.dataOffset), 8);
        out.write(reinterpret_cast<const char*>(&e.compressedSize), 8);
    }
    return out.good();
}

// ==================== 多文件归档解压 ====================
bool DecompressArchive(const std::filesystem::path& archivePath,
                       const std::filesystem::path& outputDir,
                       ProgressCallback progress) {
    std::ifstream in(archivePath, std::ios::binary);
    if (!in) return false;
    uint32_t magic;
    in.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != ARCHIVE_MAGIC) return false;
    uint32_t fileCount;
    in.read(reinterpret_cast<char*>(&fileCount), 4);
    std::vector<FileEntry> entries(fileCount);
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t len;
        in.read(reinterpret_cast<char*>(&len), 2);
        std::string utf8path(len, '\0');
        in.read(&utf8path[0], len);
        entries[i].relativePath = std::filesystem::path(utf8path);   // 修改：直接用 string 构造
        in.read(reinterpret_cast<char*>(&entries[i].originalSize), 8);
        in.read(reinterpret_cast<char*>(&entries[i].dataOffset), 8);
        in.read(reinterpret_cast<char*>(&entries[i].compressedSize), 8);
    }

    std::filesystem::create_directories(outputDir);
    for (uint32_t i = 0; i < fileCount; ++i) {
        in.seekg(entries[i].dataOffset);
        uint64_t compSize, origSize;
        in.read(reinterpret_cast<char*>(&compSize), 8);
        in.read(reinterpret_cast<char*>(&origSize), 8);
        CodeLengths lengths;
        ReadCodeTable(in, lengths);

        std::filesystem::path outPath = outputDir / entries[i].relativePath;
        std::filesystem::create_directories(outPath.parent_path());
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) continue;
        if (!lengths.valid) {
            outFile.close();
            continue;
        }
        DecodeNode* root = BuildCanonicalTree(lengths);
        uint64_t decoded = 0;
        DecodeNode* node = root;
        char byte;
        while (decoded < origSize && in.read(&byte, 1)) {
            unsigned char uc = static_cast<unsigned char>(byte);
            for (int bit = 7; bit >= 0; --bit) {
                if (decoded >= origSize) break;
                int b = (uc >> bit) & 1;
                if (node->children[b])
                    node = node->children[b];
                if (node->sym >= 0) {
                    outFile.put(static_cast<char>(node->sym));
                    decoded++;
                    node = root;
                }
            }
        }
        DestroyTree(root);
        outFile.close();
        if (progress)
            progress(static_cast<double>(i + 1) / fileCount);
    }
    return true;
}

ArchiveInfo GetArchiveInfo(const std::filesystem::path& archivePath) {
    ArchiveInfo info{0, 0, 0};
    std::ifstream in(archivePath, std::ios::binary);
    if (!in) return info;
    uint32_t magic, fileCount;
    in.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != ARCHIVE_MAGIC) return info;
    in.read(reinterpret_cast<char*>(&fileCount), 4);
    info.fileCount = fileCount;
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint16_t len;
        in.read(reinterpret_cast<char*>(&len), 2);
        in.ignore(len);
        uint64_t orig, offset, comp;
        in.read(reinterpret_cast<char*>(&orig), 8);
        in.read(reinterpret_cast<char*>(&offset), 8);
        in.read(reinterpret_cast<char*>(&comp), 8);
        info.totalOriginal += orig;
        info.totalCompressed += comp;
    }
    return info;
}
}