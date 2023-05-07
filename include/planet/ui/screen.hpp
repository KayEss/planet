#pragma once


#include <planet/ui/widget.hpp>


namespace planet::ui {


    /// ## Whole screen UI widget
    /**
     * This widget is infinitely large so will hoover up any events that aren't
     * captured by other widgets. Unless an interface is comprised only of
     * buttons then a screen widget can be used to capture clicks that would
     * enter the play area.
     *
     * Typically the `z_layer` used for a screen widget will be the lowest one
     * to ensure that events are routed to any other widget that overlays the
     * screen.
     */
    template<typename Renderer>
    class screen final : public widget<Renderer> {
      public:
        explicit screen() : widget<Renderer>{"planet::ui::screen"} {}

        ui::panel &panel() { return widget<Renderer>::panel; }

        using constrained_type = typename widget<Renderer>::constrained_type;

      private:
        constrained_type do_reflow(constrained_type const &) override {
            return {};
        }
        void do_move_sub_elements(affine::rectangle2d const &) override {}
        void do_draw(Renderer &) override{};
        bool is_within(affine::point2d const &) const override { return true; }
        bool wants_focus() const override { return true; }
        felspar::coro::task<void> behaviour() override { co_return; }
        void do_draw_within(Renderer &, affine::rectangle2d) override {}
    };


}
