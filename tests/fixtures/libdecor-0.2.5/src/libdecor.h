struct libdecor_state;

enum libdecor_window_state {
	LIBDECOR_WINDOW_STATE_NONE = 0,
	LIBDECOR_WINDOW_STATE_ACTIVE = 1 << 0,
	LIBDECOR_WINDOW_STATE_MAXIMIZED = 1 << 1,
	LIBDECOR_WINDOW_STATE_FULLSCREEN = 1 << 2,
	LIBDECOR_WINDOW_STATE_TILED_LEFT = 1 << 3,
	LIBDECOR_WINDOW_STATE_TILED_RIGHT = 1 << 4,
	LIBDECOR_WINDOW_STATE_TILED_TOP = 1 << 5,
	LIBDECOR_WINDOW_STATE_TILED_BOTTOM = 1 << 6,
	LIBDECOR_WINDOW_STATE_SUSPENDED = 1 << 7,
};

enum libdecor_resize_edge {
	LIBDECOR_RESIZE_EDGE_NONE,
};

struct libdecor_state *
libdecor_state_new(int width,
		   int height);

void
libdecor_state_free(struct libdecor_state *state);

/**
 * Get the expected size of the content for this configuration.
 *
 * If the configuration doesn't contain a size, false is returned.
 */
