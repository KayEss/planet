#pragma once


#include <planet/serialise/base_types.hpp>
#include <planet/serialise/load_buffer.hpp>
#include <planet/serialise/save_buffer.hpp>

#include <chrono>


namespace planet::serialise {


    template<typename Clock, typename Duration>
    void
            save(save_buffer &ab,
                 std::chrono::time_point<Clock, Duration> const &tp) {
        ab.save_box("_sc::time_point", tp.time_since_epoch().count());
    }
    template<typename Clock, typename Duration>
    void load(box &b, std::chrono::time_point<Clock, Duration> &tp) {
        decltype(tp.time_since_epoch().count()) c;
        b.named("_sc::time_point", c);
        tp = std::chrono::time_point<Clock, Duration>{Duration{c}};
    }


}
