#include <array>
#include <cstring>
#include <cstdlib>
#include <format>
#include <map>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>

#include <getopt.h>
#include <xcb/xcb.h>

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct Options
{
    bool        verbose    = false;  // log unhandled events + every configure-notify
    bool        log_motion = true;   // --no-motion suppresses XCB_MOTION_NOTIFY logging
    bool        draw       = true;   // --no-draw skips all xcb drawing calls
    std::string output;              // "" means stdout; otherwise a file path
    uint16_t    win_w      = 512;
    uint16_t    win_h      = 512;
    std::string title      = "xevtest";
};

[[noreturn]] void usage(std::string_view argv0, int exit_code)
{
    std::print(
        "Usage: {} [OPTIONS]\n"
        "\n"
        "An XCB input-event logger with optional drawing.\n"
        "\n"
        "Options:\n"
        "  -v, --verbose          Log unhandled events and all configure-notify events\n"
        "  -M, --no-motion        Suppress motion-notify log output (reduces noise)\n"
        "  -D, --no-draw          Disable drawing; only log events\n"
        "  -o, --output FILE      Write log to FILE instead of stdout\n"
        "  -s, --size WxH         Initial window size (default: 512x512)\n"
        "  -t, --title TITLE      Window title (default: xevtest)\n"
        "  -h, --help             Show this help and exit\n"
        "\n"
        "Key bindings:\n"
        "  Escape                 Quit\n"
        "\n"
        "Drawing (when --no-draw is not set):\n"
        "  Drag with button(s)    Paint coloured squares; each held button\n"
        "                         uses a different colour and slight x-offset\n"
        "  Move without button    Leave a 1-pixel cursor trail\n",
        argv0);
    std::exit(exit_code);
}

// Parse WxH, e.g. "800x600". Throws on bad input.
std::pair<uint16_t, uint16_t> parse_size(std::string_view s)
{
    auto x = s.find('x');
    if (x == std::string_view::npos)
        throw std::runtime_error(std::format("invalid size '{}': expected WxH", s));

    auto parse_u16 = [&](std::string_view part, std::string_view name) -> uint16_t {
        if (part.empty())
            throw std::runtime_error(std::format("invalid size '{}': {} is empty", s, name));
        int v = 0;
        for (char c : part) {
            if (c < '0' || c > '9')
                throw std::runtime_error(std::format("invalid size '{}': non-digit in {}", s, name));
            v = v * 10 + (c - '0');
        }
        if (v < 1 || v > 32767)
            throw std::runtime_error(std::format("invalid size '{}': {} out of range", s, name));
        return static_cast<uint16_t>(v);
    };

    return { parse_u16(s.substr(0, x), "width"), parse_u16(s.substr(x + 1), "height") };
}

Options parse_args(int argc, char** argv)
{
    static constexpr option long_opts[] = {
        { "verbose",   no_argument,       nullptr, 'v' },
        { "no-motion", no_argument,       nullptr, 'M' },
        { "no-draw",   no_argument,       nullptr, 'D' },
        { "output",    required_argument, nullptr, 'o' },
        { "size",      required_argument, nullptr, 's' },
        { "title",     required_argument, nullptr, 't' },
        { "help",      no_argument,       nullptr, 'h' },
        { nullptr, 0, nullptr, 0 }
    };

    Options opts;
    int c;
    while ((c = getopt_long(argc, argv, "vMDo:s:t:h", long_opts, nullptr)) != -1) {
        switch (c) {
            case 'v': opts.verbose    = true;   break;
            case 'M': opts.log_motion = false;  break;
            case 'D': opts.draw       = false;  break;
            case 'o': opts.output     = optarg; break;
            case 't': opts.title      = optarg; break;
            case 's': {
                auto [w, h] = parse_size(optarg);
                opts.win_w  = w;
                opts.win_h  = h;
                break;
            }
            case 'h': usage(argv[0], EXIT_SUCCESS);
            default:  usage(argv[0], EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        std::print(stderr, "error: unexpected argument '{}'\n", argv[optind]);
        usage(argv[0], EXIT_FAILURE);
    }

    return opts;
}

// ---------------------------------------------------------------------------
// Modifier decoder
// ---------------------------------------------------------------------------

std::string format_modifiers(uint16_t state)
{
    static constexpr std::array<std::string_view, 13> names = {
        "Shift", "Lock", "Ctrl", "Mod1", "Mod2", "Mod3", "Mod4", "Mod5",
        "Btn1", "Btn2", "Btn3", "Btn4", "Btn5"
    };
    std::string result;
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        if (state & (1u << i)) {
            if (!result.empty()) result += '+';
            result += names[i];
        }
    }
    return result.empty() ? "none" : result;
}

// ---------------------------------------------------------------------------
// EventPrinter
// ---------------------------------------------------------------------------

class EventPrinter
{
public:
    explicit EventPrinter(FILE* fout) : m_fout(fout) {}

    void print(xcb_key_press_event_t const& ev) {
        std::print(m_fout,
            "key-{:<11}  x={:<6}  y={:<6}  state={:016b}  ({:<28})  keycode={}\n",
            (ev.response_type & ~0x80) == XCB_KEY_PRESS ? "press" : "release",
            ev.event_x, ev.event_y,
            ev.state, format_modifiers(ev.state),
            ev.detail);
        flush();
    }

    void print(xcb_button_press_event_t const& ev) {
        bool const is_press = (ev.response_type & ~0x80) == XCB_BUTTON_PRESS;

        if (ev.detail == 4 || ev.detail == 5) {
            if (!is_press) return;
            std::print(m_fout,
                "scroll-{:<9}  x={:<6}  y={:<6}  state={:016b}  ({:<28})\n",
                ev.detail == 4 ? "up" : "down",
                ev.event_x, ev.event_y,
                ev.state, format_modifiers(ev.state));
            flush();
            return;
        }

        std::print(m_fout,
            "button-{:<8}  x={:<6}  y={:<6}  state={:016b}  ({:<28})  button={}\n",
            is_press ? "press" : "release",
            ev.event_x, ev.event_y,
            ev.state, format_modifiers(ev.state),
            ev.detail);
        flush();
    }

    void print(xcb_motion_notify_event_t const& ev) {
        std::print(m_fout,
            "motion-notify    x={:<6}  y={:<6}  state={:016b}  ({:<28})\n",
            ev.event_x, ev.event_y,
            ev.state, format_modifiers(ev.state));
        flush();
    }

    void print(xcb_expose_event_t const& ev) {
        std::print(m_fout,
            "expose           x={:<6}  y={:<6}  w={:<6}  h={:<6}  count={}\n",
            ev.x, ev.y, ev.width, ev.height, ev.count);
        flush();
    }

    void print(xcb_configure_notify_event_t const& ev) {
        std::print(m_fout,
            "configure-notify x={:<6}  y={:<6}  w={:<6}  h={}\n",
            ev.x, ev.y, ev.width, ev.height);
        flush();
    }

    void print(xcb_mapping_notify_event_t const& ev) {
        std::print(m_fout,
            "mapping-notify   request={}  first_keycode={}  count={}\n",
            ev.request, ev.first_keycode, ev.count);
        flush();
    }

    void print(xcb_generic_event_t const& ev) {
        std::print(m_fout,
            "unhandled        response_type={}\n",
            ev.response_type & ~0x80u);
        flush();
    }

private:
    void flush() { std::fflush(m_fout); }
    FILE* m_fout;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

xcb_atom_t intern_atom(xcb_connection_t* conn, std::string_view name)
{
    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(conn, 0, static_cast<uint16_t>(name.size()), name.data());
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, nullptr);
    if (!reply) return XCB_ATOM_NONE;
    std::unique_ptr<xcb_intern_atom_reply_t, decltype(&free)> g(reply, &free);
    return reply->atom;
}

void draw_text(xcb_connection_t* conn, xcb_window_t win, xcb_gcontext_t gc,
               int16_t x, int16_t y, std::string_view text)
{
    xcb_image_text_8(conn, static_cast<uint8_t>(text.size()),
                     win, gc, x, y, text.data());
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void run(Options const& opts)
{
    // -- Log output -----------------------------------------------------------
    FILE* fout = stdout;
    auto fclose_deleter = [](FILE* f) { std::fclose(f); };
    std::unique_ptr<FILE, decltype(fclose_deleter)> fout_guard(nullptr, fclose_deleter);
    if (!opts.output.empty()) {
        FILE* f = std::fopen(opts.output.c_str(), "w");
        if (!f)
            throw std::runtime_error(
                std::format("cannot open '{}': {}", opts.output, std::strerror(errno)));
        fout_guard.reset(f);
        fout = f;
        std::print("logging to '{}'\n", opts.output);
    }

    // -- Connection -----------------------------------------------------------
    xcb_connection_t* conn = xcb_connect(nullptr, nullptr);
    if (!conn || xcb_connection_has_error(conn))
        throw std::runtime_error("xcb_connect failed");
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)>
        conn_guard{ conn, &xcb_disconnect };

    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    xcb_atom_t const wm_protocols     = intern_atom(conn, "WM_PROTOCOLS");
    xcb_atom_t const wm_delete_window = intern_atom(conn, "WM_DELETE_WINDOW");

    // -- Window ---------------------------------------------------------------
    uint32_t const win_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t const win_values[2] = {
        screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE         |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_POINTER_MOTION   |
        XCB_EVENT_MASK_KEY_PRESS        |
        XCB_EVENT_MASK_KEY_RELEASE      |
        XCB_EVENT_MASK_BUTTON_PRESS     |
        XCB_EVENT_MASK_BUTTON_RELEASE
    };

    xcb_window_t const window = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, screen->root,
                      0, 0, opts.win_w, opts.win_h, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual, win_mask, win_values);

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window,
                        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        static_cast<uint32_t>(opts.title.size()), opts.title.data());

    if (wm_protocols != XCB_ATOM_NONE && wm_delete_window != XCB_ATOM_NONE)
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window,
                            wm_protocols, XCB_ATOM_ATOM, 32, 1, &wm_delete_window);

    xcb_map_window(conn, window);
    xcb_flush(conn);

    // -- GC + colours (only needed when drawing) ------------------------------
    xcb_gcontext_t gc = 0;
    std::array<uint32_t, 12> colors{};

    if (opts.draw) {
        gc = xcb_generate_id(conn);
        uint32_t const gc_mask     = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
        uint32_t       gc_values[] = { screen->black_pixel ^ 0x00FF00u, 0u };
        xcb_create_gc(conn, gc, window, gc_mask, gc_values);

        xcb_colormap_t colormap = xcb_generate_id(conn);
        xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE,
                            colormap, screen->root, screen->root_visual);

        static constexpr std::array<std::array<uint8_t, 3>, 12> rgb_colors = {{
            {{255,   0,   0}}, {{  0, 255,   0}}, {{  0,   0, 255}},
            {{255, 255,   0}}, {{128,   0, 128}}, {{173, 216, 230}},
            {{255, 165,   0}}, {{255, 192, 203}}, {{  0, 128, 128}},
            {{  0,   0, 139}}, {{  0, 100,   0}}, {{144, 238, 144}},
        }};

        for (std::size_t i = 0; i < colors.size(); ++i) {
            xcb_alloc_color_cookie_t ck =
                xcb_alloc_color(conn, colormap,
                                rgb_colors[i][0] * 257u,
                                rgb_colors[i][1] * 257u,
                                rgb_colors[i][2] * 257u);
            xcb_alloc_color_reply_t* reply = xcb_alloc_color_reply(conn, ck, nullptr);
            if (!reply) throw std::runtime_error("color allocation failure");
            std::unique_ptr<xcb_alloc_color_reply_t, decltype(&free)> g(reply, &free);
            colors[i] = reply->pixel;
        }
    }

    // -- Event loop -----------------------------------------------------------
    EventPrinter evprinter(fout);
    std::map<xcb_button_t, bool> button_state;
    uint16_t win_w       = opts.win_w;
    uint16_t win_h       = opts.win_h;
    uint32_t click_count = 0;

    auto redraw_counter = [&] {
        if (!opts.draw) return;
        xcb_rectangle_t bg = { 2, 2, 100, 14 };
        uint32_t black = screen->black_pixel;
        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &black);
        xcb_poly_fill_rectangle(conn, window, gc, 1, &bg);
        uint32_t white = screen->white_pixel;
        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &white);
        auto label = std::format("clicks: {}", click_count);
        draw_text(conn, window, gc, 4, 13, label);
    };

    bool quit = false;
    while (!quit) {
        xcb_generic_event_t* event = xcb_wait_for_event(conn);
        if (!event) throw std::runtime_error("xcb_wait_for_event error");
        std::unique_ptr<xcb_generic_event_t, decltype(&free)> ev_guard(event, &free);

        switch (event->response_type & ~0x80u)
        {
            case XCB_EXPOSE: {
                auto const& ev = reinterpret_cast<xcb_expose_event_t&>(*event);
                evprinter.print(ev);
                if (ev.count == 0) { redraw_counter(); xcb_flush(conn); }
                break;
            }

            case XCB_CONFIGURE_NOTIFY: {
                auto const& ev = reinterpret_cast<xcb_configure_notify_event_t&>(*event);
                if (ev.width != win_w || ev.height != win_h) {
                    win_w = ev.width;
                    win_h = ev.height;
                    if (opts.verbose) evprinter.print(ev);
                }
                break;
            }

            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE: {
                auto const& ev = reinterpret_cast<xcb_key_press_event_t&>(*event);
                evprinter.print(ev);
                if ((ev.response_type & ~0x80u) == XCB_KEY_PRESS && ev.detail == 9) {
                    std::print(fout, "escape pressed, exiting…\n");
                    quit = true;
                }
                break;
            }

            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE: {
                auto const& ev      = reinterpret_cast<xcb_button_press_event_t&>(*event);
                bool const is_press = (ev.response_type & ~0x80u) == XCB_BUTTON_PRESS;
                evprinter.print(ev);
                if (ev.detail != 4 && ev.detail != 5) {
                    button_state[ev.detail] = is_press;
                    if (is_press) { ++click_count; redraw_counter(); xcb_flush(conn); }
                }
                break;
            }

            case XCB_MOTION_NOTIFY: {
                auto const& ev = reinterpret_cast<xcb_motion_notify_event_t&>(*event);
                if (opts.log_motion) evprinter.print(ev);

                if (opts.draw) {
                    int offset = 0;
                    for (auto const& [btn, held] : button_state) {
                        if (!held) continue;
                        ++offset;
                        constexpr int SZ = 5;
                        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND,
                                      &colors[(btn - 1) % colors.size()]);
                        xcb_rectangle_t rect = {
                            static_cast<int16_t>(ev.event_x - SZ/2 + offset * SZ),
                            static_cast<int16_t>(ev.event_y - SZ/2),
                            SZ, SZ
                        };
                        xcb_poly_fill_rectangle(conn, window, gc, 1, &rect);
                    }
                    if (offset == 0) {
                        xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &colors.back());
                        xcb_rectangle_t rect = {
                            static_cast<int16_t>(ev.event_x),
                            static_cast<int16_t>(ev.event_y), 1, 1
                        };
                        xcb_poly_fill_rectangle(conn, window, gc, 1, &rect);
                    }
                    xcb_flush(conn);
                }
                break;
            }

            case XCB_CLIENT_MESSAGE: {
                auto const& ev = reinterpret_cast<xcb_client_message_event_t&>(*event);
                if (ev.type == wm_protocols && ev.data.data32[0] == wm_delete_window) {
                    std::print(fout, "window closed, exiting…\n");
                    quit = true;
                }
                break;
            }

            case XCB_MAPPING_NOTIFY: {
                auto const& ev = reinterpret_cast<xcb_mapping_notify_event_t&>(*event);
                evprinter.print(ev);
                break;
            }

            default:
                if (opts.verbose) evprinter.print(*event);
                break;
        }
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    try {
        Options const opts = parse_args(argc, argv);
        run(opts);
        return EXIT_SUCCESS;
    } catch (std::exception const& err) {
        std::print(stderr, "error: {}\n", err.what());
        return EXIT_FAILURE;
    }
}
