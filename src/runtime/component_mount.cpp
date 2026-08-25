#include "runtime/component_mount.hpp"

#include <ryn/reactive.hpp>

#include <utility>
#include <vector>

namespace ryn::runtime {

struct MountContext::State {
    NodeStore* nodes;
    Scope scope;
    std::vector<NodeId> roots;
    std::size_t mount_runs{0};
    bool active{true};

    explicit State(NodeStore& node_store) : nodes(&node_store) {}

    void dispose() noexcept {
        if (!active) {
            return;
        }
        active = false;
        scope.dispose();
        for (auto iterator = roots.rbegin(); iterator != roots.rend(); ++iterator) {
            static_cast<void>(nodes->destroy(*iterator));
        }
        roots.clear();
    }

    ~State() {
        dispose();
    }
};

struct ComponentInstance::State : MountContext::State {
    explicit State(NodeStore& nodes) : MountContext::State(nodes) {}
};

MountContext::MountContext(State& state) noexcept : state_(&state) {}

NodeId MountContext::create_root() {
    const NodeId root = state_->nodes->create_root();
    try {
        state_->roots.push_back(root);
    } catch (...) {
        static_cast<void>(state_->nodes->destroy(root));
        throw;
    }
    return root;
}

NodeId MountContext::create_child(NodeId parent) {
    return state_->nodes->create_child(parent);
}

Scope& MountContext::scope() noexcept {
    return state_->scope;
}

ComponentInstance::ComponentInstance(NodeStore& nodes, MountFunction mount)
    : state_(std::make_unique<State>(nodes)) {
    MountContext context(*state_);
    ++state_->mount_runs;
    mount(context);
}

ComponentInstance::~ComponentInstance() = default;

void ComponentInstance::dispose() noexcept {
    state_->dispose();
}

bool ComponentInstance::active() const noexcept {
    return state_->active;
}

std::size_t ComponentInstance::mount_runs() const noexcept {
    return state_->mount_runs;
}

} // namespace ryn::runtime
