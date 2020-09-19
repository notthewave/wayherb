//Testing area
#include <linux/input-event-codes.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include "wayland.h"
#include "config.h"
#include "util.h"

//Local globals
static struct wayland_state wayland;
static uint32_t output = UINT32_MAX;
static int cur_x = -1, cur_y = -1;
static int button = 0;
static uint32_t layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
static bool keyboard_interactive = false;

/* Listeners and handles */

//Layer_shell listener

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t w, uint32_t h)
{

	zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
	zwlr_layer_surface_v1_destroy(surface);
	wl_surface_destroy(wayland.wl_surface);
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

//Pointer listeners and events
static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	wayland.input_surface = surface;
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface)
{
	cur_x = cur_y = -1;
	button = 0;
}	

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	cur_x = wl_fixed_to_int(surface_x);
	cur_y = wl_fixed_to_int(surface_y);
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	if (wayland.input_surface == wayland.wl_surface) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			if (button == BTN_RIGHT) {
				pid_t pid = getpid();
				kill(pid, SIGUSR2);	
			} else if (button == BTN_LEFT) {
				pid_t pid = getpid();
				kill(pid, SIGUSR1);
			}
		}
	}
}

struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
};

//Seat listener
static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat, enum wl_seat_capability caps)
{
	if((caps & WL_SEAT_CAPABILITY_POINTER)) {
		wayland.pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(wayland.pointer, &pointer_listener, NULL);
	}
}

const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
};

//Registry
void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		wayland.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		wayland.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, "wl_output") == 0) {
		if (output != UINT32_MAX) {
			if (!wayland.wl_output) {
				wayland.wl_output = wl_registry_bind(registry, name, &wl_output_interface, 1);
			} else {
				output --;
			}
		}
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		wayland.seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(wayland.seat, &seat_listener, NULL);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		wayland.layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
	}

}

void registry_handle_remove(void *data, struct wl_registry *registry, uint32_t name)
{


}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_handle_remove,
};

/* Wayland intialization, freeing, and main loop for drawing and input. */
int draw()
{
	
	if (wl_display_dispatch_pending(wayland.display) == -1)
		return 1;
	
	
	return 0;
}

int init_wayland(void)
{
	//Some variables for setting up our layer_surface
	char *namespace = "herbew";
	int exclusive_zone = height;

	//Connect to the wayland server
	wayland.display = wl_display_connect(NULL);
	if (!wayland.display)
		die("Failed to create display\n");

	//Set up our registry for getting events
	struct wl_registry *registry = wl_display_get_registry(wayland.display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(wayland.display);
	
	if (!wayland.compositor)
		die("No compositor available\n");

	if (!wayland.shm)
		die("Shared memory buffer not available\n");

	if (!wayland.layer_shell)
		die("Layer shell not available\n");
	
	//Cursor image for being able to see the cursor over our notifcations
	wayland.cursor_theme = wl_cursor_theme_load(NULL, 16, wayland.shm);
	wayland.cursor_surface = wl_compositor_create_surface(wayland.compositor);
	
	wayland.wl_surface = wl_compositor_create_surface(wayland.compositor);
	wayland.layer_surface = zwlr_layer_shell_v1_get_layer_surface(wayland.layer_shell, wayland.wl_surface, wayland.wl_output, layer, namespace);

	//Configure how layer surface acts
	zwlr_layer_surface_v1_set_size(wayland.layer_surface, width, height);
	zwlr_layer_surface_v1_set_anchor(wayland.layer_surface, anchor);
	zwlr_layer_surface_v1_set_exclusive_zone(wayland.layer_surface, exclusive_zone);
	zwlr_layer_surface_v1_set_margin(wayland.layer_surface, margin_top, margin_right, margin_bottom, margin_left);
	zwlr_layer_surface_v1_set_keyboard_interactivity(wayland.layer_surface, keyboard_interactive);
	zwlr_layer_surface_v1_add_listener(wayland.layer_surface, &layer_surface_listener, wayland.layer_surface);

	wl_surface_commit(wayland.wl_surface);
	wl_display_roundtrip(wayland.display);

	fprintf(stdout, "Finished init wayland\n");

	return 1;
}

int free_wayland(void)
{

	return 1;
}
