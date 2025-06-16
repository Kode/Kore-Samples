#include <kore3/image.h>
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
static fs_vertex_in_buffer   vertices_fs;
static kore_gpu_buffer       indices;
static kore_gpu_texture      texture;
static kore_gpu_sampler      sampler;
static fs_set                set;
static kore_gpu_buffer       image_buffer0;
static kore_gpu_buffer       image_buffer1;

static const int width  = 800;
static const int height = 600;

static bool first = true;

static void update(void *data) {
	if (first) {
		{
			kore_gpu_image_copy_buffer source = {
			    .bytes_per_row  = kore_gpu_device_align_texture_row_bytes(&device, 512 * 4),
			    .rows_per_image = 512,
			    .buffer         = &image_buffer0,
			};

			kore_gpu_image_copy_texture destination = {
			    .mip_level = 0,
			    .texture   = &texture,
			};

			kore_gpu_command_list_copy_buffer_to_texture(&list, &source, &destination, 512, 512, 1);
		}

		{
			kore_gpu_image_copy_buffer source = {
			    .bytes_per_row  = kore_gpu_device_align_texture_row_bytes(&device, 256 * 4),
			    .rows_per_image = 256,
			    .buffer         = &image_buffer1,
			};

			kore_gpu_image_copy_texture destination = {
			    .mip_level = 1,
			    .texture   = &texture,
			};

			kore_gpu_command_list_copy_buffer_to_texture(&list, &source, &destination, 256, 256, 1);
		}

		first = false;
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
		                .texture.texture           = framebuffer,
		                .texture.array_layer_count = 1,
		                .texture.mip_level_count   = 1,
		                .texture.format            = KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM,
		                .texture.dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D,
		            },
		        },
		    .color_attachments_count = 1,
		};
		kore_gpu_command_list_begin_render_pass(&list, &parameters);

		kong_set_render_pipeline_fs_pipeline(&list);

		kong_set_descriptor_set_fs(&list, &set);

		kong_set_vertex_buffer_fs_vertex_in(&list, &vertices_fs);

		kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

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
	kore_init("14_set_mipmap", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	{
		kore_gpu_buffer_parameters buffer_parameters = {
		    .size        = kore_gpu_device_align_texture_row_bytes(&device, 512 * 4) * 512,
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE | KORE_GPU_BUFFER_USAGE_COPY_SRC,
		};
		kore_gpu_device_create_buffer(&device, &buffer_parameters, &image_buffer0);

		kore_image image;
		kore_image_init_from_file_with_stride(&image, kore_gpu_buffer_lock_all(&image_buffer0), "uvtemplate.png",
		                                      kore_gpu_device_align_texture_row_bytes(&device, 512 * 4));
		kore_image_destroy(&image);
		kore_gpu_buffer_unlock(&image_buffer0);
	}

	{
		kore_gpu_buffer_parameters buffer_parameters = {
		    .size        = kore_gpu_device_align_texture_row_bytes(&device, 256 * 4) * 256,
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE | KORE_GPU_BUFFER_USAGE_COPY_SRC,
		};
		kore_gpu_device_create_buffer(&device, &buffer_parameters, &image_buffer1);

		kore_image image;
		kore_image_init_from_file_with_stride(&image, kore_gpu_buffer_lock_all(&image_buffer1), "uvtemplate2.png",
		                                      kore_gpu_device_align_texture_row_bytes(&device, 256 * 4));
		kore_image_destroy(&image);
		kore_gpu_buffer_unlock(&image_buffer1);
	}

	{
		kore_gpu_texture_parameters texture_parameters = {
		    .width                 = 512,
		    .height                = 512,
		    .depth_or_array_layers = 1,
		    .mip_level_count       = 2,
		    .sample_count          = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_COPY_DST | fs_texture_texture_usage_flags(),
		};
		kore_gpu_device_create_texture(&device, &texture_parameters, &texture);
	}

	kore_gpu_sampler_parameters sampler_params = {
	    .lod_min_clamp  = 0,
	    .lod_max_clamp  = 1,
	    .max_anisotropy = 1,
	};
	kore_gpu_device_create_sampler(&device, &sampler_params, &sampler);

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

	fs_parameters fsparams = {
	    .fs_texture =
	        {
	            .texture           = &texture,
	            .base_mip_level    = 0,
	            .mip_level_count   = 2,
	            .base_array_layer  = 0,
	            .array_layer_count = 1,
	        },
	    .fs_sampler = &sampler,
	};
	kong_create_fs_set(&device, &fsparams, &set);

	kore_start();

	return 0;
}
