#include "platform/sdl/file_services.hpp"
#include <condition_variable>
#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace ryn;

namespace {
int checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

class MockDialog final : public detail::SdlDialogApi {
public:
    struct Call final {
        Callback callback;
        void* data;
        FileDialogOptions options;
        detail::PlatformWindowHandle window;
    };

    std::vector<Call> calls;

    void show(detail::PlatformWindowHandle window, const FileDialogOptions& options, Callback callback,
              void* data) override {
        calls.push_back({callback, data, options, window});
    }

    String error() const override {
        return String{u8"mock SDK failure"};
    }

    void deliver(std::size_t index, const char* const* files) {
        auto call = calls.at(index);
        call.callback(call.data, files, 0);
    }
};

struct Wake final {
    std::mutex mutex;
    std::condition_variable cv;
    std::size_t count{};

    void signal() {
        std::lock_guard lock(mutex);
        ++count;
        cv.notify_all();
    }

    void await(std::size_t target) {
        std::unique_lock lock(mutex);
        check(cv.wait_for(lock, std::chrono::seconds(3), [&] { return count >= target; }),
              "Native worker did not wake owner");
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--clipboard") {
            const auto* display = std::getenv("WAYLAND_DISPLAY");
            check(display && std::string_view(display) == "rynui-test" && !std::getenv("DISPLAY"),
                  "Clipboard fixture requires run_background_wayland.py");
            detail::PlatformConfig config;
            config.title = "RynUI private file clipboard fixture";
            auto host = detail::PlatformState::create(config);
            check(bool(host), "Private SDL window failed");
            MockDialog sdk;
            Wake event;
            detail::SdlFileServices files(sdk, host.state->window(), [&] { event.signal(); });
            std::string payload = "file://localhost" + std::string(RYNUI_IMAGE_FIXTURES) + "/%63olors.png\r\n";
            const char* mime[]{"text/uri-list"};
            check(SDL_SetClipboardData(
                      [](void* data, const char*, std::size_t* size) -> const void* {
                          const auto& text = *static_cast<std::string*>(data);
                          *size = text.size();
                          return text.data();
                      },
                      nullptr, &payload, mime, 1),
                  "SDL own MIME clipboard setup failed");
            FileSelection selection;
            bool complete{};
            files.paste([&](FileSelection value) {
                selection = std::move(value);
                complete = true;
                return true;
            });
            event.await(1);
            files.drain();
            check(complete && selection.error == FileServiceError::None && selection.files.size() == 1 &&
                      selection.files[0]->bytes,
                  "Native file MIME paste did not resolve fixture");
            check(sdk.calls.empty(), "Clipboard fixture opened a chooser");
            check(SDL_ClearClipboardData(), "SDL clipboard cleanup failed");
            std::cout << checks << " private SDL own-MIME clipboard checks passed; no chooser invoked\n";
            return 0;
        }
        MockDialog sdk;
        Wake wake;
        std::size_t delivered{};
        FileSelection result;
        auto owner = std::this_thread::get_id();
        {
            detail::SdlFileServices files(sdk, reinterpret_cast<void*>(7), [&] { wake.signal(); });
            const FileSelectionCompletion done = [&](FileSelection v) {
                check(std::this_thread::get_id() == owner, "SDK delivered callback off owner");
                ++delivered;
                result = std::move(v);
                return true;
            };
            auto cancel = files.choose({true, false, {String{u8".png"}}}, done);
            check(sdk.calls.size() == 1 && sdk.calls[0].window == reinterpret_cast<void*>(7) &&
                      sdk.calls[0].options.multiple,
                  "SDK options/window lost");
            auto path = std::string(RYNUI_IMAGE_FIXTURES) + "/colors.png";
            const char* paths[]{path.c_str(), nullptr};
            std::thread sdk_thread([&] { sdk.deliver(0, paths); });
            sdk_thread.join();
            path.assign("not the selected path");
            wake.await(1);
            check(delivered == 0, "Foreign callback mutated Core before drain");
            check(files.drain() == 1 && delivered == 1, "Owner result not delivered");
            check(result.error == FileServiceError::None && result.files.size() == 1 && result.files[0]->bytes &&
                      result.files[0]->name == String{u8"colors.png"},
                  "Owned file snapshot/pixels missing");
            cancel();
            check(files.drain() == 0, "Completed result duplicated");
            files.choose({}, done);
            const char* empty[]{nullptr};
            sdk.deliver(1, empty);
            files.drain();
            check(result.error == FileServiceError::Cancelled, "SDK cancel lost");
            files.choose({}, done);
            sdk.deliver(2, nullptr);
            files.drain();
            check(result.error == FileServiceError::Unavailable, "SDK failure lost");
            auto retired = files.choose({}, done);
            retired();
            sdk.deliver(3, empty);
            check(files.drain() == 0, "Cancelled chooser delivered");
            files.choose({}, done);
            files.choose({}, done);
            sdk.deliver(4, empty);
            check(files.drain() == 0, "Later chooser accepted old generation");
            sdk.deliver(5, empty);
            check(files.drain() == 1, "New chooser callback lost");
            bool foreign_rejected{};
            std::thread wrong([&] {
                try {
                    files.choose({}, done);
                } catch (const std::logic_error&) {
                    foreign_rejected = true;
                }
            });
            wrong.join();
            check(foreign_rejected, "Foreign choose accepted");
            bool invalid{};
            try {
                files.choose({false, false, {String{u8"../bad;*"}}}, done);
            } catch (const std::invalid_argument&) {
                invalid = true;
            }
            check(invalid, "Invalid extension reached SDK");
            auto resolve = files.resolve({String::from_utf8(std::string(RYNUI_IMAGE_FIXTURES) + "/colors.jpg").value()},
                                         false, done);
            resolve();
            files.choose({}, done); // Deliberately deliver after the owner is destroyed below.
        }
        const char* empty[]{nullptr};
        sdk.deliver(6, empty);
        check(delivered == 4, "Destroyed native owner delivered callback");
        {
            Wake event;
            detail::SdlFileServices files(sdk, nullptr, [&] { event.signal(); });
            FileSelection directory;
            bool complete{};
            files.resolve({String::from_utf8(RYNUI_IMAGE_FIXTURES).value()}, true, [&](FileSelection value) {
                directory = std::move(value);
                complete = true;
                return true;
            });
            event.await(1);
            files.drain();
            check(complete && directory.error == FileServiceError::None && directory.files.size() == 3,
                  "Directory resolution failed");
            for (const auto& file : directory.files) {
                check(file->bytes && !file->relative_path.empty(), "Directory snapshot lacks bytes/path");
            }
            FileSelection error;
            files.resolve({String::from_utf8(std::string(RYNUI_IMAGE_FIXTURES) + "/missing.png").value()}, false,
                          [&](FileSelection value) {
                              error = std::move(value);
                              return true;
                          });
            event.await(2);
            files.drain();
            check(error.error == FileServiceError::IO, "Missing native source did not report IO failure");
            bool invalid{};
            try {
                files.resolve({String{}}, false, {});
            } catch (const std::invalid_argument&) {
                invalid = true;
            }
            check(invalid, "Invalid native locator reached worker");
        }
        std::cout << checks << " native SDK seam checks passed; no OS chooser invoked\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
