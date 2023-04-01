#include <planet/ui/layout.column.hpp>
#include <planet/debug/ui.hpp>
#include <felspar/test.hpp>


namespace {


    auto const suite = felspar::testsuite("column.layout");


    auto const array_one = suite.test("array/one", [](auto check) {
        auto empty = planet::ui::column{
                std::array{planet::debug::fixed_element{{3, 4}}}, 10};
        check(empty.extents({20, 20}).width) == 3;
        check(empty.extents({20, 20}).height) == 4;
    });


    auto const array_two = suite.test("array/two", [](auto check) {
        auto empty = planet::ui::column{
                std::array{
                        planet::debug::fixed_element{{3, 4}},
                        planet::debug::fixed_element{{8, 4}}},
                10};
        check(empty.extents({20, 20}).width) == 8;
        check(empty.extents({20, 20}).height) == 18;
    });


    auto const tuple_empty = suite.test("tuple/empty", [](auto check) {
        auto empty = planet::ui::column{std::tuple{}, 10};
        check(empty.extents({20, 20}).width) == 0;
        check(empty.extents({20, 20}).height) == 0;
    });


    auto const tuple_one = suite.test("tuple/one", [](auto check) {
        auto empty = planet::ui::column{
                std::tuple{planet::debug::fixed_element{{3, 4}}}, 10};
        check(empty.extents({20, 20}).width) == 3;
        check(empty.extents({20, 20}).height) == 4;
    });


    auto const tuple_two = suite.test("tuple/two", [](auto check) {
        auto empty = planet::ui::column{
                std::tuple{
                        planet::debug::fixed_element{{3, 4}},
                        planet::debug::fixed_element{{8, 4}}},
                10};
        check(empty.extents({20, 20}).width) == 8;
        check(empty.extents({20, 20}).height) == 18;
    });


}
