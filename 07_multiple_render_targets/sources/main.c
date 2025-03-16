#include <kore3/gpu/buffer.h>
#include <kore3/gpu/gpu.h>
#include <kore3/io/filereader.h>
#include <kore3/system.h>

#include <kong.h>

#include <assert.h>
#include <stdlib.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kore_gpu_device       device;
static kore_gpu_command_list list;
static vertex_in_buffer      vertices;
static kore_gpu_buffer       indices;
static kore_gpu_texture      render_targets[4];

static const int width  = 800;
static const int height = 600;

static void update(void *data) {
	kore_gpu_render_pass_parameters parameters = {
	    .color_attachments_count = 4,
	};
	for (uint32_t i = 0; i < 4; ++i) {
		kore_gpu_color clear_color = {
		    .r = 0.0f,
		    .g = 0.0f,
		    .b = 0.0f,
		    .a = 1.0f,
		};

		parameters.color_attachments[i].load_op                   = KORE_GPU_LOAD_OP_CLEAR;
		parameters.color_attachments[i].clear_value               = clear_color;
		parameters.color_attachments[i].texture.texture           = &render_targets[i];
		parameters.color_attachments[i].texture.array_layer_count = 1;
		parameters.color_attachments[i].texture.mip_level_count   = 1;
		parameters.color_attachments[i].texture.format            = KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM;
		parameters.color_attachments[i].texture.dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D;
	}
	kore_gpu_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline_pipeline(&list);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0, 3 * sizeof(uint16_t));

	kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

	kore_gpu_command_list_end_render_pass(&list);

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	kore_gpu_image_copy_texture destination = {
	    .texture   = framebuffer,
	    .aspect    = KORE_GPU_IMAGE_COPY_ASPECT_ALL,
	    .mip_level = 0,
	    .origin_x  = 0,
	    .origin_y  = 0,
	    .origin_z  = 0,
	};

	kore_gpu_image_copy_texture source = {
	    .texture   = &render_targets[0],
	    .aspect    = KORE_GPU_IMAGE_COPY_ASPECT_ALL,
	    .mip_level = 0,
	    .origin_x  = 0,
	    .origin_y  = 0,
	    .origin_z  = 0,
	};

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width / 2, height / 2, 1);

	destination.origin_x = width / 2;
	destination.origin_y = 0;
	source.texture       = &render_targets[1];

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width / 2, height / 2, 1);

	destination.origin_x = 0;
	destination.origin_y = height / 2;
	source.texture       = &render_targets[2];

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width / 2, height / 2, 1);

	destination.origin_x = width / 2;
	destination.origin_y = height / 2;
	source.texture       = &render_targets[3];

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width / 2, height / 2, 1);

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);
}

int kickstart(int argc, char **argv) {
	kore_init("Example", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	for (uint32_t i = 0; i < 4; ++i) {
		kore_gpu_texture_parameters texture_parameters = {
		    .width                 = width / 2,
		    .height                = height / 2,
		    .depth_or_array_layers = 1,
		    .mip_level_count       = 1,
		    .sample_count          = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_RENDER_ATTACHMENT | KORE_GPU_TEXTURE_USAGE_COPY_SRC,
		};
		kore_gpu_device_create_texture(&device, &texture_parameters, &render_targets[i]);
	}

	kong_create_buffer_vertex_in(&device, 3, &vertices);
	{
		vertex_in *v = kong_vertex_in_buffer_lock(&vertices);

		v[0].pos.x = -1.0;
		v[0].pos.y = -1.0;
		v[0].pos.z = 0.0;

		v[1].pos.x = 1.0;
		v[1].pos.y = -1.0;
		v[1].pos.z = 0.0;

		v[2].pos.x = 0.0;
		v[2].pos.y = 1.0;
		v[2].pos.z = 0.0;

		kong_vertex_in_buffer_unlock(&vertices);
	}

	kore_gpu_buffer_parameters params = {
	    .size        = 3 * sizeof(uint16_t),
	    .usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *i = (uint16_t *)kore_gpu_buffer_lock_all(&indices);

		i[0] = 0;
		i[1] = 1;
		i[2] = 2;

		kore_gpu_buffer_unlock(&indices);
	}

	kore_start();

	return 0;
}
