#include <planet/comms/signal.hpp>
#include <planet/log.hpp>
#include <planet/queue/tspsc.hpp>
#include <planet/serialise/chrono.hpp>
#include <planet/serialise/load_buffer.hpp>
#include <planet/telemetry/counter.hpp>
#include <planet/telemetry/rate.hpp>
#include <planet/time/checkpointer.hpp>

#include <felspar/io/warden.poll.hpp>
#include <felspar/memory/hexdump.hpp>

#include <iostream>
#include <thread>


using namespace std::literals;


thread_local planet::serialise::save_buffer planet::log::detail::ab;


namespace {
    constexpr std::string_view log_root_directory = LOG_ROOT_DIRECTORY;


    auto g_start_time() {
        static auto st = std::chrono::steady_clock::now();
        return st;
    }
    [[maybe_unused]] auto const g_started = g_start_time();


    auto &printers_mutex() {
        static std::mutex m;
        return m;
    }
    auto &printers() {
        static std::map<std::string_view, planet::log::detail::formatter const *>
                m;
        return m;
    }


    void show(planet::serialise::load_buffer &, std::size_t, std::string_view);
    void
            show(planet::serialise::box &b,
                 std::size_t const depth,
                 std::string_view const separator) {
        if (auto printer = printers().find(b.name);
            printer == printers().end()) {
            std::cout << b.name << " v" << int(b.version) << " size "
                      << b.content.size() << " bytes\n";
            show(b.content, depth + 1, separator);
        } else {
            printer->second->print(std::cout, b);
        }
    }
    void
            show(planet::serialise::load_buffer &lb,
                 std::size_t const depth,
                 std::string_view const separator) {
        if (depth) { std::cout << std::string(depth, ' '); }
        while (not lb.empty()) {
            auto const mv = static_cast<std::uint8_t>(lb.cmemory()[0]);
            if (mv > 0 and mv < 80) {
                auto b = load_type<planet::serialise::box>(lb);
                show(b, depth, separator);
            } else {
                auto const m = lb.extract_marker();
                switch (m) {
                case planet::serialise::marker::empty:
                    std::cout << "empty";
                    break;

                case planet::serialise::marker::std_byte_array: {
                    auto const size = lb.extract_size_t();
                    auto s = lb.split(size);
                    felspar::memory::hexdump(std::cout, s);
                    break;
                }

                case planet::serialise::marker::u8:
                    std::cout << std::to_string(lb.extract<std::uint8_t>());
                    break;

                case planet::serialise::marker::b_true:
                    std::cout << "true";
                    break;
                case planet::serialise::marker::b_false:
                    std::cout << "false";
                    break;

                case planet::serialise::marker::i32le:
                    std::cout << std::to_string(lb.extract<std::int32_t>());
                    break;
                case planet::serialise::marker::u32le:
                    std::cout << std::to_string(lb.extract<std::uint32_t>());
                    break;
                case planet::serialise::marker::i64le:
                    std::cout << std::to_string(lb.extract<std::int64_t>());
                    break;
                case planet::serialise::marker::u64le:
                    std::cout << std::to_string(lb.extract<std::uint64_t>());
                    break;

                case planet::serialise::marker::f32le:
                    std::cout << lb.extract<float>();
                    break;
                case planet::serialise::marker::f128le:
                    std::cout << lb.extract<long double>();
                    break;

                case planet::serialise::marker::poly_list: {
                    auto const count = lb.extract_size_t();
                    std::cout << "(poly-list with " << count << " items)\n";
                    for (std::size_t index{}; index < count; ++index) {
                        show(lb, depth + 1, separator);
                    }
                    break;
                }

                case planet::serialise::marker::u8string8: {
                    auto const buffer = lb.split(lb.extract_size_t());
                    std::cout << std::string_view{
                            reinterpret_cast<char const *>(buffer.data()),
                            buffer.size()};
                    break;
                }

                default:
                    std::cerr << "unknown marker [" << to_string(m) << " - 0x"
                              << std::hex << static_cast<unsigned>(m)
                              << std::dec << ']';
                    return;
                }
            }
            if (not lb.empty()) { std::cout << separator; }
        }
    }


    void print(planet::log::message const &m) {
        std::cout << std::fixed
                  << static_cast<double>(
                             (m.logged - g_start_time()).count() / 1e9)
                  << std::defaultfloat << ' ';
        switch (m.level) {
        case planet::log::level::debug:
            std::cout << "\33[0;37mDBUG\33[0;39m ";
            break;
        case planet::log::level::info:
            std::cout << "\33[0;32mINFO\33[0;39m ";
            break;
        case planet::log::level::warning:
            std::cout << "\33[1;33mWARN\33[0;39m ";
            break;
        case planet::log::level::error:
            std::cout << "\33[0;31mERRR\33[0;39m ";
            break;
        case planet::log::level::critical:
            std::cout << "\33[0;31mCRIT\33[0;39m ";
            break;
        }
        planet::serialise::load_buffer buffer{m.payload.cmemory()};
        show(buffer, 0, " ");
        std::string_view fn{m.location.file_name()};
        if (not log_root_directory.empty()
            and fn.starts_with(log_root_directory)) {
            fn.remove_prefix(log_root_directory.size() + 1);
        }
        std::cout << " \33[0;37m" << m.location.function_name() << ' ' << fn
                  << ':' << m.location.line() << ':' << m.location.column()
                  << "\33[0;39m" << std::endl;
    }


    planet::telemetry::counter debug_count{"planet_log_message_debug"};
    planet::telemetry::counter info_count{"planet_log_message_info"};
    planet::telemetry::counter warning_count{"planet_log_message_warning"};
    planet::telemetry::counter error_count{"planet_log_message_error"};


    struct log_thread {
        felspar::io::poll_warden warden;
        planet::queue::tspsc<planet::log::message> messages;
        planet::comms::signal signal{warden};
        planet::comms::signal terminate{warden};

        log_thread() {}
        ~log_thread() {
            if (thread.joinable()) { stop_thread(); }
        }
        void stop_thread() {
            terminate.send({});
            thread.join();
        }
        std::thread thread{[this]() {
            try {
                warden.run(
                        +[](felspar::io::warden &, log_thread *ltp)
                                -> felspar::io::warden::task<void> {
                            co_await ltp->run_loops();
                        },
                        this);
            } catch (...) { std::terminate(); }
        }};
        felspar::io::warden::task<void> run_loops() {
            felspar::io::warden::starter<> tasks;
            tasks.post(*this, &log_thread::display_performance_loop);
            tasks.post(*this, &log_thread::display_log_messages_loop);
            std::array<std::byte, 1> buffer;
            co_await terminate.read_some(buffer);
        }

        void print_performance() {
            planet::telemetry::performance::current_values(
                    planet::log::detail::ab);
            auto const bytes = planet::log::detail::ab.complete();
            planet::log::logged_performance_counters lgc{.counters = bytes};
            {
                std::cout << "\33[0;32mPerformance counters "
                          << static_cast<double>(
                                     (lgc.logged - g_start_time()).count()
                                     / 1e9)
                          << "\33[0;39m\n  ";
                planet::serialise::load_buffer lb{bytes.cmemory()};
                std::scoped_lock _{printers_mutex()};
                show(lb, 0, "\n  ");
                std::cout << std::endl;
            }
            if (auto out = planet::log::output.load(); out) {
                save(planet::log::detail::ab, lgc);
                auto const lb = planet::log::detail::ab.complete();
                (*out).write(
                        reinterpret_cast<char const *>(lb.data()), lb.size());
            }
        }
        felspar::io::warden::task<void> display_performance_loop() {
            planet::log::info("Starting performance counter loop");
            while (true) {
                co_await warden.sleep(1s);
                print_performance();
            }
        }


        felspar::io::warden::task<void> display_log_messages_loop() {
            std::cout << std::setprecision(9);
            while (true) {
                auto block = messages.consume();
                if (block.empty()) {
                    std::array<std::byte, 16> buffer;
                    co_await signal.read_some(buffer);
                } else {
                    auto out = planet::log::output.load();
                    std::scoped_lock _{printers_mutex()};
                    for (auto const &message : block) {
                        if (out) {
                            save(planet::log::detail::ab, message);
                            auto const bytes =
                                    planet::log::detail::ab.complete();
                            (*out).write(
                                    reinterpret_cast<char const *>(bytes.data()),
                                    bytes.size());
                        }
                        print(message);
                        switch (message.level) {
                        case planet::log::level::debug: ++debug_count; break;
                        case planet::log::level::info: ++info_count; break;
                        case planet::log::level::warning:
                            ++warning_count;
                            break;
                        case planet::log::level::error: ++error_count; break;
                        case planet::log::level::critical:
                            std::cout << "\33[0;31mCritical log message "
                                         "forcing unclean shutdown\33[0;39m\n";
                            std::exit(120);
                        }
                    }
                }
            }
        }
    };

    auto &g_log_thread() {
        static log_thread lt;
        return lt;
    }
}


void planet::log::stop_thread() {
    auto &thread = g_log_thread();
    thread.stop_thread();
    thread.print_performance();
}


void planet::log::detail::critical_log_encountered() {
    /**
     * Wait for a bit here.
     *
     * The terminate that is actually meaningful is the one a few lines above
     * which will cause the program to terminate after dealing with the log
     * message this is called from. The one here is just to ensure that this
     * function doesn't actually return.
     */
    std::this_thread::sleep_for(2s);
    std::exit(121);
}


namespace {
    planet::telemetry::counter message_count{"planet_log_message_count"};
    planet::telemetry::real_time_rate message_rate{
            "planet_log_message_rate", 2s};
}
void planet::log::detail::write_log(
        level const l,
        serialise::shared_bytes b,
        felspar::source_location const &loc) {
    auto &lt = g_log_thread();
    lt.messages.push({l, std::move(b), loc});
    lt.signal.send({});
    ++message_count;
    message_rate.tick();
}


void planet::log::pretty_print(
        serialise::load_buffer &lb,
        std::size_t const depth,
        std::string_view const prefix) {
    show(lb, depth, prefix);
}
void planet::log::pretty_print(
        serialise::box &b,
        std::size_t const depth,
        std::string_view const prefix) {
    if (depth) { std::cout << std::string(depth, ' '); }
    show(b, depth, prefix);
}


/// ## `planet::log::detail::formatter`


planet::log::detail::formatter::formatter(std::string_view const n)
: box_name{n} {
    std::scoped_lock _{printers_mutex()};
    printers()[box_name] = this;
}


planet::log::detail::formatter::~formatter() {
    /**
     * Because we don't control the destruction order it's possible for a
     * formatter to be destroyed after the global in this translation unit has
     * gone. Because these are all static functions anyway it's fine to just let
     * this "leak".
     */
    // std::scoped_lock _{printers_mutex()};
    // printers().erase(printers().find(box_name));
}


/// ## `planet::log::counters`


auto planet::log::counters::current() noexcept -> counters {
    return {debug_count.value(), info_count.value(), warning_count.value(),
            error_count.value()};
}


/// ## `planet::log::file_header`


void planet::log::write_log_file_header() {
    detail::ab.save_box(file_header::box, g_start_time(), log_root_directory);
    auto const bytes = detail::ab.complete();
    (*planet::log::output.load())
            .write(reinterpret_cast<char const *>(bytes.data()), bytes.size());
}
void planet::log::load_fields(serialise::box &b, file_header &f) {
    b.fields(f.base_time, f.file_prefix);
    b.check_empty_or_throw();
}


/// ## `planet::log::logged_performance_counters`


void planet::log::save(
        serialise::save_buffer &ab, logged_performance_counters const l) {
    ab.save_box(l.box, l.logged, l.counters);
}


/// ## `planet::log::message`


void planet::log::save(serialise::save_buffer &ab, message const m) {
    ab.save_box(m.box, m.level, m.location, m.logged, m.payload);
}
