#pragma once


#include <planet/ui/concepts.hpp>
#include <planet/ui/gravity.hpp>
#include <planet/ui/helpers.hpp>
#include <planet/ui/reflowable.hpp>
#include <planet/ui/padding.hpp>


namespace planet::ui {


    /// ## Box wrapper
    /**
     * A container for another element. The box itself is not drawn. The
     * `gravity` can be used to control how the content is positioned within the
     * box.
     */
    template<typename C>
    struct box final : public reflowable {
        using content_type = C;


        box() : reflowable{"planet::ui::box"} {}
        box(content_type c)
        : reflowable{"planet::ui::box"}, content{std::move(c)} {}
        box(content_type c, float const hp, float const vp)
        : reflowable{"planet::ui::box"},
          content{std::move(c)},
          padding{hp, vp} {}
        box(content_type c, ui::gravity const g, float const p = {})
        : reflowable{"planet::ui::box"},
          content{std::move(c)},
          gravity{g},
          padding{p} {}
        box(std::string_view const n, content_type c)
        : reflowable{n}, content{std::move(c)} {}


        /// ### What is inside the box
        content_type content;
        /// #### The size of the box in its container's coordinate system
        ui::gravity gravity = {
                ui::gravity::left | ui::gravity::right | ui::gravity::top
                | ui::gravity::bottom};
        /// #### The amount of padding to be added around the content.
        ui::padding padding = {};


        /// ### Drawing the box content
        void draw() { content.draw(); }


        /// ### Visibility
        void visible(bool const v)
            requires(visibility<content_type>)
        {
            content.visible(v);
        }
        bool is_visible() const noexcept
            requires(visibility<content_type>)
        {
            return content.is_visible();
        }


      private:
        constrained_type do_reflow(constrained_type const &ex) override {
            return add(content.reflow(padding.remove(ex)), ex);
        }
        affine::rectangle2d
                move_sub_elements(affine::rectangle2d const &outer) override {
            auto const inner_size = content.constraints().extents();
            auto const area =
                    within(gravity, padding.remove(outer), inner_size);
            content.move_to(area);
            return outer;
        }
        constrained_type
                add(constrained_type const &inner,
                    constrained_type const &outer) const noexcept {
            auto const min_width =
                    inner.width.min() + padding.left + padding.right;
            auto const min_height =
                    inner.height.min() + padding.top + padding.bottom;
            return {{inner.width.value() + padding.left + padding.right,
                     std::max(min_width, outer.width.min()), outer.width.max()},
                    {inner.height.value() + padding.top + padding.bottom,
                     std::max(min_height, outer.height.min()),
                     outer.height.max()}};
        }
    };


}
