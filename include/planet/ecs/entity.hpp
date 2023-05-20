#pragma once


#include <planet/ecs/forward.hpp>

#include <felspar/coro/coroutine.hpp>

#include <vector>


namespace planet::ecs {


    /// ## Entity
    /// Each entity is comprised of a set of components
    class entity final {
        friend class entity_id;
        std::size_t reference_count = 0;

      public:
        explicit entity(std::size_t const component_count)
        : components(component_count) {}

        /// Bitset of the components that this entity uses in each of the
        /// storages
        /// TODO Should be a std::array, or should it? Span is probably best and
        /// leave storage in the entities where this instance is also stored
        std::vector<mask_type> components = {};

        struct on_destroy_continuation {
            mask_type mask;
            felspar::coro::coroutine_handle<> continuation;
        };
        std::vector<on_destroy_continuation> destroy_continuations;
        auto co_on_destroy(mask_type const mask) {
            struct awaitable {
                ecs::entity &entity;
                mask_type mask;

                bool await_ready() const noexcept { return false; }
                void await_suspend(felspar::coro::coroutine_handle<> h) noexcept {
                    entity.destroy_continuations.push_back({mask, h});
                }
                void await_resume() const noexcept {}
            };
            return awaitable{*this, mask};
        }
    };


}