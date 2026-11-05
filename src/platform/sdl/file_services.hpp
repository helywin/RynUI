#pragma once
#include <ryn/file_service.hpp>
#include "platform/sdl/platform_state.hpp"
#include <thread>

namespace ryn::detail {
// SDK seam: tests supply callbacks without opening any OS service.
class SdlDialogApi {
public:
    using Callback = void (*)(void*, const char* const*, int);
    virtual ~SdlDialogApi() = default;
    virtual void show(PlatformWindowHandle, const FileDialogOptions&, Callback, void*) = 0;
    virtual String error() const = 0;
};

class SdlFileServices final : public NativeFileServices {
public:
    explicit SdlFileServices(PlatformState&);
    SdlFileServices(SdlDialogApi&, PlatformWindowHandle, std::function<void()> wake);
    ~SdlFileServices() override;
    FileServiceCancel choose(const FileDialogOptions&, FileSelectionCompletion) override;
    FileServiceCancel resolve(const std::vector<String>&, bool directory, FileSelectionCompletion) override;
    FileServiceCancel paste(FileSelectionCompletion) override;
    std::size_t drain() override;

private:
    struct Queue;
    struct Operation;
    struct Worker;
    static void dialog_result(void*, const char* const*, int);
    static void resolve_async(const std::shared_ptr<Queue>&, const std::shared_ptr<Operation>&, std::vector<String>);
    SdlDialogApi* api_;
    PlatformWindowHandle window_{};
    std::shared_ptr<Queue> queue_;
    std::weak_ptr<Operation> dialog_;
};
} // namespace ryn::detail
