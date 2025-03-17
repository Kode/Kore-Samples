#include <kinc/image.h>
#include <kinc/io/filereader.h>
#include <kinc/system.h>

#include <kope/graphics5/device.h>

#include <kong.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kope_g5_device       device;
static kope_g5_command_list list;
static vertex_in_buffer     vertices;
static kope_g5_buffer       indices;
static kope_g5_buffer       image_buffers[9];
static kope_g5_texture      textures[9];
static kope_g5_sampler      sampler;
static kope_g5_buffer       constants;
static everything_set       everything;

static const int width  = 800;
static const int height = 600;

static const int image_width  = 213;
static const int image_height = 214;

static bool     first_update = true;
static uint64_t update_index = 0;

static float time(void) {
#ifdef SCREENSHOT
	return 0.3f;
#else
	return (float)kinc_time();
#endif
}

static void update(void *data) {
	kinc_matrix3x3_t matrix = kinc_matrix3x3_rotation_z(time());

	constants_type *constants_data = constants_type_buffer_lock(&constants, update_index % KOPE_G5_MAX_FRAMEBUFFERS, 1);
	constants_data->mvp            = matrix;
	constants_type_buffer_unlock(&constants);

	if (first_update) {
		for (int i = 0; i < 9; ++i) {
			kope_g5_image_copy_buffer source = {0};
			source.buffer                    = &image_buffers[i];
			source.bytes_per_row             = kope_g5_device_align_texture_row_bytes(&device, image_width * 4);
			source.offset                    = 0;

			kope_g5_image_copy_texture destination = {0};
			destination.texture                    = &textures[i];
			destination.mip_level                  = 0;

			kope_g5_command_list_copy_buffer_to_texture(&list, &source, &destination, image_width, image_height, 1);
		}
		first_update = false;
	}

	kope_g5_texture *framebuffer = kope_g5_device_get_framebuffer(&device);

	kope_g5_render_pass_parameters parameters = {0};
	parameters.color_attachments_count        = 1;
	parameters.color_attachments[0].load_op   = KOPE_G5_LOAD_OP_CLEAR;
	kope_g5_color clear_color;
	clear_color.r                                             = 0.0f;
	clear_color.g                                             = 0.0f;
	clear_color.b                                             = 0.0f;
	clear_color.a                                             = 1.0f;
	parameters.color_attachments[0].clear_value               = clear_color;
	parameters.color_attachments[0].texture.texture           = framebuffer;
	parameters.color_attachments[0].texture.array_layer_count = 1;
	parameters.color_attachments[0].texture.mip_level_count   = 1;
	parameters.color_attachments[0].texture.format            = KOPE_G5_TEXTURE_FORMAT_BGRA8_UNORM;
	parameters.color_attachments[0].texture.dimension         = KOPE_G5_TEXTURE_VIEW_DIMENSION_2D;
	kope_g5_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline(&list, &pipeline);

	kong_set_descriptor_set_everything(&list, &everything, update_index % KOPE_G5_MAX_FRAMEBUFFERS);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kope_g5_command_list_set_index_buffer(&list, &indices, KOPE_G5_INDEX_FORMAT_UINT16, 0, 3 * sizeof(uint16_t));

	kope_g5_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

	kope_g5_command_list_end_render_pass(&list);

	kope_g5_command_list_present(&list);

	kope_g5_device_execute_command_list(&device, &list);

	update_index += 1;

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif
}

int kickstart(int argc, char **argv) {
	kinc_init("bindless", width, height, NULL, NULL);
	kinc_set_update_callback(update, NULL);

	kope_g5_device_wishlist wishlist = {0};
	kope_g5_device_create(&device, &wishlist);

	kong_init(&device);

	kope_g5_device_create_command_list(&device, KOPE_G5_COMMAND_LIST_TYPE_GRAPHICS, &list);

	for (int i = 0; i < 9; ++i) {
		kope_g5_buffer_parameters buffer_parameters = {0};
		buffer_parameters.size                      = kope_g5_device_align_texture_row_bytes(&device, image_width * 4) * image_height;
		buffer_parameters.usage_flags               = KOPE_G5_BUFFER_USAGE_CPU_WRITE;
		kope_g5_device_create_buffer(&device, &buffer_parameters, &image_buffers[i]);

		char image_name[64];
		sprintf(image_name, "%i.png", i + 1);

		kinc_image_t image;
		kinc_image_init_from_file_with_stride(&image, kope_g5_buffer_lock_all(&image_buffers[i]), image_name,
		                                      kope_g5_device_align_texture_row_bytes(&device, image_width * 4));
		kinc_image_destroy(&image);
		kope_g5_buffer_unlock(&image_buffers[i]);

		kope_g5_texture_parameters texture_parameters = {0};
		texture_parameters.width                      = image_width;
		texture_parameters.height                     = image_height;
		texture_parameters.depth_or_array_layers      = 1;
		texture_parameters.mip_level_count            = 1;
		texture_parameters.sample_count               = 1;
		texture_parameters.dimension                  = KOPE_G5_TEXTURE_DIMENSION_2D;
		texture_parameters.format                     = KOPE_G5_TEXTURE_FORMAT_RGBA8_UNORM;
		texture_parameters.usage                      = KONG_G5_TEXTURE_USAGE_SAMPLE | KONG_G5_TEXTURE_USAGE_COPY_DST;
		kope_g5_device_create_texture(&device, &texture_parameters, &textures[i]);
	}

	kope_g5_sampler_parameters sampler_parameters = {0};
	sampler_parameters.address_mode_u             = KOPE_G5_ADDRESS_MODE_REPEAT;
	sampler_parameters.address_mode_v             = KOPE_G5_ADDRESS_MODE_REPEAT;
	sampler_parameters.address_mode_w             = KOPE_G5_ADDRESS_MODE_REPEAT;
	sampler_parameters.mag_filter                 = KOPE_G5_FILTER_MODE_LINEAR;
	sampler_parameters.min_filter                 = KOPE_G5_FILTER_MODE_LINEAR;
	sampler_parameters.mipmap_filter              = KOPE_G5_MIPMAP_FILTER_MODE_NEAREST;
	sampler_parameters.lod_min_clamp              = 1;
	sampler_parameters.lod_max_clamp              = 32;
	sampler_parameters.compare                    = KOPE_G5_COMPARE_FUNCTION_ALWAYS;
	sampler_parameters.max_anisotropy             = 1;
	kope_g5_device_create_sampler(&device, &sampler_parameters, &sampler);

	kong_create_buffer_vertex_in(&device, 3, &vertices);
	vertex_in *v = kong_vertex_in_buffer_lock(&vertices);

	v[0].pos.x = -1.0f;
	v[0].pos.y = -1.0f;
	v[0].pos.z = 0.5f;

	v[0].tex.x = 0.0f;
	v[0].tex.y = 1.0f;

	v[1].pos.x = 1.0f;
	v[1].pos.y = -1.0f;
	v[1].pos.z = 0.5f;

	v[1].tex.x = 1.0f;
	v[1].tex.y = 1.0f;

	v[2].pos.x = -1.0f;
	v[2].pos.y = 1.0f;
	v[2].pos.z = 0.5f;

	v[2].tex.x = 0.0f;
	v[2].tex.y = 0.0f;

	kong_vertex_in_buffer_unlock(&vertices);

	kope_g5_buffer_parameters params;
	params.size        = 3 * sizeof(uint16_t);
	params.usage_flags = KOPE_G5_BUFFER_USAGE_INDEX | KOPE_G5_BUFFER_USAGE_CPU_WRITE;
	kope_g5_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *i = (uint16_t *)kope_g5_buffer_lock_all(&indices);
		i[0]        = 0;
		i[1]        = 1;
		i[2]        = 2;
		kope_g5_buffer_unlock(&indices);
	}

	constants_type_buffer_create(&device, &constants, KOPE_G5_MAX_FRAMEBUFFERS);

	{
		everything_parameters parameters = {0};
		parameters.constants             = &constants;
		parameters.textures              = calloc(9, sizeof(kope_g5_texture_view));
		for (int i = 0; i < 9; ++i) {
			parameters.textures[i].texture           = &textures[i];
			parameters.textures[i].base_mip_level    = 0;
			parameters.textures[i].mip_level_count   = 1;
			parameters.textures[i].array_layer_count = 1;
		}
		parameters.textures_count = 9;
		parameters.sam            = &sampler;
		kong_create_everything_set(&device, &parameters, &everything);
	}

	kinc_start();

	return 0;
}
