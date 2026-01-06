#include <kore3/framebuffer/framebuffer.h>
#include <kore3/system.h>

#include <assert.h>
#include <stdlib.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static const uint32_t width  = 800;
static const uint32_t height = 600;

static uint32_t frame = 0;

static void update(void *data) {
	kore_fb_begin();

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			kore_fb_set_pixel(x, y, (frame + x) % 256, (frame + y) % 256, 0);
		}
	}

	kore_fb_end();

	++frame;
}

int kickstart(int argc, char **argv) {
	kore_init("framebuffer", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_fb_init();

	kore_start();

	return 0;
}
