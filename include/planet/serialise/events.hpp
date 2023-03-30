#include <planet/serialise/load_buffer.hpp>
#include <planet/serialise/save_buffer.hpp>
#include <planet/events.hpp>


namespace planet::events {


    /// ## `planet::events::quit`
    inline void save(serialise::save_buffer &ab, quit) {
        ab.save_box("_p:e:quit");
    }
    inline void load(serialise::load_buffer &lb, events::quit &) {
        lb.load_box("_p:e:quit");
    }


}
