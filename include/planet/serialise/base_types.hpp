#pragma once


#include <planet/serialise/load_buffer.hpp>
#include <planet/serialise/save_buffer.hpp>

#include <array>
#include <cstring>
#include <optional>
#include <span>
#include <vector>


namespace planet::serialise {


    inline void save(save_buffer &ab, bool const b) {
        ab.append(b ? marker::b_true : marker::b_false);
    }
    inline void load(load_buffer &lb, bool &b) {
        auto const m = lb.extract_marker();
        if (m == marker::b_true) {
            b = true;
        } else if (m == marker::b_false) {
            b = false;
        } else {
            throw wanted_boolean(lb.cmemory(), m);
        }
    }


    inline void save(save_buffer &ab, std::span<std::byte const> const s) {
        ab.append(marker::std_byte_array);
        ab.append_size_t(s.size());
        ab.append(s);
    }
    inline void load(load_buffer &lb, std::span<std::byte const> &s) {
        if (auto const m = lb.extract_marker(); m != marker::std_byte_array) {
            throw wrong_marker{lb.cmemory(), marker::std_byte_array, m};
        } else {
            auto const size = lb.extract_size_t();
            s = lb.split(size);
        }
    }


    inline void save(save_buffer &ab, std::vector<std::byte> const &v) {
        save(ab, std::span{v.data(), v.size()});
    }
    inline void load(load_buffer &lb, std::vector<std::byte> &v) {
        auto const d = load_type<std::span<std::byte const>>(lb);
        v.resize(d.size());
        std::memcpy(v.data(), d.data(), d.size());
    }


    template<felspar::parse::concepts::numeric T>
    inline void save(save_buffer &ab, T const t) {
        ab.append(marker_for<T>());
        ab.append(t);
    }
    template<felspar::parse::concepts::numeric T>
    inline void
            load(load_buffer &lb,
                 T &s,
                 felspar::source_location const &loc =
                         felspar::source_location::current()) {
        auto const m = lb.extract_marker();
        if (auto const want = marker_for<T>(); m != want) {
            if (is_endian(m) and other_endian(m) == want) {
                s = lb.extract_non_native<T>(loc);
            } else {
                throw wrong_marker(lb.cmemory(), want, m);
            }
        } else {
            s = lb.extract<T>(loc);
        }
    }


    template<typename T, std::size_t N>
    inline void save(save_buffer &ab, std::span<T, N> const &a) {
        ab.append(marker::poly_list);
        ab.append_size_t(a.size());
        for (auto &&item : a) { save(ab, item); }
    }
    template<typename T, std::size_t N>
    void load(load_buffer &lb, std::span<T, N> a) {
        lb.check_marker(marker::poly_list);
        auto const items = lb.extract_size_t();
        if (items > N) {
            throw felspar::stdexcept::runtime_error{"Too many items for array"};
        }
        for (auto &item : a) { load(lb, item); }
    }


    template<typename T, std::size_t N>
    inline void save(save_buffer &ab, std::array<T, N> const &a) {
        save(ab, std::span{a});
    }
    template<typename T, std::size_t N>
    void load(load_buffer &lb, std::array<T, N> &a) {
        load(lb, std::span{a});
    }


    template<typename T>
    inline void save(save_buffer &ab, std::vector<T> const &v) {
        save(ab, std::span{v});
    }
    template<typename T>
    inline void load(load_buffer &lb, std::vector<T> &v) {
        if (auto const m = lb.extract_marker(); m != marker::poly_list) {
            throw wrong_marker{lb.cmemory(), marker::poly_list, m};
        } else {
            auto const items = lb.extract_size_t();
            v = std::vector<T>(items);
            for (auto &item : v) { load(lb, item); }
        }
    }


    template<typename T>
    inline void save(save_buffer &ab, std::optional<T> const &v) {
        if (v) {
            ab.save_box("_s:opt", true, *v);
        } else {
            ab.save_box("_s:opt", false);
        }
    }
    template<typename T>
    inline void load(box &b, std::optional<T> &v) {
        try {
            b.check_name_or_throw("_s:opt");
            bool has_value = {};
            load(b.content, has_value);
            if (has_value) {
                v.emplace();
                load(b.content, *v);
            } else {
                v.reset();
            }
            b.check_empty_or_throw();
        } catch (serialisation_error &e) {
            e.inside_box("_s:opt");
            throw;
        }
    }


}
