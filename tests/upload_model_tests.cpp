#include "component/upload_model.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>
using namespace ryn;
using namespace ryn::detail;

namespace {
int checks{};

void check(bool v) {
    ++checks;
    if (!v) {
        throw std::runtime_error("Upload model check " + std::to_string(checks));
    }
}

std::shared_ptr<const NativeFile> file(const char8_t* name = u8"fixture.png", const char8_t* type = u8"image/png") {
    auto f = std::make_shared<NativeFile>();
    f->name = String::from_utf8(std::u8string_view(name)).value();
    f->mime_type = String::from_utf8(std::u8string_view(type)).value();
    f->size = 4;
    f->bytes = std::make_shared<const std::vector<std::byte>>(4, std::byte{42});
    return f;
}

UploadFile existing(String id, std::shared_ptr<const NativeFile> source) {
    UploadFile f;
    f.uid = id;
    f.origin = std::move(source);
    f.name = f.origin->name;
    f.type = f.origin->mime_type;
    return f;
}

template <class F> void reject(F f) {
    bool threw{};
    try {
        f();
    } catch (...) {
        threw = true;
    }
    check(threw);
}

void run(std::string mode) {
    if (mode == "accept") {
        const auto accept = upload_accept(String{u8".PNG,image/jpeg,text/*"}.view());
        check(upload_accepts(accept, *file(u8"a.PNG")));
        check(upload_accepts(accept, *file(u8"a.unknown", u8"image/jpeg")));
        check(upload_accepts(accept, *file(u8"x", u8"text/plain")));
        check(!upload_accepts(accept, *file(u8"x.exe", u8"application/octet-stream")));
        reject([] { upload_accept(String{u8"bad"}.view()); });
    } else if (mode == "multiple") {
        UploadModel model;
        UploadModelOptions o;
        o.multiple = true;
        o.max_count = 2;
        model.configure(o);
        check(model.select({file(), file(), file()}));
        check(model.files().size() == 2);
        const auto a = model.files()[0].uid;
        model.set_options({}, true, false, 1);
        check(model.select({file(u8"replace.png")}));
        check(model.files().size() == 1 && model.files()[0].uid != a);
        model.set_options({}, false, false, {});
        check(model.select({file(), file()}));
        check(model.files().size() == 2);
    } else if (mode == "before") {
        UploadModel model;
        UploadModelOptions o;
        o.multiple = true;
        int before{};
        int sent{};
        o.before = [&](const UploadFile& f, const UploadFiles&, UploadBeforeCompletion done) {
            ++before;
            done({f.name == String{u8"ignore.png"} ? UploadBeforeDecision::Ignore : UploadBeforeDecision::Keep, {}});
            return UploadCancel{};
        };
        o.transport = [&](const UploadRequest&) {
            ++sent;
            return UploadCancel{};
        };
        model.configure(o);
        model.select({file(u8"ignore.png"), file(u8"keep.png")});
        check(before == 2 && model.files().size() == 1 && sent == 0 &&
              model.files()[0].status == UploadFileStatus::Ready);
        check(model.retry(model.files()[0].uid) && sent == 1);
    } else if (mode == "async_before") {
        UploadModel model;
        UploadModelOptions o;
        o.multiple = true;
        std::vector<UploadBeforeCompletion> gates;
        int cancels{};
        o.before = [&](const UploadFile&, const UploadFiles&, UploadBeforeCompletion done) {
            gates.push_back(done);
            return [&] {
                ++cancels;
            };
        };
        model.configure(o);
        model.select({file(u8"first.png"), file(u8"second.png")});
        check(model.files().empty());
        bool foreign = true;
        std::thread t([&] { foreign = gates[0]({}); });
        t.join();
        check(!foreign);
        check(gates[1]({}));
        check(model.files().empty());
        check(!gates[1]({}));
        check(gates[0]({}));
        check(model.files().size() == 2 && model.files()[0].name == String{u8"first.png"});
        model.select({file()});
        model.retire();
        check(cancels == 1 && !gates.back()({}) && model.pending_count() == 0);
    } else if (mode == "transport") {
        UploadModel model;
        UploadModelOptions o;
        std::vector<UploadRequest> requests;
        int aborts{};
        o.transport = [&](const UploadRequest& r) {
            requests.push_back(r);
            return [&] {
                ++aborts;
            };
        };
        model.configure(o);
        model.select({file()});
        check(requests.size() == 1 && model.files()[0].status == UploadFileStatus::Uploading);
        check(requests[0].progress(25));
        check(!requests[0].progress(25));
        check(!requests[0].progress(5));
        check(!requests[0].progress(std::numeric_limits<float>::quiet_NaN()));
        bool foreign = true;
        std::thread t([&] { foreign = requests[0].success({String{u8"bad"}, {}}); });
        t.join();
        check(!foreign);
        check(requests[0].failure(String{u8"real failure"}));
        check(!requests[0].success({}));
        check(model.files()[0].status == UploadFileStatus::Error);
        const auto id = model.files()[0].uid;
        check(model.retry(id) && requests.size() == 2);
        check(requests[1].success({String{u8"server response"}, String{u8"app://resource"}}));
        check(model.files()[0].response == String{u8"server response"} && model.files()[0].percent == 100 &&
              aborts == 0);
        check(!requests[1].failure({}));
    } else if (mode == "controlled") {
        UploadModel model;
        UploadModelOptions o;
        o.controlled = true;
        int sent{};
        std::vector<UploadChange> changes;
        o.on_change = [&](const UploadChange& c) {
            changes.push_back(c);
        };
        o.transport = [&](const UploadRequest&) {
            ++sent;
            return UploadCancel{};
        };
        model.configure(o);
        model.select({file()});
        check(model.files().empty() && changes.size() == 1 && sent == 0);
        o.on_change = [&](const UploadChange& c) {
            changes.push_back(c);
            model.set_files(c.file_list);
        };
        model.configure(o);
        model.select({file()});
        check(model.files().size() == 1 && sent == 1);
        auto list = model.files();
        const auto uid = list[0].uid;
        model.set_files({});
        check(!model.abort(uid));
    } else if (mode == "remove") {
        UploadModel model;
        UploadModelOptions o;
        UploadRemoveCompletion complete;
        o.remove = [&](const UploadFile&, UploadRemoveCompletion done) {
            complete = done;
            return UploadCancel{};
        };
        model.configure(o);
        model.select({file()});
        const auto uid = model.files()[0].uid;
        check(model.remove(uid));
        check(!model.remove(uid));
        check(complete(false));
        check(model.files().size() == 1);
        check(!complete(true));
        check(model.remove(uid));
        check(complete(true));
        check(model.files().empty());
    } else if (mode == "cancel") {
        UploadModel model;
        UploadModelOptions o;
        o.multiple = true;
        std::vector<UploadRequest> requests;
        int aborts{};
        o.transport = [&](const UploadRequest& r) {
            requests.push_back(r);
            return [&] {
                ++aborts;
            };
        };
        model.configure(o);
        model.select({file(), file()});
        check(model.abort(model.files()[0].uid));
        check(aborts == 1 && !requests[0].progress(50));
        model.set_enabled(false);
        check(aborts == 2 && model.pending_count() == 0 && !requests[1].success({}));
        check(!model.select({file()}));
    } else if (mode == "reentrant") {
        UploadModel model;
        UploadModelOptions o;
        o.transport = [&](const UploadRequest& r) {
            r.progress(10);
            model.set_files({});
            return UploadCancel{};
        };
        model.configure(o);
        model.select({file()});
        check(model.files().empty() && model.pending_count() == 0);
        UploadModel dying;
        UploadModelOptions d;
        d.on_change = [&](const UploadChange&) {
            dying.retire();
        };
        dying.configure(d);
        dying.select({file()});
        check(dying.pending_count() == 0);
        UploadModel throwing;
        UploadModelOptions e;
        e.before = [](const UploadFile&, const UploadFiles&, UploadBeforeCompletion) -> UploadCancel {
            throw std::runtime_error("predicate");
        };
        throwing.configure(e);
        reject([&] { throwing.select({file()}); });
        check(throwing.pending_count() == 0);
    } else if (mode == "validation") {
        UploadModel model;
        auto a = existing(String{u8"a"}, file());
        reject([&] { model.set_files({a, a}); });
        check(model.files().empty());
        reject([&] { model.select({file(), {}}); });
        check(model.pending_count() == 0);
        UploadModelOptions o;
        o.max_count = 0;
        reject([&] { model.configure(o); });
    } else {
        throw std::runtime_error("Unknown mode");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        run(argc > 1 ? argv[1] : "accept");
        std::cout << checks << " Upload model checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
