#pragma once
#include <ryn/string.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ryn {
// Immutable selection snapshot. locator is opaque to Core; only the application/host performs IO.
struct NativeFile final {
    String name;
    String mime_type;
    String locator;
    String relative_path;
    std::uint64_t size{};
    std::shared_ptr<const std::vector<std::byte>> bytes;
    friend bool operator==(const NativeFile&, const NativeFile&) = default;
};

using NativeFiles = std::vector<std::shared_ptr<const NativeFile>>;

struct FileDialogOptions final {
    bool multiple{};
    bool directory{};
    std::vector<String> extensions;
};
enum class FileServiceError { None, Cancelled, Unavailable, InvalidSelection, IO, Limit };

struct FileSelection final {
    NativeFiles files;
    FileServiceError error{};
    String message;
};

using FileServiceCancel = std::function<void()>;
using FileSelectionCompletion = std::function<bool(FileSelection)>;

// Callback delivery and drain are owner-thread operations. SDK adapters marshal foreign-thread results.
class NativeFileServices {
public:
    virtual ~NativeFileServices() = default;
    virtual FileServiceCancel choose(const FileDialogOptions&, FileSelectionCompletion) = 0;
    virtual FileServiceCancel resolve(const std::vector<String>& locators, bool directory, FileSelectionCompletion) = 0;

    virtual FileServiceCancel paste(FileSelectionCompletion completion) {
        completion(pasted_files());
        return {};
    }

    virtual FileSelection pasted_files() {
        return {{}, FileServiceError::Unavailable, String{u8"File clipboard is unavailable"}};
    }

    virtual std::size_t drain() {
        return 0;
    }
};
} // namespace ryn
