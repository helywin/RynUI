static void decoration_frame_configure(void)
{
#if SDL_LIBDECOR_CHECK_VERSION(0, 2, 0)
        suspended = (window_state & LIBDECOR_WINDOW_STATE_SUSPENDED) != 0;
#endif
#if SDL_LIBDECOR_CHECK_VERSION(0, 3, 0)
        resizing = (window_state & LIBDECOR_WINDOW_STATE_RESIZING) != 0;

        if (window_state & LIBDECOR_WINDOW_STATE_CONSTRAINED_LEFT) {
            wind->toplevel_constraints |= WAYLAND_TOPLEVEL_CONSTRAINED_LEFT;
        }
#endif
}
