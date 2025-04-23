#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <kore3/util/align.h>

#ifdef KORE_EMSCRIPTEN
#include <emscripten.h>

static uint8_t *png_data      = NULL;
static int      png_data_size = 0;

static void png_write_func(void *context, void *data, int size) {
	memcpy(&png_data[png_data_size], data, size);
	png_data_size += size;
}

EM_JS(void, kore_html5_download_image, (const char *filename, void *data, int size), {
	const a = document.createElement('a');
	a.style = 'display:none';
	document.body.appendChild(a);
	const view = new Uint8Array(HEAPU8.buffer, data, size);
	const blob = new Blob([view], {
	    type:
		    'octet/stream'
	});
	const url  = window.URL.createObjectURL(blob);
	a.href     = url;
	a.download = UTF8ToString(filename);
	a.click();
	window.URL.revokeObjectURL(url);
	document.body.removeChild(a);
});
#endif

static kore_gpu_buffer screenshot_buffer;

static void screenshot_take(kore_gpu_device *device, kore_gpu_command_list *list, kore_gpu_texture *framebuffer, uint32_t width, uint32_t height) {
	uint32_t row_bytes = kore_gpu_device_align_texture_row_bytes(device, width * 4);

	kore_gpu_buffer_parameters buffer_parameters = {
	    .size        = row_bytes * height,
	    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_READ | KORE_GPU_BUFFER_USAGE_COPY_DST,
	};
	kore_gpu_device_create_buffer(device, &buffer_parameters, &screenshot_buffer);

	kore_gpu_image_copy_texture source = {
	    .texture   = framebuffer,
	    .mip_level = 0,
	};

	kore_gpu_image_copy_buffer destination = {
	    .buffer         = &screenshot_buffer,
	    .bytes_per_row  = row_bytes,
	    .rows_per_image = height,
	};

	kore_gpu_command_list_copy_texture_to_buffer(list, &source, &destination, width, height, 1);

	kore_gpu_device_execute_command_list(device, list);

	kore_gpu_device_wait_until_idle(device);

	uint8_t *pixels = (uint8_t *)kore_gpu_buffer_lock_all(&screenshot_buffer);

	if (kore_gpu_device_framebuffer_format(device) == KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM ||
	    kore_gpu_device_framebuffer_format(device) == KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM_SRGB) {

		for (uint32_t y = 0; y < height; ++y) {
			for (uint32_t x = 0; x < width; ++x) {
				uint8_t b = pixels[y * row_bytes + x * 4 + 0];
				uint8_t r = pixels[y * row_bytes + x * 4 + 2];

				pixels[y * row_bytes + x * 4 + 0] = r;
				pixels[y * row_bytes + x * 4 + 2] = b;
			}
		}
	}

#ifdef KORE_EMSCRIPTEN
	png_data      = (uint8_t *)malloc(width * height * 4);
	png_data_size = 0;
	stbi_write_png_to_func(png_write_func, NULL, width, height, 4, pixels, row_bytes);
	kore_html5_download_image("test.png", png_data, png_data_size);
#else
	stbi_write_png("test.png", width, height, 4, pixels, row_bytes);
#endif

	kore_gpu_buffer_unlock(&screenshot_buffer);

#ifdef KORE_EMSCRIPTEN
	emscripten_force_exit(0);
#else
	exit(0);
#endif
}
