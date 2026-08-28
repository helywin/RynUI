struct libdecor_limits {
	int min_width;
	int min_height;
	int max_width;
	int max_height;
};

struct libdecor_configuration {
	uint32_t serial;

	bool has_window_state;
	enum libdecor_window_state window_state;

	bool has_size;
	int window_width;
};

static struct libdecor_configuration *
libdecor_configuration_new(void)
{
	struct libdecor_configuration *configuration;

	configuration = zalloc(sizeof *configuration);

	return configuration;
}

static void
libdecor_configuration_free(struct libdecor_configuration *configuration)
{
	free(configuration);
}

static bool
frame_get_window_size_for(void)
{
	return true;
}

static void
xdg_surface_configure(void *user_data,
		      struct xdg_surface *xdg_surface,
		      uint32_t serial)
{
	frame_priv->iface->configure(frame,
				     configuration,
				     frame_priv->user_data);

	libdecor_configuration_free(configuration);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	xdg_surface_configure,
};

static enum libdecor_window_state
parse_states(struct wl_array *states)
{
		switch (state) {
		case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
			pending_state |= LIBDECOR_WINDOW_STATE_TILED_BOTTOM;
			break;
#ifdef HAVE_XDG_SHELL_V6
		case XDG_TOPLEVEL_STATE_SUSPENDED:
			pending_state |= LIBDECOR_WINDOW_STATE_SUSPENDED;
			break;
#endif
		}
}
