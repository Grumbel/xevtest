#include <array>
#include <map>
#include <optional>
#include <stdio.h>
#include <stdlib.h>

#include <fmt/format.h>
#include <xcb/xcb.h>

struct Options
{
};

class EventPrinter
{
public:
  EventPrinter(FILE* fout) :
    m_fout(fout)
  {}

  void print(xcb_key_press_event_t const& ev) {
    fmt::print(
      m_fout,
      "key-{:<11}  x={:<6}  y={:<6}  state={:<016b}  keycode={:<6}\n",
      (ev.response_type & ~0x80) == XCB_KEY_PRESS ? "press" : "release",
      ev.event_x,
      ev.event_y,
      ev.state,
      ev.detail
      );
    fflush(m_fout);
  }

  void print(xcb_button_press_event_t const& ev) {
    fmt::print(
      m_fout,
      "button-{:<8}  x={:<6}  y={:<6}  state={:<016b}  button={:<6}\n",
      (ev.response_type & ~0x80) == XCB_BUTTON_PRESS ? "press" : "release",
      ev.event_x,
      ev.event_y,
      ev.state,
      ev.detail
      );
    fflush(m_fout);
  }

  void print(xcb_motion_notify_event_t const& ev) {
    fmt::print(
      m_fout,
      "motion-notify    x={:<6}  y={:<6}  state={:<016b}\n",
      ev.event_x,
      ev.event_y,
      ev.state);
    fflush(m_fout);
  }

  void print(xcb_expose_event_t const& ev) {
    fmt::print(m_fout, "expose\n");
  }

  void print(xcb_mapping_notify_event_t const& ev) {
    fmt::print(m_fout, "mapping-notify   request={} first_keycode={} count={}\n",
               ev.request,
               ev.first_keycode,
               ev.count);
    fflush(m_fout);
  }

  void print(xcb_generic_event_t const& ev) {
    fmt::print(m_fout, "unhandled {}\n", ev.response_type);
    fflush(m_fout);
  }

  void print(std::string_view text) {
    fmt::print(m_fout, "{}\n", text);
    fflush(m_fout);
  }

private:
  FILE* m_fout;
};

void run(Options& opts)
{
  xcb_connection_t* conn = xcb_connect(nullptr, nullptr);
  if (conn == nullptr) {
    throw std::runtime_error("xcb_connect failed");
  }
  std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> conn_uptr{ conn, &xcb_disconnect };

  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

  uint32_t win_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  uint32_t win_values [2];
  win_values [0] = screen -> black_pixel;
  win_values [1] = (XCB_EVENT_MASK_EXPOSURE |
                    XCB_EVENT_MASK_POINTER_MOTION |
                    XCB_EVENT_MASK_KEY_PRESS |
                    XCB_EVENT_MASK_KEY_RELEASE |
                    XCB_EVENT_MASK_BUTTON_PRESS |
                    XCB_EVENT_MASK_BUTTON_RELEASE);

  xcb_window_t window = xcb_generate_id(conn);
  xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 256, 256, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, win_mask, win_values);

  std::string window_title = "xevtest";
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, window_title.size(), window_title.data());

  xcb_map_window(conn, window);
  xcb_flush(conn);

  xcb_gcontext_t gc = xcb_generate_id(conn);
  uint32_t gc_mask = (XCB_GC_FOREGROUND |
                      XCB_GC_GRAPHICS_EXPOSURES);
  uint32_t gc_values[2] = { screen->black_pixel ^ 0x00FF00, 0 };
  xcb_create_gc(conn, gc, window, gc_mask, gc_values);

  xcb_colormap_t colormap = xcb_generate_id(conn);
  xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, screen->root_visual);

  std::array<std::array<uint8_t, 3>, 12> rgb_colors = {{
      {{255, 0, 0}},    // Red
      {{0, 255, 0}},    // Green
      {{0, 0, 255}},    // Blue
      {{255, 255, 0}},  // Yellow
      {{128, 0, 128}},  // Purple
      {{173, 216, 230}},// Light Blue
      {{255, 165, 0}},  // Orange
      {{255, 192, 203}},// Pink
      {{0, 128, 128}},  // Teal
      {{0, 0, 139}},    // Dark Blue
      {{0, 100, 0}},    // Dark Green
      {{144, 238, 144}} // Light Green
    }};
  std::array<uint32_t, rgb_colors.size()> colors;
  for (int i = 0; i < colors.size(); ++i)
  {
    xcb_alloc_color_cookie_t cookie = xcb_alloc_color(conn, colormap,
                                                      rgb_colors[i][0] * 255,
                                                      rgb_colors[i][1] * 255,
                                                      rgb_colors[i][2] * 255);
    xcb_alloc_color_reply_t* reply = xcb_alloc_color_reply(conn, cookie, nullptr);
    if (reply == nullptr) {
      throw std::runtime_error("color allocation failure");
    }
    std::unique_ptr<xcb_alloc_color_reply_t, decltype(&free)> reply_uptr(reply, &free);

    colors[i] = reply->pixel;
  }

  EventPrinter evprinter(stdout);
  std::map<xcb_button_t, bool> button_state;
  bool quit = false;
  while (!quit)
  {
    xcb_generic_event_t *event = xcb_wait_for_event(conn);
    if (event == nullptr) {
      throw std::runtime_error("xcb_wait_for_event error");
    }
    std::unique_ptr<xcb_generic_event_t, decltype(&free)> event_uptr(event, &free);

    switch (event->response_type & ~0x80)
    {
      case XCB_EXPOSE: {
        auto const& ev = reinterpret_cast<xcb_expose_event_t&>(*event);
        evprinter.print(ev);
        break;
      }

      case XCB_MAPPING_NOTIFY: {
        auto const& ev = reinterpret_cast<xcb_mapping_notify_event_t&>(*event);
        evprinter.print(ev);
        break;
      }

      case XCB_KEY_PRESS:
      case XCB_KEY_RELEASE: {
        auto const& ev = reinterpret_cast<xcb_key_press_event_t&>(*event);
        evprinter.print(ev);
        if ((ev.response_type & ~0x80) == XCB_KEY_PRESS) {
          if (ev.detail == 9 /* Escape */) {
            fmt::print("escape pressed, exiting...\n");
            quit = true;
          }
        }
        break;
      }

      case XCB_BUTTON_PRESS:
      case XCB_BUTTON_RELEASE: {
        auto const& ev = reinterpret_cast<xcb_button_press_event_t&>(*event);
        evprinter.print(ev);
        button_state[ev.detail] = ((ev.response_type & ~0x80) == XCB_BUTTON_PRESS);
        break;
      }

      case XCB_MOTION_NOTIFY: {
        auto const& ev = reinterpret_cast<xcb_motion_notify_event_t&>(*event);
        evprinter.print(ev);

        int offset = 0;
        for (auto const& it : button_state)
        {
          if (it.second) {
            offset += 1;

            constexpr int square_size = 5;
            xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &colors[(it.first - 1) % colors.size()]);
            xcb_rectangle_t rect = {
              static_cast<int16_t>(ev.event_x - square_size / 2 - 1 + offset * square_size),
              static_cast<int16_t>(ev.event_y - square_size / 2 - 1),
              square_size, square_size
            };
            xcb_poly_fill_rectangle(conn, window, gc, 1, &rect);
          }
        }

        if (offset == 0)
        {
          constexpr int square_size = 1;
          xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &colors.back());
          xcb_rectangle_t rect = {
            static_cast<int16_t>(ev.event_x - square_size / 2 - 1 + offset * square_size),
            static_cast<int16_t>(ev.event_y - square_size / 2 - 1),
            square_size, square_size
          };
          xcb_poly_fill_rectangle(conn, window, gc, 1, &rect);
        }

        xcb_flush(conn);

        break;
      }

      default:
        evprinter.print(*event);
        break;
    }
  }
}

int main(int argc, char** argv)
{
  try
  {
    Options opts;
    run(opts);
    return 0;
  }
  catch (std::exception const& err) {
    fmt::print(stderr, "error: {}\n", err.what());
  }
}

/* EOF */

