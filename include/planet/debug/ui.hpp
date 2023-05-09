#pragma once


#include <planet/affine/rectangle2d.hpp>
#include <planet/ostream.hpp>
#include <planet/ui/baseplate.hpp>
#include <planet/ui/reflowable.hpp>
#include <planet/ui/widget.hpp>


namespace planet::debug {


    /// ## Fixed size UI element
    struct fixed_element : public ui::reflowable {
        affine::extents2d size;

        fixed_element(affine::extents2d const s)
        : reflowable{"planet::debug::fixed_element"}, size{s} {}

        void draw(std::ostream &os) {
            os << name() << " draw @ " << position() << '\n';
        }

      private:
        constrained_type do_reflow(constrained_type const &) override {
            return constrained_type{size};
        }
        void move_sub_elements(affine::rectangle2d const &) override {}
    };


    /// ## Button
    struct button final : public ui::widget<std::ostream &> {
        using superclass = ui::widget<std::ostream &>;

        button() : superclass{"planet::debug::button"} {}

        /// ### The number of times the button has been pressed
        std::size_t clicks = {};

        constrained_type do_reflow(constrained_type const &c) { return c; }
        felspar::coro::task<void> behaviour() {
            for (auto mc = events::identify_clicks(
                         baseplate->mouse_settings, events.mouse.stream());
                 auto click = co_await mc.next();) {
                ++clicks;
            }
        }
        void do_draw(std::ostream &os) {
            os << name() << " do_draw @ " << position() << '\n';
        }
        void do_move_sub_elements(affine::rectangle2d const &) {}
    };


}
