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
static fs_vertex_in_buffer   vertices_fs;
static kore_gpu_buffer       indices;
static kore_gpu_texture      render_target;
static kore_gpu_sampler      sampler;
static kore_gpu_sampler      mip_sampler;
static fs_set                set;
static mip_set               mip_sets[4];

static const int width  = 800;
static const int height = 600;

static void update(void *data) {
	{
		kore_gpu_render_pass_parameters parameters = {
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
		                        .texture           = &render_target,
		                        .array_layer_count = 1,
		                        .mip_level_count   = 1,
		                        .format            = KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM,
		                        .dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D,
		                    },
		            },
		        },
		    parameters.color_attachments_count = 1,
		};
		kore_gpu_command_list_begin_render_pass(&list, &parameters);

		kong_set_render_pipeline_pipeline(&list);

		kong_set_vertex_buffer_vertex_in(&list, &vertices);

		kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0, 3 * sizeof(uint16_t));

		kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

		kore_gpu_command_list_end_render_pass(&list);
	}

	kong_set_compute_shader_comp(&list);

	int width_height = 1024;

	for (int i = 0; i < 4; ++i) {
		width_height /= 2;

		kong_set_descriptor_set_mip(&list, &mip_sets[i]);

		mip_constants_type mip_constants = {
		    .size.x = width_height,
		    .size.y = width_height,
		};
		kong_set_root_constants_mip_constants(&list, &mip_constants);

		kore_gpu_command_list_compute(&list, (width_height + 7) / 8, (width_height + 7) / 8, 1);
	}

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	{
		kore_gpu_render_pass_parameters parameters = {
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
		    .color_attachments_count = 1,
		};
		kore_gpu_command_list_begin_render_pass(&list, &parameters);

		kong_set_render_pipeline_fs_pipeline(&list);

		kong_set_descriptor_set_fs(&list, &set);

		kong_set_vertex_buffer_fs_vertex_in(&list, &vertices_fs);

		kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0, 3 * sizeof(uint16_t));

		kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

		kore_gpu_command_list_end_render_pass(&list);
	}

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);
}

int kickstart(int argc, char **argv) {
	kore_init("13_generate_mipmaps", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	{
		kore_gpu_texture_parameters texture_parameters = {
		    .width                 = 1024,
		    .height                = 1024,
		    .depth_or_array_layers = 1,
		    .mip_level_count       = 5,
		    .sample_count          = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_RENDER_ATTACHMENT | KORE_GPU_TEXTURE_USAGE_READ_WRITE,
		};
		kore_gpu_device_create_texture(&device, &texture_parameters, &render_target);
	}

	kore_gpu_sampler_parameters sampler_params = {
	    .lod_min_clamp = 0,
	    .lod_max_clamp = 5,
	};
	kore_gpu_device_create_sampler(&device, &sampler_params, &sampler);

	kong_create_buffer_vertex_in(&device, 3, &vertices);
	{
		vertex_in *v = kong_vertex_in_buffer_lock(&vertices);

		v[0].pos.x = -1.0;
		v[0].pos.y = 1.0;
		v[0].pos.z = 0.0;

		v[1].pos.x = 1.0;
		v[1].pos.y = 1.0;
		v[1].pos.z = 0.0;

		v[2].pos.x = 0.0;
		v[2].pos.y = -1.0;
		v[2].pos.z = 0.0;

		kong_vertex_in_buffer_unlock(&vertices);
	}

	kong_create_buffer_fs_vertex_in(&device, 3, &vertices_fs);
	{
		fs_vertex_in *v = kong_fs_vertex_in_buffer_lock(&vertices_fs);

		v[0].pos.x = -1.0;
		v[0].pos.y = -1.0;

		v[1].pos.x = 3.0;
		v[1].pos.y = -1.0;

		v[2].pos.x = -1.0;
		v[2].pos.y = 3.0;

		kong_fs_vertex_in_buffer_unlock(&vertices_fs);
	}

	kore_gpu_buffer_parameters params;
	params.size        = 3 * sizeof(uint16_t);
	params.usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE;
	kore_gpu_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *i = (uint16_t *)kore_gpu_buffer_lock_all(&indices);

		i[0] = 0;
		i[1] = 1;
		i[2] = 2;

		kore_gpu_buffer_unlock(&indices);
	}

	fs_parameters fsparams = {
	    .fs_texture =
	        {
	            .texture           = &render_target,
	            .base_mip_level    = 0,
	            .mip_level_count   = 5,
	            .base_array_layer  = 0,
	            .array_layer_count = 1,
	        },
	    .fs_sampler = &sampler,
	};
	kong_create_fs_set(&device, &fsparams, &set);

	kore_gpu_sampler_parameters mip_sampler_params = {
	    .address_mode_u = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .address_mode_v = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .address_mode_w = KORE_GPU_ADDRESS_MODE_REPEAT,
	    .mag_filter     = KORE_GPU_FILTER_MODE_LINEAR,
	    .min_filter     = KORE_GPU_FILTER_MODE_LINEAR,
	    .mipmap_filter  = KORE_GPU_MIPMAP_FILTER_MODE_NEAREST,
	    .lod_min_clamp  = 0,
	    .lod_max_clamp  = 32,
	    .compare        = KORE_GPU_COMPARE_FUNCTION_ALWAYS,
	    .max_anisotropy = 1,
	};
	kore_gpu_device_create_sampler(&device, &mip_sampler_params, &mip_sampler);

	for (int i = 0; i < 4; ++i) {
		mip_parameters mip_params = {
		    .mip_source_texture =
		        {
		            .texture           = &render_target,
		            .base_mip_level    = i,
		            .mip_level_count   = 1,
		            .base_array_layer  = 0,
		            .array_layer_count = 1,
		        },
		    .mip_source_sampler = &mip_sampler,
		    .mip_destination_texture =
		        {
		            .texture           = &render_target,
		            .base_mip_level    = i + 1,
		            .mip_level_count   = 1,
		            .base_array_layer  = 0,
		            .array_layer_count = 1,
		        },
		};

		kong_create_mip_set(&device, &mip_params, &mip_sets[i]);
	}

	kore_start();

	return 0;
}
