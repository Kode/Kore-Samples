#include <kore3/image.h>
#include <kore3/io/filereader.h>
#include <kore3/system.h>

#include <kore3/gpu/device.h>

#include <kong.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kore_gpu_device       device;
static kore_gpu_command_list list;
static vertex_in_buffer      vertices;
static kore_gpu_buffer       indices;
static kore_gpu_buffer       image_buffers[9];
static kore_gpu_texture      textures[9];
static kore_gpu_sampler      sampler;
static kore_gpu_buffer       constants;
static everything_set        everything;

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
	return (float)kore_time();
#endif
}

static void update(void *data) {
	kore_matrix3x3 matrix = kore_matrix3x3_rotation_z(time());

	constants_type *constants_data = constants_type_buffer_lock(&constants, update_index % KORE_GPU_MAX_FRAMEBUFFERS, 1);
	constants_data->mvp            = matrix;
	constants_type_buffer_unlock(&constants);

	if (first_update) {
		for (int i = 0; i < 9; ++i) {
			kore_gpu_image_copy_buffer source = {
			    .buffer        = &image_buffers[i],
			    .bytes_per_row = kore_gpu_device_align_texture_row_bytes(&device, image_width * 4),
			    .offset        = 0,
			};

			kore_gpu_image_copy_texture destination = {
			    .texture   = &textures[i],
			    .mip_level = 0,
			};

			kore_gpu_command_list_copy_buffer_to_texture(&list, &source, &destination, image_width, image_height, 1);
		}
	}

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	kore_gpu_render_pass_parameters parameters = {
	    .color_attachments_count = 1,
	    .color_attachments =
	        {
	            {
	                .load_op = KORE_GPU_LOAD_OP_CLEAR,
	                .clear_value =
	                    {
	                        .r = 0.0f,
	                        .g = 0.0f,
	                        .b = 0.0f,
	                        .a = 1.0f,
	                    },
	                .texture =
	                    {
	                        .texture           = framebuffer,
	                        .array_layer_count = 1,
	                        .mip_level_count   = 1,
	                        .format            = KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM,
	                        .dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D,
	                    },
	            },
	        },
	};
	kore_gpu_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline_pipeline(&list);

	kong_set_descriptor_set_everything(&list, &everything, update_index % KORE_GPU_MAX_FRAMEBUFFERS);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

	kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

	kore_gpu_command_list_end_render_pass(&list);

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);

	if (first_update) {
		for (int i = 0; i < 9; ++i) {
			kore_gpu_buffer_destroy(&image_buffers[i]);
		}
		first_update = false;
	}

	update_index += 1;
}

int kickstart(int argc, char **argv) {
	kore_init("bindless", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	for (int i = 0; i < 9; ++i) {
		kore_gpu_buffer_parameters buffer_parameters = {
		    .size        = kore_gpu_device_align_texture_row_bytes(&device, image_width * 4) * image_height,
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE,
		};
		kore_gpu_device_create_buffer(&device, &buffer_parameters, &image_buffers[i]);

		char image_name[64];
		sprintf(image_name, "%i.png", i + 1);

		kore_image image;
		kore_image_init_from_file_with_stride(&image, kore_gpu_buffer_lock_all(&image_buffers[i]), image_name,
		                                      kore_gpu_device_align_texture_row_bytes(&device, image_width * 4));
		kore_image_destroy(&image);
		kore_gpu_buffer_unlock(&image_buffers[i]);

		kore_gpu_texture_parameters texture_parameters = {
		    .width                 = image_width,
		    .height                = image_height,
		    .depth_or_array_layers = 1,
		    .mip_level_count       = 1,
		    .sample_count          = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_COPY_DST | textures_texture_usage_flags(),
		};
		kore_gpu_device_create_texture(&device, &texture_parameters, &textures[i]);
	}

	kore_gpu_sampler_parameters sampler_parameters = {
	    .address_mode_u = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .address_mode_v = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .address_mode_w = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .mag_filter     = KORE_GPU_FILTER_MODE_LINEAR,
	    .min_filter     = KORE_GPU_FILTER_MODE_LINEAR,
	    .mipmap_filter  = KORE_GPU_MIPMAP_FILTER_MODE_NEAREST,
	    .lod_min_clamp  = 1,
	    .lod_max_clamp  = 32,
	    .compare        = KORE_GPU_COMPARE_FUNCTION_NEVER,
	    .max_anisotropy = 1,
	};
	kore_gpu_device_create_sampler(&device, &sampler_parameters, &sampler);

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

	kore_gpu_buffer_parameters params = {
	    params.size        = 3 * sizeof(uint16_t),
	    params.usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *i = (uint16_t *)kore_gpu_buffer_lock_all(&indices);

		i[0] = 0;
		i[1] = 1;
		i[2] = 2;

		kore_gpu_buffer_unlock(&indices);
	}

	constants_type_buffer_create(&device, &constants, KORE_GPU_MAX_FRAMEBUFFERS);

	{
		everything_parameters parameters = {
		    .constants      = &constants,
		    .sam            = &sampler,
		    .textures_count = 9,
		    .textures       = calloc(9, sizeof(kore_gpu_texture_view)),
		};
		for (int i = 0; i < 9; ++i) {
			parameters.textures[i].texture           = &textures[i];
			parameters.textures[i].base_mip_level    = 0;
			parameters.textures[i].mip_level_count   = 1;
			parameters.textures[i].array_layer_count = 1;
		}
		kong_create_everything_set(&device, &parameters, &everything);
	}

	kore_start();

	kong_destroy_everything_set(&everything);
	constants_type_buffer_destroy(&constants);
	kore_gpu_buffer_destroy(&indices);
	kong_destroy_buffer_vertex_in(&vertices);
	for (int i = 0; i < 9; ++i) {
		kore_gpu_texture_destroy(&textures[i]);
	}
	kore_gpu_command_list_destroy(&list);
	kore_gpu_device_destroy(&device);

	return 0;
}
