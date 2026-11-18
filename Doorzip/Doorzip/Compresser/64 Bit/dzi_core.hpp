#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// 1. Target Byte Compression & Null-Byte Padding (.doorz, .dzi, .dzz, .zd)
bool targetByteCompress(const std::string& inputPath, const std::string& outputFilePath, size_t targetBytes) {
    // Note: For full ZIP compression in C++, link with 'miniz.h' or 'zlib'. 
    // This example handles the binary stream padding and container wrapping logic.
    
    std::ifstream inFile(inputPath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) return false;
    
    std::streamsize currentSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    // Safety check: Payload cannot exceed target size
    if (static_cast<size_t>(currentSize) > targetBytes) {
        std::cerr << "Error: Payload size exceeds target bytes.\n";
        return false;
    }

    std::vector<char> buffer(currentSize);
    if (!inFile.read(buffer.data(), currentSize)) return false;
    inFile.close();

    // Write payload + exact null-byte padding to the custom extension file
    std::ofstream outFile(outputFilePath, std::ios::binary);
    if (!outFile.is_open()) return false;

    outFile.write(buffer.data(), currentSize);

    size_t paddingNeeded = targetBytes - currentSize;
    std::vector<char> zeroPad(4096, 0); // 4KB chunk of zeros for fast writing
    while (paddingNeeded > 0) {
        size_t writeSize = std::min(paddingNeeded, zeroPad.size());
        outFile.write(zeroPad.data(), writeSize);
        paddingNeeded -= writeSize;
    }

    outFile.close();
    std::cout << "Successfully created exact-byte container: " << outputFilePath 
              << " (" << fs::file_size(outputFilePath) << " bytes)\n";
    return true;
}

// 2. Unpadding & Extraction Logic (Strips trailing null bytes using ZIP EOCD signature)
bool extractArchive(const std::string& archivePath, const std::string& extractTo) {
    std::ifstream file(archivePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> data(fileSize);
    if (!file.read(data.data(), fileSize)) return false;
    file.close();

    // Search backwards for the ZIP End of Central Directory (EOCD) signature: 0x06054b50
    long long eocdIdx = -1;
    for (long long i = fileSize - 22; i >= 0; --i) {
        if (data[i] == '\x50' && data[i+1] == '\x4b' && 
            data[i+2] == '\x05' && data[i+3] == '\x06') {
            eocdIdx = i;
            break;
        }
    }

    if (eocdIdx == -1 || fileSize < eocdIdx + 22) return false;

    // Calculate true ZIP size by reading the comment length field
    unsigned short commentLen = *reinterpret_cast<unsigned short*>(&data[eocdIdx + 20]);
    size_t trueZipSize = eocdIdx + 22 + commentLen;

    // Output clean temporary zip file for standard extraction
    std::string cleanZipPath = archivePath + ".tmp.zip";
    std::ofstream cleanFile(cleanZipPath, std::ios::binary);
    cleanFile.write(data.data(), trueZipSize);
    cleanFile.close();

    // (Extraction logic using your preferred unzipping library goes here)
    // After extraction, clean up: fs::remove(cleanZipPath);
    
    return true;
}