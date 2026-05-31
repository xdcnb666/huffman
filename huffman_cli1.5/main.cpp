#include "huffman.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ---------- 宽字符版通配符匹配 ----------
bool WildcardMatch(const std::wstring& pattern, const std::wstring& str) {
    size_t i = 0, j = 0;
    size_t starPos = std::wstring::npos;
    size_t matchPos = 0;
    while (j < str.size()) {
        if (i < pattern.size() && (pattern[i] == str[j] || pattern[i] == L'?')) {
            ++i; ++j;
        } else if (i < pattern.size() && pattern[i] == L'*') {
            starPos = i;
            matchPos = j;
            ++i;
        } else if (starPos != std::wstring::npos) {
            i = starPos + 1;
            ++matchPos;
            j = matchPos;
        } else {
            return false;
        }
    }
    while (i < pattern.size() && pattern[i] == L'*') ++i;
    return i == pattern.size();
}

// ---------- 宽字符版通配符展开 ----------
std::vector<fs::path> ExpandWildcard(const std::wstring& raw) {
    if (raw.find(L'*') == std::wstring::npos && raw.find(L'?') == std::wstring::npos) {
        return { fs::path(raw) };
    }

    fs::path rawPath(raw);
    fs::path parent = rawPath.parent_path();
    std::wstring pattern = rawPath.filename().wstring();

    if (parent.empty()) parent = L".";

    std::error_code ec;
    std::vector<fs::path> result;
    if (!fs::is_directory(parent, ec)) {
        return {};
    }

    for (const auto& entry : fs::directory_iterator(parent, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec)) {
            std::wstring fname = entry.path().filename().wstring();
            if (WildcardMatch(pattern, fname)) {
                result.push_back(entry.path());
            }
        }
    }
    return result;
}

// ---------- 窄字符版进度显示 ----------
void ShowProgress(double progress) {
    static int lastPercent = -1;
    int percent = static_cast<int>(progress * 100.0);
    if (percent != lastPercent) {
        std::cerr << "\rProgress: " << percent << "%" << std::flush;
        lastPercent = percent;
    }
    if (progress >= 1.0) {
        std::cerr << std::endl;
        lastPercent = -1;
    }
}

// ---------- 递归获取路径总大小 ----------
uint64_t GetTotalSize(const std::vector<fs::path>& paths) {
    uint64_t total = 0;
    for (const auto& p : paths) {
        std::error_code ec;
        if (fs::is_regular_file(p, ec)) {
            total += fs::file_size(p, ec);
        } else if (fs::is_directory(p, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(p, ec)) {
                if (entry.is_regular_file(ec)) {
                    total += entry.file_size(ec);
                }
            }
        }
    }
    return total;
}

// ---------- 窄字符版压缩统计输出 ----------
void PrintCompressStats(double seconds, uint64_t originalSize,
                        const fs::path& outputPath) {
    std::error_code ec;
    uint64_t compressedSize = fs::file_size(outputPath, ec);
    if (ec) {
        std::cerr << "Warning: cannot read output file size.\n";
        return;
    }

    double ratioSaved = 0.0;
    double ratioCompressed = 0.0;
    double factor = 0.0;
    if (originalSize > 0 && compressedSize > 0) {
        ratioSaved = (1.0 - static_cast<double>(compressedSize) / originalSize) * 100.0;
        ratioCompressed = static_cast<double>(compressedSize) / originalSize * 100.0;
        factor = static_cast<double>(originalSize) / compressedSize;
    }

    std::cerr << "Compression finished.\n"
              << "  Time:                    " << seconds << " s\n"
              << "  Original size:           " << originalSize << " bytes\n"
              << "  Compressed size:         " << compressedSize << " bytes\n"
              << "  Compressed/Original:     " << ratioCompressed << "%\n"
              << "  Compression factor:      " << factor << "x (original/compressed)\n"
              << "  Space saved:             " << ratioSaved << "%\n";
}

// ---------- 使用说明 ----------
void PrintUsage() {
    std::cerr << "Huffman Compression Tool (C++ CLI)\n\n"
              << "Usage:\n"
              << "  huffman -11 <input> [output]\n"
              << "      Compress a single file. Wildcards allowed if they match exactly one file.\n"
              << "      Default output: <input>.huf\n\n"
              << "  huffman -12 <file/pattern> [file/pattern ...] [output]\n"
              << "      Compress multiple files. Wildcards (*, ?) are supported.\n"
              << "      If the last argument is NOT an existing file, it will be used as output name.\n"
              << "      Auto-generated name: file1_file2_....huf\n\n"
              << "  huffman -13 <folder> [output]\n"
              << "      Compress a folder (preserves structure). Wildcards NOT allowed.\n"
              << "      Default output: <folder>.huf\n\n"
              << "  huffman -21 <input> [output]\n"
              << "      Decompress a SINGLE-FILE .huf. Default output: strip .huf extension\n"
              << "      WARNING: Do not use on multi-file archives! Use -22 instead.\n\n"
              << "  huffman -22 <archive> [output_dir]\n"
              << "  huffman -23 <archive> [output_dir]\n"
              << "      Decompress a MULTI-FILE archive. Default output: current directory (.)\n\n"
              << "Examples:\n"
              << "  huffman -11 document.txt\n"
              << "  huffman -12 *.jpg\n"
              << "  huffman -12 a.txt b.txt c.txt myarchive.huf\n"
              << "  huffman -13 my_project\n"
              << "  huffman -21 document.txt.huf restored.txt\n"
              << "  huffman -22 backup.huf .\n";
}

// ---------- 生成归档名 ----------
std::wstring MakeArchiveName(const std::vector<fs::path>& inputs) {
    if (inputs.empty()) return L"archive.huf";
    std::wostringstream oss;
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (i > 0) oss << L"_";
        std::wstring stem = inputs[i].stem().wstring();
        if (stem.empty()) stem = inputs[i].filename().wstring();
        oss << stem;
    }
    oss << L".huf";
    return oss.str();
}

// ---------- 主函数 ----------
int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::wstring mode = argv[1];

    // ========== 模式 -11 ==========
    if (mode == L"-11") {
        if (argc < 3) {
            std::cerr << "Error: missing input file.\n";
            PrintUsage();
            return 1;
        }

        std::vector<fs::path> matches = ExpandWildcard(argv[2]);
        if (matches.empty()) {
            std::cerr << "Error: no file matches the pattern.\n";
            return 1;
        }
        if (matches.size() > 1) {
            std::cerr << "Error: pattern matched multiple files, use -12 for multiple files.\n";
            return 1;
        }

        fs::path input = matches[0];
        fs::path output;
        if (argc >= 4) {
            output = fs::path(argv[3]);
        } else {
            output = input.wstring() + L".huf";
        }

        std::error_code ec;
        uint64_t originalSize = fs::file_size(input, ec);
        if (ec) {
            std::cerr << "Error: cannot read input file size.\n";
            return 1;
        }

        std::cerr << "Compressing file..." << std::endl;

        auto start = std::chrono::steady_clock::now();
        bool success = HuffmanLib::CompressFile(input, output, ShowProgress);
        auto end = std::chrono::steady_clock::now();

        if (!success) {
            std::cerr << "Compression failed!\n";
            return 1;
        }

        std::chrono::duration<double> elapsed = end - start;
        PrintCompressStats(elapsed.count(), originalSize, output);
    }

    // ========== 模式 -12 ==========
    else if (mode == L"-12") {
        if (argc < 3) {
            std::cerr << "Error: no input files provided.\n";
            PrintUsage();
            return 1;
        }

        std::vector<std::wstring> rawArgs;
        for (int i = 2; i < argc; ++i) {
            rawArgs.emplace_back(argv[i]);
        }

        std::vector<fs::path> allInputs;
        for (const auto& arg : rawArgs) {
            auto expanded = ExpandWildcard(arg);
            if (expanded.empty()) {
                std::cerr << "Warning: pattern did not match any file, skipping.\n";
                continue;
            }
            allInputs.insert(allInputs.end(), expanded.begin(), expanded.end());
        }

        if (allInputs.empty()) {
            std::cerr << "Error: no input files found after expanding patterns.\n";
            return 1;
        }

        fs::path output;
        if (!rawArgs.empty()) {
            std::wstring lastStr = rawArgs.back();
            // 如果最后一个参数包含通配符，直接自动生成输出名
            if (lastStr.find(L'*') != std::wstring::npos || lastStr.find(L'?') != std::wstring::npos) {
                output = MakeArchiveName(allInputs);
            } else {
                fs::path lastArg(lastStr);
                bool lastArgIsInput = false;
                std::error_code ec;
                if (fs::is_regular_file(lastArg, ec)) {
                    auto it = std::find(allInputs.begin(), allInputs.end(), lastArg);
                    if (it != allInputs.end()) {
                        lastArgIsInput = true;
                    }
                }
                if (!lastArgIsInput && !fs::is_directory(lastArg, ec)) {
                    output = lastArg;
                } else {
                    output = MakeArchiveName(allInputs);
                }
            }
        } else {
            output = MakeArchiveName(allInputs);
        }

        uint64_t originalSize = GetTotalSize(allInputs);
        std::cerr << "Compressing " << allInputs.size() << " file(s)..." << std::endl;

        auto start = std::chrono::steady_clock::now();
        bool success = HuffmanLib::CompressFiles(allInputs, output, L"", ShowProgress, false);
        auto end = std::chrono::steady_clock::now();

        if (!success) {
            std::cerr << "Compression failed!\n";
            return 1;
        }

        std::chrono::duration<double> elapsed = end - start;
        PrintCompressStats(elapsed.count(), originalSize, output);
    }

    // ========== 模式 -13 ==========
    else if (mode == L"-13") {
        if (argc < 3) {
            std::cerr << "Error: missing folder path.\n";
            PrintUsage();
            return 1;
        }
        fs::path folder(argv[2]);
        std::wstring folderStr = folder.wstring();
        if (folderStr.find(L'*') != std::wstring::npos || folderStr.find(L'?') != std::wstring::npos) {
            std::cerr << "Error: wildcards not allowed in folder mode (-13). Use -12 for multiple files.\n";
            return 1;
        }
        std::error_code ec;
        if (!fs::is_directory(folder, ec)) {
            std::cerr << "Error: specified path is not a directory.\n";
            return 1;
        }

        fs::path output;
        if (argc >= 4) {
            output = fs::path(argv[3]);
        } else {
            output = folder.filename().wstring() + L".huf";
        }

        fs::path rootDir = folder.parent_path();
        if (rootDir.empty()) rootDir = L".";

        uint64_t originalSize = GetTotalSize({folder});
        std::cerr << "Compressing folder..." << std::endl;

        auto start = std::chrono::steady_clock::now();
        bool success = HuffmanLib::CompressFiles({folder}, output, rootDir, ShowProgress, false);
        auto end = std::chrono::steady_clock::now();

        if (!success) {
            std::cerr << "Compression failed!\n";
            return 1;
        }

        std::chrono::duration<double> elapsed = end - start;
        PrintCompressStats(elapsed.count(), originalSize, output);
    }

    // ========== 模式 -21 ==========
    else if (mode == L"-21") {
        if (argc < 3) {
            std::cerr << "Error: missing input file.\n";
            PrintUsage();
            return 1;
        }
        fs::path input(argv[2]);
        fs::path output;
        if (argc >= 4) {
            output = fs::path(argv[3]);
        } else {
            std::wstring name = input.filename().wstring();
            if (name.size() > 4 && name.substr(name.size() - 4) == L".huf") {
                output = input.parent_path() / name.substr(0, name.size() - 4);
            } else {
                output = input.wstring() + L".out";
            }
        }

        std::error_code ec;
        if (fs::exists(output, ec) && fs::is_regular_file(output, ec)) {
            std::cerr << "Error: output file already exists. Please delete it first.\n";
            return 1;
        }

        std::cerr << "Decompressing file..." << std::endl;
        if (!HuffmanLib::DecompressFile(input, output, ShowProgress)) {
            std::cerr << "Decompression failed!\n";
            return 1;
        }
        std::cerr << "Decompression finished.\n";
    }

    // ========== 模式 -22 / -23 ==========
    else if (mode == L"-22" || mode == L"-23") {
        if (argc < 3) {
            std::cerr << "Error: missing archive file.\n";
            PrintUsage();
            return 1;
        }
        fs::path archive(argv[2]);
        fs::path outDir;
        if (argc >= 4) {
            outDir = fs::path(argv[3]);
        } else {
            outDir = L".";
        }

        std::error_code ec;
        if (fs::exists(outDir, ec) && fs::is_regular_file(outDir, ec)) {
            std::cerr << "Error: output path exists as a file, not a directory.\n";
            return 1;
        }

        std::cerr << "Decompressing archive..." << std::endl;
        if (!HuffmanLib::DecompressArchive(archive, outDir, ShowProgress)) {
            std::cerr << "Decompression failed!\n";
            return 1;
        }
        std::cerr << "Decompression finished.\n";
    }

    else {
        std::cerr << "Unknown mode.\n";
        PrintUsage();
        return 1;
    }

    return 0;
}