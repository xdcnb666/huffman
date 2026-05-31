#include "huffman.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace fs = std::filesystem;

void setConsoleUTF8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void progressCallback(double p) {
    std::cout << "\r进度: " << std::fixed << std::setprecision(2) << p * 100 << "%" << std::flush;
    if (p >= 1.0) std::cout << std::endl;
}

void printHelp(const char* progName) {
    std::cout << "用法:\n";
    std::cout << "  压缩 (支持多文件/文件夹/通配符): " << progName << " -1 <输入1> [输入2] ... [输出.huf]\n";
    std::cout << "  解压:                           " << progName << " -2 <输入.huf> <输出目录/文件>\n";
    std::cout << "  帮助:                           " << progName << " -3\n";
    std::cout << "  版本:                           " << progName << " -4\n";
    std::cout << "\n示例:\n";
    std::cout << "  压缩多个文件（自动生成输出名）: " << progName << " -1 a.txt b.jpg c.png\n";
    std::cout << "  压缩文件夹并指定输出名:         " << progName << " -1 myfolder myfolder.huf\n";
    std::cout << "  通配符压缩（自动生成输出名）:   " << progName << " -1 \"*.txt\"\n";
    std::cout << "  解压单文件:                     " << progName << " -2 test.huf out.txt\n";
    std::cout << "  解压归档包:                     " << progName << " -2 myfolder.huf output_dir\n";
}

bool isWildcardPattern(const std::string& path) {
    return path.find('*') != std::string::npos || path.find('?') != std::string::npos;
}

std::regex wildcardToRegex(const std::string& pattern) {
    std::string regexStr;
    for (char c : pattern) {
        if (c == '*') regexStr += ".*";
        else if (c == '?') regexStr += ".";
        else regexStr += c;
    }
    return std::regex(regexStr, std::regex::icase);
}

std::vector<fs::path> expandWildcard(const std::string& pattern) {
    fs::path parent = fs::current_path();
    std::string pat = pattern;
    if (fs::path(pattern).has_parent_path()) {
        parent = fs::path(pattern).parent_path();
        pat = fs::path(pattern).filename().string();
    }
    std::regex re = wildcardToRegex(pat);
    std::vector<fs::path> result;
    if (!fs::exists(parent) || !fs::is_directory(parent))
        return result;
    for (const auto& entry : fs::directory_iterator(parent)) {
        if (fs::is_regular_file(entry.path())) {
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, re))
                result.push_back(entry.path());
        }
    }
    return result;
}

// 自动生成输出文件名：将所有输入文件名（不带路径）用下划线连接，加上 .huf
std::string generateOutputName(const std::vector<fs::path>& inputs) {
    const size_t MAX_NAME_LEN = 200;
    std::string result;
    for (size_t i = 0; i < inputs.size(); ++i) {
        std::string name = inputs[i].filename().string();
        if (i != 0) result += "_";
        result += name;
        if (result.size() > MAX_NAME_LEN) {
            // 过长则使用固定名称
            return "combined.huf";
        }
    }
    result += ".huf";
    return result;
}

int funcCompress(int argc, char* argv[], int startIdx) {
    // 收集所有参数（从 startIdx 开始）
    std::vector<std::string> rawInputs;
    for (int i = startIdx; i < argc; ++i) {
        rawInputs.push_back(argv[i]);
    }
    if (rawInputs.empty()) {
        std::cerr << "错误: 压缩模式需要至少一个输入文件。\n";
        return 1;
    }

    std::string outputPath;
    std::vector<std::string> inputStrs;

    // 判断最后一个参数是否可能是输出文件名（以 .huf 结尾且不是通配符模式）
    std::string last = rawInputs.back();
    bool isOutputSpecified = false;
    if (last.size() >= 4 && 
        std::equal(last.end() - 4, last.end(), ".huf", [](char a, char b) { return std::tolower(a) == std::tolower(b); }) &&
        !isWildcardPattern(last)) {
        // 最后一个参数以 .huf 结尾，视为输出文件
        outputPath = last;
        inputStrs.assign(rawInputs.begin(), rawInputs.end() - 1);
        isOutputSpecified = true;
    } else {
        // 所有参数都是输入，自动生成输出文件名
        inputStrs = rawInputs;
    }

    // 展开通配符，收集所有有效输入路径
    std::vector<fs::path> allInputs;
    for (const auto& s : inputStrs) {
        if (isWildcardPattern(s)) {
            auto matched = expandWildcard(s);
            if (matched.empty()) {
                std::cerr << "警告: 通配符没有匹配到任何文件: " << s << std::endl;
            } else {
                allInputs.insert(allInputs.end(), matched.begin(), matched.end());
            }
        } else {
            allInputs.push_back(fs::path(s));
        }
    }

    if (allInputs.empty()) {
        std::cerr << "错误: 没有有效的输入文件。\n";
        return 1;
    }

    // 去重（保留顺序）
    std::sort(allInputs.begin(), allInputs.end());
    allInputs.erase(std::unique(allInputs.begin(), allInputs.end()), allInputs.end());

    // 如果没有指定输出路径，自动生成
    if (!isOutputSpecified) {
        outputPath = generateOutputName(allInputs);
        std::cout << "未指定输出文件名，自动生成为: " << outputPath << std::endl;
    }

    // 确定根目录（用于相对路径）
    fs::path rootDir = fs::current_path();
    // 如果所有输入都在同一个父目录下，使用该父目录；否则用当前目录
    if (allInputs.size() == 1 && fs::is_directory(allInputs[0])) {
        rootDir = allInputs[0].parent_path();
    } else {
        fs::path commonParent = allInputs[0].parent_path();
        bool sameParent = true;
        for (size_t i = 1; i < allInputs.size(); ++i) {
            if (allInputs[i].parent_path() != commonParent) {
                sameParent = false;
                break;
            }
        }
        if (sameParent) rootDir = commonParent;
    }

    auto start = std::chrono::high_resolution_clock::now();
    bool success = HuffmanLib::CompressFiles(allInputs, fs::path(outputPath), rootDir, progressCallback, false);
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    if (success) {
        std::cout << "\n压缩成功！耗时: " << elapsed << " 秒" << std::endl;
        // 计算总压缩率（可选，需要遍历所有输入文件计算原始总大小，这里省略）
        return 0;
    } else {
        std::cerr << "压缩失败！" << std::endl;
        return 1;
    }
}

int funcDecompress(int argc, char* argv[], int startIdx) {
    if (argc - startIdx < 2) {
        std::cerr << "错误: 解压模式需要输入文件和输出路径。\n";
        return 1;
    }
    std::string inputPath = argv[startIdx];
    std::string outputPath = argv[startIdx + 1];

    if (!fs::exists(inputPath)) {
        std::cerr << "错误: 输入文件不存在: " << inputPath << std::endl;
        return 1;
    }

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "错误: 无法打开文件: " << inputPath << std::endl;
        return 1;
    }
    uint32_t magic = 0;
    bool isArchive = false;
    if (in.read(reinterpret_cast<char*>(&magic), 4) && magic == HuffmanLib::ARCHIVE_MAGIC)
        isArchive = true;
    in.close();

    auto start = std::chrono::high_resolution_clock::now();
    bool success = false;
    if (isArchive) {
        std::cout << "检测到归档包，将解压到目录: " << outputPath << std::endl;
        success = HuffmanLib::DecompressArchive(fs::path(inputPath), fs::path(outputPath), progressCallback);
    } else {
        std::cout << "检测到单文件包，将解压到文件: " << outputPath << std::endl;
        success = HuffmanLib::DecompressFile(fs::path(inputPath), fs::path(outputPath), progressCallback);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    if (success) {
        std::cout << "\n解压成功！耗时: " << elapsed << " 秒" << std::endl;
        return 0;
    } else {
        std::cerr << "解压失败！" << std::endl;
        return 1;
    }
}

int funcHelp(int, char**, int) {
    printHelp("huffman_cli.exe");
    return 0;
}

int funcVersion(int, char**, int) {
    std::cout << "Huffman 压缩工具 1.0\n";
    std::cout << "算法: 规范哈夫曼编码\n";
    std::cout << "支持单文件、文件夹、多文件、通配符压缩\n";
    return 0;
}

int main(int argc, char* argv[]) {
    setConsoleUTF8();

    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    static const std::unordered_map<std::string, std::function<int(int, char**, int)>> funcMap = {
        {"-1", funcCompress},
        {"-2", funcDecompress},
        {"-3", funcHelp},
        {"-4", funcVersion},
    };

    auto it = funcMap.find(mode);
    if (it == funcMap.end()) {
        std::cerr << "错误: 未知选项 " << mode << "。使用 -3 查看帮助。\n";
        return 1;
    }

    return it->second(argc, argv, 2);
}