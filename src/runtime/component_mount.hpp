#pragma once

#include "runtime/node_store.hpp"

#include <cstddef>
#include <functional>
#include <memory>

namespace ryn {
class Scope;
}

namespace ryn::runtime {

class ComponentInstance;

class MountContext final {
public:
    [[nodiscard]] NodeId create_root();
    [[nodiscard]] NodeId create_child(NodeId parent);
    [[nodiscard]] Scope& scope() noexcept;

private:
    friend class ComponentInstance;
    struct State;

    explicit MountContext(State& state) noexcept;

    State* state_;
};

class ComponentInstance final {
public:
    using MountFunction = std::function<void(MountContext&)>;

    ComponentInstance(NodeStore& nodes, MountFunction mount);
    ComponentInstance(const ComponentInstance&) = delete;
    ComponentInstance& operator=(const ComponentInstance&) = delete;
    ComponentInstance(ComponentInstance&&) = delete;
    ComponentInstance& operator=(ComponentInstance&&) = delete;
    ~ComponentInstance();

    void dispose() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::size_t mount_runs() const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace ryn::runtime
