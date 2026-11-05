#include "platform/sdl/file_services.hpp"
#include <SDL3/SDL.h>
#include <atomic>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <utility>

namespace ryn::detail {
namespace {
class DialogSdk final : public SdlDialogApi {
    struct Lease final {
        Callback callback;
        void* data;
        std::vector<std::string> patterns;
        std::vector<SDL_DialogFileFilter> filters;
    };

    static void SDLCALL delivered(void* data, const char* const* files, int filter) {
        std::unique_ptr<Lease> lease(static_cast<Lease*>(data));
        lease->callback(lease->data, files, filter);
    }

public:
    void show(PlatformWindowHandle window, const FileDialogOptions& options, Callback callback, void* data) override {
        auto lease = std::make_unique<Lease>();
        lease->callback = callback;
        lease->data = data;
        for (const auto& ext : options.extensions) {
            auto pattern = std::string(ext.bytes());
            if (!pattern.empty() && pattern.front() == '.') {
                pattern.erase(0, 1);
            }
            if (!pattern.empty()) {
                lease->patterns.push_back(std::move(pattern));
            }
        }
        for (const auto& pattern : lease->patterns) {
            lease->filters.push_back({pattern.c_str(), pattern.c_str()});
        }
        auto* raw = lease.release();
        if (options.directory) {
            SDL_ShowOpenFolderDialog(&delivered, raw, static_cast<SDL_Window*>(window), nullptr, options.multiple);
        } else {
            SDL_ShowOpenFileDialog(&delivered, raw, static_cast<SDL_Window*>(window),
                                   raw->filters.empty() ? nullptr : raw->filters.data(),
                                   static_cast<int>(raw->filters.size()), nullptr, options.multiple);
        }
    }

    String error() const override {
        auto result = String::from_utf8(SDL_GetError());
        return result ? std::move(result).value() : String{u8"Native file dialog failed"};
    }
};

DialogSdk sdk;

String text(const std::filesystem::path& p) {
    const auto bytes = p.u8string();
    return String::from_utf8(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())).value();
}

std::filesystem::path path(const String& p) {
    const auto b = p.bytes();
    if (b.empty() || b.size() > 1024U * 1024U || b.find(char(0)) != b.npos) {
        throw std::invalid_argument("Invalid native file locator");
    }
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(b.data()), b.size()));
}

String mime(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".png") {
        return String{u8"image/png"};
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return String{u8"image/jpeg"};
    }
    if (ext == ".bmp") {
        return String{u8"image/bmp"};
    }
    return {};
}
} // namespace

struct SdlFileServices::Operation final {
    std::atomic<bool> active{true};
    std::atomic<bool> callback_claimed{false};
    std::weak_ptr<Queue> queue;
    SdlDialogApi* api{};
    FileDialogOptions options;
    FileSelectionCompletion complete;
};

struct SdlFileServices::Worker final {
    std::jthread thread;
    std::shared_ptr<std::atomic<bool>> done;
};

struct SdlFileServices::Queue final {
    std::mutex mutex;
    std::thread::id owner{std::this_thread::get_id()};
    bool alive{true};
    std::function<void()> wake;
    std::deque<std::pair<std::shared_ptr<Operation>, FileSelection>> results;
    std::vector<Worker> workers;

    Queue() {
        workers.reserve(64);
    }

    void push(const std::shared_ptr<Operation>& operation, FileSelection result) {
        std::function<void()> notify;
        {
            std::lock_guard lock(mutex);
            if (!alive || !operation->active.load()) {
                return;
            }
            results.emplace_back(operation, std::move(result));
            notify = wake;
        }
        if (notify) {
            try {
                notify();
            } catch (...) {
            }
        }
    }
};

SdlFileServices::SdlFileServices(SdlDialogApi& api, PlatformWindowHandle window, std::function<void()> wake)
    : api_(&api), window_(window), queue_(std::make_shared<Queue>()) {
    queue_->wake = std::move(wake);
}

SdlFileServices::SdlFileServices(PlatformState& host)
    : SdlFileServices(sdk, host.window(), [id = SDL_GetWindowID(static_cast<SDL_Window*>(host.window()))] {
          SDL_Event event{};
          event.type = SDL_EVENT_WINDOW_EXPOSED;
          event.window.windowID = id;
          SDL_PushEvent(&event);
      }) {}

SdlFileServices::~SdlFileServices() {
    auto q = queue_;
    {
        std::lock_guard lock(q->mutex);
        q->alive = false;
        q->wake = {};
        for (auto& [op, _] : q->results) {
            op->active.store(false);
        }
        q->results.clear();
    }
    if (auto op = dialog_.lock()) {
        op->active.store(false);
    }
    for (auto& worker : q->workers) {
        worker.thread.request_stop();
    }
    q->workers.clear();
}

void SdlFileServices::resolve_async(const std::shared_ptr<Queue>& q, const std::shared_ptr<Operation>& operation,
                                    std::vector<String> paths) {
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::lock_guard lock(q->mutex);
    if (!q->alive || !operation->active.load()) {
        return;
    }
    if (q->workers.size() >= 64) {
        operation->active.store(false);
        throw std::length_error("Native file worker limit exceeded");
    }
    q->workers.push_back(
        {std::jthread(
             [weak = std::weak_ptr<Queue>(q), operation, paths = std::move(paths), done](std::stop_token stop) {
                 FileSelection result;
                 std::size_t retained_bytes{};
                 try {
                     const auto add = [&](const std::filesystem::path& p, const std::filesystem::path& relative) {
                         if (stop.stop_requested() || !operation->active.load()) {
                             return;
                         }
                         if (result.files.size() >= 4096) {
                             throw std::length_error("Selection exceeds 4096 files");
                         }
                         std::error_code error;
                         if (!std::filesystem::is_regular_file(p, error) || error) {
                             throw std::runtime_error("Selected entry is not a readable regular file");
                         }
                         auto file = std::make_shared<NativeFile>();
                         file->name = text(p.filename());
                         file->locator = text(p);
                         file->relative_path = text(relative);
                         file->mime_type = mime(p);
                         file->size = std::filesystem::file_size(p);
                         if (!file->mime_type.empty() && file->size <= 16U * 1024U * 1024U &&
                             file->size <= 64U * 1024U * 1024U - retained_bytes) {
                             std::ifstream input(p, std::ios::binary);
                             auto bytes = std::make_shared<std::vector<std::byte>>();
                             bytes->resize(static_cast<std::size_t>(file->size));
                             std::size_t offset{};
                             while (offset < bytes->size() && !stop.stop_requested() && operation->active.load()) {
                                 const auto chunk = std::min<std::size_t>(65536, bytes->size() - offset);
                                 input.read(reinterpret_cast<char*>(bytes->data() + offset),
                                            static_cast<std::streamsize>(chunk));
                                 if (input.gcount() != static_cast<std::streamsize>(chunk)) {
                                     throw std::runtime_error("Selected image read failed");
                                 }
                                 offset += chunk;
                             }
                             if (offset == bytes->size()) {
                                 retained_bytes += bytes->size();
                                 file->bytes = std::move(bytes);
                             }
                         }
                         result.files.push_back(std::move(file));
                     };
                     for (const auto& locator : paths) {
                         if (stop.stop_requested() || !operation->active.load()) {
                             break;
                         }
                         const auto p = path(locator);
                         if (operation->options.directory && std::filesystem::is_directory(p)) {
                             for (const auto& entry : std::filesystem::recursive_directory_iterator(
                                      p, std::filesystem::directory_options::skip_permission_denied)) {
                                 if (stop.stop_requested() || !operation->active.load()) {
                                     break;
                                 }
                                 if (!entry.is_symlink() && entry.is_regular_file()) {
                                     add(entry.path(), std::filesystem::relative(entry.path(), p.parent_path()));
                                 }
                             }
                         } else {
                             add(p, p.filename());
                         }
                     }
                 } catch (const std::length_error& e) {
                     result = {{}, FileServiceError::Limit, String::from_utf8(e.what()).value()};
                 } catch (const std::exception& e) {
                     result = {{}, FileServiceError::IO, String::from_utf8(e.what()).value()};
                 }
                 if (const auto q = weak.lock(); q && !stop.stop_requested()) {
                     q->push(operation, std::move(result));
                 }
                 done->store(true);
             }),
         done});
}

void SdlFileServices::dialog_result(void* data, const char* const* files, int) {
    std::unique_ptr<std::shared_ptr<Operation>> lease(static_cast<std::shared_ptr<Operation>*>(data));
    const auto op = *lease;
    op->callback_claimed.store(true);
    const auto q = op->queue.lock();
    if (!q || !op->active.load()) {
        return;
    }
    try {
        if (!files) {
            q->push(op, {{}, FileServiceError::Unavailable, op->api->error()});
            return;
        }
        if (!files[0]) {
            q->push(op, {{}, FileServiceError::Cancelled, {}});
            return;
        }
        std::vector<String> paths;
        for (std::size_t i = 0; files[i]; ++i) {
            if (i >= 4096) {
                throw std::length_error("Dialog selection exceeds 4096 paths");
            }
            std::size_t size{};
            while (size <= 1024U * 1024U && files[i][size]) {
                ++size;
            }
            if (size > 1024U * 1024U) {
                throw std::length_error("Dialog path exceeds limit");
            }
            paths.push_back(String::from_utf8({files[i], size}).value());
        }
        resolve_async(q, op, std::move(paths));
    } catch (const std::exception& e) {
        q->push(op, {{}, FileServiceError::InvalidSelection, String::from_utf8(e.what()).value()});
    }
}

FileServiceCancel SdlFileServices::choose(const FileDialogOptions& options, FileSelectionCompletion complete) {
    if (queue_->owner != std::this_thread::get_id()) {
        throw std::logic_error("File chooser requires owner thread");
    }
    if (options.extensions.size() > 128) {
        throw std::length_error("Too many chooser filters");
    }
    for (const auto& ext : options.extensions) {
        const auto value = ext.bytes();
        if (value.empty() || value.size() > 64 || std::ranges::any_of(value, [](unsigned char c) {
                return !(std::isalnum(c) || c == '.' || c == '_' || c == '-');
            })) {
            throw std::invalid_argument("Invalid chooser extension");
        }
    }
    if (auto old = dialog_.lock()) {
        old->active.store(false);
    }
    auto op = std::make_shared<Operation>();
    op->queue = queue_;
    op->api = api_;
    op->options = options;
    op->complete = std::move(complete);
    dialog_ = op;
    auto lease = std::make_unique<std::shared_ptr<Operation>>(op);
    auto* raw = lease.release();
    try {
        api_->show(window_, op->options, &dialog_result, raw);
    } catch (...) {
        if (!op->callback_claimed.load()) {
            delete raw;
        }
        op->active.store(false);
        throw;
    }
    return [op] {
        op->active.store(false);
    };
}

FileServiceCancel SdlFileServices::resolve(const std::vector<String>& paths, bool directory,
                                           FileSelectionCompletion complete) {
    if (queue_->owner != std::this_thread::get_id()) {
        throw std::logic_error("File resolution requires owner thread");
    }
    if (paths.size() > 4096) {
        throw std::length_error("Too many file paths");
    }
    for (const auto& locator : paths) {
        static_cast<void>(path(locator));
    }
    auto op = std::make_shared<Operation>();
    op->queue = queue_;
    op->options.directory = directory;
    op->complete = std::move(complete);
    resolve_async(queue_, op, paths);
    return [op] {
        op->active.store(false);
    };
}

std::size_t SdlFileServices::drain() {
    auto q = queue_;
    if (q->owner != std::this_thread::get_id()) {
        throw std::logic_error("File service drain requires owner thread");
    }
    std::size_t delivered{};
    while (true) {
        std::pair<std::shared_ptr<Operation>, FileSelection> entry;
        {
            std::lock_guard lock(q->mutex);
            if (q->results.empty()) {
                break;
            }
            entry = std::move(q->results.front());
            q->results.pop_front();
        }
        auto& [op, result] = entry;
        if (op->active.exchange(false)) {
            const auto callback = op->complete;
            if (callback) {
                callback(std::move(result));
                ++delivered;
            }
        }
    }
    {
        std::lock_guard lock(q->mutex);
        std::erase_if(q->workers, [](const auto& w) { return w.done->load(); });
    }
    return delivered;
}

FileServiceCancel SdlFileServices::paste(FileSelectionCompletion complete) {
    if (queue_->owner != std::this_thread::get_id()) {
        throw std::logic_error("File clipboard requires owner thread");
    }
    if (!SDL_HasClipboardData("text/uri-list")) {
        complete({{}, FileServiceError::Unavailable, String{u8"No native file clipboard data"}});
        return {};
    }
    std::size_t size{};
    std::unique_ptr<void, decltype(&SDL_free)> bytes(SDL_GetClipboardData("text/uri-list", &size), &SDL_free);
    if (!bytes || !size || size > 4U * 1024U * 1024U) {
        complete({{}, FileServiceError::InvalidSelection, String{u8"Invalid file clipboard payload"}});
        return {};
    }
    try {
        std::vector<String> paths;
        std::string_view remaining(static_cast<const char*>(bytes.get()), size);
        while (!remaining.empty() && remaining.back() == 0) {
            remaining.remove_suffix(1);
        }
        while (!remaining.empty()) {
            const auto end = remaining.find_first_of("\r\n\0");
            auto line = remaining.substr(0, end);
            if (!line.empty() && line[0] != '#') {
                if (line.starts_with("file://localhost/")) {
                    line.remove_prefix(16);
                } else if (line.starts_with("file:///")) {
                    line.remove_prefix(7);
                } else {
                    throw std::invalid_argument("File clipboard contains a nonlocal URI");
                }
                std::string decoded;
                for (std::size_t i = 0; i < line.size(); ++i) {
                    if (line[i] == '%') {
                        if (i + 2 >= line.size()) {
                            throw std::invalid_argument("Truncated URI escape");
                        }
                        unsigned value{};
                        const auto parsed = std::from_chars(line.data() + i + 1, line.data() + i + 3, value, 16);
                        if (parsed.ec != std::errc{} || parsed.ptr != line.data() + i + 3 || value == 0) {
                            throw std::invalid_argument("Invalid URI escape");
                        }
                        decoded.push_back(static_cast<char>(value));
                        i += 2;
                    } else {
                        if (line[i] == 0) {
                            throw std::invalid_argument("NUL in file URI");
                        }
                        decoded.push_back(line[i]);
                    }
                }
#ifdef _WIN32
                if (decoded.size() > 3 && decoded[0] == '/' && decoded[2] == ':') {
                    decoded.erase(0, 1);
                }
#endif
                paths.push_back(String::from_utf8(decoded).value());
                if (paths.size() > 4096) {
                    throw std::length_error("Clipboard selection limit exceeded");
                }
            }
            if (end == remaining.npos) {
                break;
            }
            remaining.remove_prefix(end + 1);
        }
        if (paths.empty()) {
            complete({{}, FileServiceError::Cancelled, {}});
            return {};
        }
        return resolve(paths, false, std::move(complete));
    } catch (const std::exception& e) {
        complete({{}, FileServiceError::InvalidSelection, String::from_utf8(e.what()).value()});
        return {};
    }
}
} // namespace ryn::detail
