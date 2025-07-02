#include <kore3/gpu/device.h>
#include <kore3/system.h>

#include <kong.h>

#include <assert.h>
#include <stdlib.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

// This tests writing into buffers that are still in use.
// Kore handles this automatically and waits for buffers that are in use
// but please avoid it in real life - waiting is not good for frame times.

static kore_gpu_device       device;
static kore_gpu_command_list list;
static vertex_in_buffer      vertices;
static kore_gpu_buffer       indices;

static const int width  = 800;
static const int height = 600;

static void update(void *data) {
	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	kore_gpu_color clear_color = {
	    .r = 0.0f,
	    .g = 0.0f,
	    .b = 0.0f,
	    .a = 1.0f,
	};

	kore_gpu_render_pass_parameters parameters = {
	    .color_attachments_count = 1,
	    .color_attachments =
	        {
	            {
	                .load_op     = KORE_GPU_LOAD_OP_CLEAR,
	                .clear_value = clear_color,
	                .texture =
	                    {
	                        .texture           = framebuffer,
	                        .array_layer_count = 1,
	                        .mip_level_count   = 1,
	                        .format            = kore_gpu_device_framebuffer_format(&device),
	                        .dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D,
	                    },
	            },
	        },
	};

	for (uint32_t draw_index = 0; draw_index < 4; ++draw_index) {
		if (draw_index > 0) {
			parameters.color_attachments[0].load_op = KORE_GPU_LOAD_OP_LOAD;
		}

		kore_gpu_command_list_begin_render_pass(&list, &parameters);

		kong_set_render_pipeline_pipeline(&list);

		{
			vertex_in *v = kong_vertex_in_buffer_lock(&vertices);

			v[0].pos.x = -1.0f + 0.5f * draw_index;
			v[0].pos.y = -0.5f;
			v[0].pos.z = 0.5f;

			v[1].pos.x = -0.5f + 0.5f * draw_index;
			v[1].pos.y = -0.5f;
			v[1].pos.z = 0.5f;

			v[2].pos.x = -1.0f + 0.5f * draw_index;
			v[2].pos.y = 0.5f;
			v[2].pos.z = 0.5f;

			kong_vertex_in_buffer_unlock(&vertices);
		}

		kong_set_vertex_buffer_vertex_in(&list, &vertices);

		{
			uint16_t *i = (uint16_t *)kore_gpu_buffer_lock_all(&indices);

			i[0] = 0;
			i[1] = 1;
			i[2] = 2;

			kore_gpu_buffer_unlock(&indices);
		}

		kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

		kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

		kore_gpu_command_list_end_render_pass(&list);

		kore_gpu_device_execute_command_list(&device, &list);
	}

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);
}

int kickstart(int argc, char **argv) {
	kore_init("busybuffer", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	kong_create_buffer_vertex_in(&device, 3, &vertices);

	kore_gpu_buffer_parameters params = {
	    .size        = 3 * sizeof(uint16_t),
	    .usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &params, &indices);

	kore_start();

	return 0;
}
