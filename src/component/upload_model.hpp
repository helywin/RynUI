#pragma once
#include <ryn/upload.hpp>

namespace ryn::detail {
struct UploadModelOptions final {
    bool controlled{};
    bool enabled{true};
    bool multiple{};
    bool directory{};
    UploadAccept accept;
    std::optional<std::size_t> max_count;
    UploadBefore before;
    UploadBefore transform;
    UploadTransport transport;
    UploadRemove remove;
    std::function<void(const UploadChange&)> on_change;
    std::function<void()> changed;
};

class UploadModel final {
public:
    UploadModel();
    ~UploadModel();
    UploadModel(const UploadModel&) = delete;
    UploadModel& operator=(const UploadModel&) = delete;
    void configure(UploadModelOptions);
    void set_files(UploadFiles);
    void set_options(UploadAccept, bool multiple, bool directory, std::optional<std::size_t>);
    void set_enabled(bool);
    bool select(NativeFiles);
    bool retry(const UploadUid&);
    bool abort(const UploadUid&);
    bool remove(const UploadUid&);
    void retire() noexcept;
    [[nodiscard]] UploadFiles files() const;
    [[nodiscard]] std::size_t pending_count() const;
    static void validate_files(const UploadFiles&);

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace ryn::detail
