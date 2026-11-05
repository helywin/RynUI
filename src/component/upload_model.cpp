#include "component/upload_model.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <thread>
#include <stdexcept>

namespace ryn {
namespace {
std::string lower(std::string_view text) {
    std::string value(text);
    for (auto& c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = char(c - 'A' + 'a');
        }
    }
    return value;
}

std::string trim(std::string_view text) {
    const auto a = text.find_first_not_of(" \t\r\n");
    if (a == text.npos) {
        return {};
    }
    const auto b = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(a, b - a + 1));
}

void validate_accept(const UploadAccept& accept) {
    if (accept.size() > 128) {
        throw std::length_error("Upload accept rule limit exceeded");
    }
    for (const auto& rule : accept) {
        const auto mime = rule.mime_type.bytes();
        if (!mime.empty() && (mime.find('/') == mime.npos || mime.find_first_of(" ,;\r\n") != mime.npos)) {
            throw std::invalid_argument("Invalid Upload MIME accept rule");
        }
        for (const auto& ext : rule.extensions) {
            if (ext.empty() || ext.bytes()[0] != '.' || ext.bytes().find_first_of("/\\,; \r\n") != ext.bytes().npos) {
                throw std::invalid_argument("Invalid Upload extension accept rule");
            }
        }
    }
}
} // namespace

UploadAccept upload_accept(StringView csv) {
    UploadAccept result;
    auto remaining = csv.bytes();
    while (!remaining.empty()) {
        const auto comma = remaining.find(',');
        const auto token = lower(trim(remaining.substr(0, comma)));
        if (!token.empty()) {
            const auto text = String::from_utf8(token).value();
            if (token[0] == '.') {
                result.push_back({{}, {text}});
            } else {
                result.push_back({text, {}});
            }
        }
        if (comma == remaining.npos) {
            break;
        }
        remaining.remove_prefix(comma + 1);
    }
    validate_accept(result);
    return result;
}

bool upload_accepts(const UploadAccept& accept, const NativeFile& file) {
    validate_accept(accept);
    if (accept.empty()) {
        return true;
    }
    const auto name = lower(file.name.bytes());
    const auto mime = lower(file.mime_type.bytes());
    for (const auto& rule : accept) {
        bool matched = false;
        const auto type = lower(rule.mime_type.bytes());
        if (!type.empty()) {
            matched = type == mime || (type.ends_with("/*") && mime.starts_with(type.substr(0, type.size() - 1)));
        }
        for (const auto& ext : rule.extensions) {
            matched |= name.ends_with(lower(ext.bytes()));
        }
        if (matched) {
            return true;
        }
    }
    return false;
}
} // namespace ryn

namespace ryn::detail {
namespace {
std::atomic<std::uint64_t> next_upload_owner{1};

void cancel(UploadCancel value) noexcept {
    if (value) {
        try {
            value();
        } catch (...) {
        }
    }
}

std::string uid(const UploadUid& value) {
    return std::string(value.bytes());
}
} // namespace

struct UploadModel::State final : std::enable_shared_from_this<State> {
    struct Job final {
        UploadFile file;
        bool active{true};
        bool ready{};
        bool cancelled{};
        bool ignored{};
        bool keep{};
        bool inserted{};
        std::uint64_t serial{};
        float progress{};
        UploadCancel cancel;
    };

    UploadModelOptions options;
    UploadFiles files;
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t owner_id{next_upload_owner.fetch_add(1)};
    std::uint64_t next_uid{};
    bool alive{true};
    bool flushing{};
    std::deque<std::shared_ptr<Job>> before;
    std::map<std::string, std::shared_ptr<Job>> requests;
    std::map<std::string, std::shared_ptr<Job>> removals;

    void require_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("UploadModel requires owner thread");
        }
    }

    auto find(const UploadUid& id) {
        return std::ranges::find_if(files, [&](const auto& f) { return f.uid == id; });
    }

    bool current(const std::shared_ptr<Job>& job) {
        const auto f = find(job->file.uid);
        return alive && job->active && f != files.end() && f->origin == job->file.origin;
    }

    void stop(const std::shared_ptr<Job>& job) noexcept {
        job->active = false;
        job->cancelled = true;
        ++job->serial;
        auto fn = std::move(job->cancel);
        cancel(std::move(fn));
    }

    void retire() noexcept {
        if (!alive) {
            return;
        }
        alive = false;
        while (!before.empty()) {
            auto job = before.front();
            before.pop_front();
            stop(job);
        }
        while (!requests.empty()) {
            auto job = requests.begin()->second;
            requests.erase(requests.begin());
            stop(job);
        }
        while (!removals.empty()) {
            auto job = removals.begin()->second;
            removals.erase(removals.begin());
            stop(job);
        }
        options.on_change = {};
        options.changed = {};
    }

    void notify() {
        const auto fn = options.changed;
        if (fn && alive) {
            try {
                fn();
            } catch (...) {
                retire();
                throw;
            }
        }
    }

    void emit(UploadFile file, UploadFiles proposed, UploadChangeReason reason) {
        if (!alive) {
            return;
        }
        if (!options.controlled) {
            set_files(std::move(proposed));
            proposed = files;
        }
        const auto callback = options.on_change;
        if (callback && alive) {
            try {
                callback({std::move(file), std::move(proposed), reason});
            } catch (...) {
                retire();
                throw;
            }
        }
    }

    void set_files(UploadFiles value) {
        UploadModel::validate_files(value);
        files = std::move(value);
        std::vector<std::shared_ptr<Job>> cancelled;
        for (auto it = requests.begin(); it != requests.end();) {
            if (!current(it->second)) {
                cancelled.push_back(it->second);
                it = requests.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = removals.begin(); it != removals.end();) {
            if (!current(it->second)) {
                cancelled.push_back(it->second);
                it = removals.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& j : cancelled) {
            stop(j);
        }
        notify();
    }

    void replace(UploadFile file, UploadChangeReason reason) {
        if (!alive) {
            return;
        }
        auto proposed = files;
        const auto it = std::ranges::find_if(proposed, [&](const auto& f) { return f.uid == file.uid; });
        if (it == proposed.end()) {
            return;
        }
        *it = file;
        emit(std::move(file), std::move(proposed), reason);
    }

    bool report(const std::shared_ptr<Job>& job, std::uint64_t serial, UploadChangeReason reason, float percent,
                UploadResult result, String error) {
        if (owner != std::this_thread::get_id() || !current(job) || job->serial != serial) {
            return false;
        }
        const auto current_file = *find(job->file.uid);
        auto next = current_file;
        if (reason == UploadChangeReason::Progress) {
            if (!std::isfinite(percent) || percent < 0 || percent > 100 || percent <= job->progress) {
                return false;
            }
            job->progress = percent;
            next.percent = percent;
            next.status = UploadFileStatus::Uploading;
        } else {
            job->active = false;
            requests.erase(uid(job->file.uid));
            job->cancel = {};
            next.status = reason == UploadChangeReason::Success ? UploadFileStatus::Done : UploadFileStatus::Error;
            next.percent = reason == UploadChangeReason::Success ? 100 : job->progress;
            next.response = std::move(result.response);
            next.url = std::move(result.url);
            next.error = std::move(error);
        }
        replace(std::move(next), reason);
        return true;
    }

    void start(UploadFile file) {
        if (!alive || !options.enabled || !file.origin || !options.transport || requests.contains(uid(file.uid))) {
            return;
        }
        auto job = std::make_shared<Job>();
        job->file = file;
        job->inserted = true;
        job->serial = 1;
        requests.emplace(uid(file.uid), job);
        file.status = UploadFileStatus::Uploading;
        file.percent = 0;
        file.error = {};
        file.response = {};
        replace(file, UploadChangeReason::Start);
        if (!current(job)) {
            requests.erase(uid(file.uid));
            stop(job);
            return;
        }
        const std::weak_ptr<State> weak = shared_from_this();
        const std::weak_ptr<Job> weak_job = job;
        const auto serial = job->serial;
        const auto report = [weak, weak_job, serial, thread = owner](UploadChangeReason reason, float p, UploadResult r,
                                                                     String error) {
            if (thread != std::this_thread::get_id()) {
                return false;
            }
            const auto s = weak.lock();
            const auto j = weak_job.lock();
            return s && j && s->report(j, serial, reason, p, std::move(r), std::move(error));
        };
        UploadRequest request{
            file, [report](float p) { return report(UploadChangeReason::Progress, p, {}, {}); },
            [report](UploadResult r) { return report(UploadChangeReason::Success, 100, std::move(r), {}); },
            [report](String e) { return report(UploadChangeReason::Failure, 0, {}, std::move(e)); }};
        try {
            const auto transport = options.transport;
            auto fn = transport(request);
            if (current(job)) {
                job->cancel = std::move(fn);
            } else if (job->cancelled) {
                cancel(std::move(fn));
            }
        } catch (const std::exception& e) {
            if (current(job)) {
                report(UploadChangeReason::Failure, 0, {}, String::from_utf8(e.what()).value());
            } else if (!alive) {
                throw;
            }
        } catch (...) {
            if (current(job)) {
                report(UploadChangeReason::Failure, 0, {}, String{u8"Transport threw"});
            } else if (!alive) {
                throw;
            }
        }
    }

    void flush() {
        if (flushing || !alive) {
            return;
        }
        flushing = true;
        try {
            while (alive && !before.empty() && before.front()->ready) {
                auto job = before.front();
                before.pop_front();
                if (!job->active || job->ignored) {
                    job->active = false;
                    continue;
                }
                auto file = job->file;
                auto proposed = files;
                if (options.max_count && *options.max_count == 1) {
                    proposed.clear();
                }
                if (options.max_count && proposed.size() >= *options.max_count) {
                    job->active = false;
                    continue;
                }
                proposed.push_back(file);
                job->active = false;
                emit(file, std::move(proposed), UploadChangeReason::Selection);
                if (alive && options.enabled && !job->keep) {
                    const auto it = find(file.uid);
                    if (it != files.end() && it->origin == file.origin) {
                        start(*it);
                    }
                }
            }
        } catch (...) {
            flushing = false;
            throw;
        }
        flushing = false;
    }

    void gate(const std::shared_ptr<Job>& job, bool transform) {
        if (!alive || !job->active) {
            return;
        }
        const auto hook = transform ? options.transform : options.before;
        if (!hook) {
            if (!transform && options.transform) {
                gate(job, true);
            } else {
                job->ready = true;
                flush();
            }
            return;
        }
        const auto serial = ++job->serial;
        const std::weak_ptr<State> weak = shared_from_this();
        const std::weak_ptr<Job> weak_job = job;
        UploadBeforeCompletion done = [weak, weak_job, serial, transform, thread = owner](UploadBeforeResult result) {
            if (thread != std::this_thread::get_id()) {
                return false;
            }
            const auto s = weak.lock();
            const auto j = weak_job.lock();
            if (!s || !j || s->owner != std::this_thread::get_id() || !s->alive || !j->active || j->serial != serial) {
                return false;
            }
            ++j->serial;
            j->cancel = {};
            if (result.decision < UploadBeforeDecision::Upload || result.decision > UploadBeforeDecision::Ignore) {
                s->retire();
                throw std::invalid_argument("Invalid beforeUpload decision");
            }
            if (result.transformed) {
                if (result.transformed->name.empty()) {
                    s->retire();
                    throw std::invalid_argument("Invalid transformed file");
                }
                j->file.origin = std::move(result.transformed);
                j->file.name = j->file.origin->name;
                j->file.type = j->file.origin->mime_type;
                j->file.size = j->file.origin->size;
            }
            if (result.decision == UploadBeforeDecision::Ignore) {
                j->ignored = true;
                j->ready = true;
                s->flush();
            } else if (result.decision == UploadBeforeDecision::Keep) {
                j->keep = true;
                j->ready = true;
                s->flush();
            } else if (!transform && s->options.transform) {
                s->gate(j, true);
            } else {
                j->ready = true;
                s->flush();
            }
            return true;
        };
        try {
            auto fn = hook(job->file, files, done);
            if (alive && job->active && job->serial == serial) {
                job->cancel = std::move(fn);
            } else if (job->cancelled) {
                cancel(std::move(fn));
            }
        } catch (...) {
            retire();
            throw;
        }
    }
};

UploadModel::UploadModel() : state_(std::make_shared<State>()) {}

UploadModel::~UploadModel() {
    retire();
}

void UploadModel::configure(UploadModelOptions options) {
    state_->require_owner();
    validate_accept(options.accept);
    if (options.max_count && (*options.max_count == 0 || *options.max_count > 4096)) {
        throw std::invalid_argument("Invalid Upload maxCount");
    }
    state_->options = std::move(options);
}

void UploadModel::validate_files(const UploadFiles& files) {
    if (files.size() > 4096) {
        throw std::length_error("Upload file list limit exceeded");
    }
    std::set<std::string> keys;
    for (const auto& f : files) {
        if (f.uid.empty() || f.name.empty() || !keys.insert(uid(f.uid)).second || f.status < UploadFileStatus::Ready ||
            f.status > UploadFileStatus::Cancelled || !std::isfinite(f.percent) || f.percent < 0 || f.percent > 100) {
            throw std::invalid_argument("Upload requires unique stable UIDs and finite status/progress");
        }
    }
}

void UploadModel::set_files(UploadFiles files) {
    const auto s = state_;
    s->require_owner();
    if (s->alive) {
        s->set_files(std::move(files));
    }
}

void UploadModel::set_options(UploadAccept accept, bool multiple, bool directory, std::optional<std::size_t> max) {
    const auto s = state_;
    s->require_owner();
    validate_accept(accept);
    if (max && (!*max || *max > 4096)) {
        throw std::invalid_argument("Invalid Upload maxCount");
    }
    s->options.accept = std::move(accept);
    s->options.multiple = multiple;
    s->options.directory = directory;
    s->options.max_count = max;
}

void UploadModel::set_enabled(bool enabled) {
    const auto s = state_;
    s->require_owner();
    if (!s->alive || s->options.enabled == enabled) {
        return;
    }
    s->options.enabled = enabled;
    if (!enabled) {
        const auto before = s->before;
        s->before.clear();
        for (const auto& j : before) {
            s->stop(j);
        }
        const auto removals = s->removals;
        s->removals.clear();
        for (const auto& [_, j] : removals) {
            s->stop(j);
        }
        const auto jobs = s->requests;
        s->requests.clear();
        for (const auto& [_, j] : jobs) {
            s->stop(j);
            if (!s->alive) {
                break;
            }
            auto it = s->find(j->file.uid);
            if (it != s->files.end()) {
                auto file = *it;
                file.status = UploadFileStatus::Cancelled;
                s->replace(file, UploadChangeReason::Cancel);
            }
        }
    }
}

bool UploadModel::select(NativeFiles files) {
    const auto s = state_;
    s->require_owner();
    if (!s->alive || !s->options.enabled) {
        return false;
    }
    if (files.size() > 4096) {
        throw std::length_error("Upload selection exceeds 4096 files");
    }
    for (const auto& f : files) {
        if (!f || f->name.empty()) {
            throw std::invalid_argument("Invalid native file selection");
        }
    }
    std::vector<std::shared_ptr<State::Job>> added;
    for (const auto& file : files) {
        if (!file || file->name.empty()) {
            throw std::invalid_argument("Invalid native file selection");
        }
        if (!upload_accepts(s->options.accept, *file)) {
            continue;
        }
        if (!s->options.multiple && !s->options.directory && !added.empty()) {
            break;
        }
        auto j = std::make_shared<State::Job>();
        j->file.uid =
            String::from_utf8("ryn-upload-" + std::to_string(s->owner_id) + "-" + std::to_string(++s->next_uid))
                .value();
        j->file.name = file->name;
        j->file.type = file->mime_type;
        j->file.size = file->size;
        j->file.origin = file;
        s->before.push_back(j);
        added.push_back(j);
    }
    for (const auto& j : added) {
        if (!s->alive) {
            break;
        }
        s->gate(j, false);
    }
    return !added.empty();
}

bool UploadModel::retry(const UploadUid& id) {
    const auto s = state_;
    s->require_owner();
    if (!s->alive || !s->options.enabled || s->requests.contains(uid(id))) {
        return false;
    }
    const auto f = s->find(id);
    if (f == s->files.end() || !f->origin || !s->options.transport) {
        return false;
    }
    const auto file = *f;
    s->start(file);
    return true;
}

bool UploadModel::abort(const UploadUid& id) {
    const auto s = state_;
    s->require_owner();
    auto it = s->requests.find(uid(id));
    if (!s->alive) {
        return false;
    }
    if (it == s->requests.end()) {
        const auto pending =
            std::ranges::find_if(s->before, [&](const auto& j) { return j->file.uid == id && j->active; });
        if (pending == s->before.end()) {
            return false;
        }
        const auto j = *pending;
        s->stop(j);
        j->ready = true;
        j->ignored = true;
        s->flush();
        return true;
    }
    auto j = it->second;
    s->requests.erase(it);
    s->stop(j);
    if (s->alive) {
        const auto it = s->find(id);
        if (it != s->files.end()) {
            auto file = *it;
            file.status = UploadFileStatus::Cancelled;
            s->replace(file, UploadChangeReason::Cancel);
        }
    }
    return true;
}

bool UploadModel::remove(const UploadUid& id) {
    const auto s = state_;
    s->require_owner();
    if (!s->alive || !s->options.enabled || s->removals.contains(uid(id))) {
        return false;
    }
    const auto file = s->find(id);
    if (file == s->files.end()) {
        return false;
    }
    auto job = std::make_shared<State::Job>();
    job->file = *file;
    s->removals.emplace(uid(id), job);
    const std::weak_ptr<State> weak = s;
    const std::weak_ptr<State::Job> weak_job = job;
    UploadRemoveCompletion done = [weak, weak_job, thread = s->owner](bool allowed) {
        if (thread != std::this_thread::get_id()) {
            return false;
        }
        const auto s = weak.lock();
        const auto j = weak_job.lock();
        if (!s || !j || s->owner != std::this_thread::get_id() || !s->current(j)) {
            return false;
        }
        j->active = false;
        s->removals.erase(uid(j->file.uid));
        j->cancel = {};
        if (allowed) {
            auto removed = *s->find(j->file.uid);
            removed.status = UploadFileStatus::Removed;
            auto files = s->files;
            std::erase_if(files, [&](const auto& f) { return f.uid == removed.uid; });
            s->emit(removed, std::move(files), UploadChangeReason::Remove);
        }
        return true;
    };
    if (s->options.remove) {
        try {
            auto fn = s->options.remove(job->file, done);
            if (s->current(job)) {
                job->cancel = std::move(fn);
            } else if (job->cancelled) {
                cancel(std::move(fn));
            }
        } catch (...) {
            if (job->active) {
                job->active = false;
                s->removals.erase(uid(id));
            }
            throw;
        }
    } else {
        done(true);
    }
    return true;
}

void UploadModel::retire() noexcept {
    state_->retire();
}

UploadFiles UploadModel::files() const {
    state_->require_owner();
    return state_->files;
}

std::size_t UploadModel::pending_count() const {
    state_->require_owner();
    return state_->requests.size() + state_->before.size() + state_->removals.size();
}
} // namespace ryn::detail
