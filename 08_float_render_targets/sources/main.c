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
static kore_gpu_texture      float_render_target;
static kore_gpu_texture      render_target; // intermediate target because D3D12 doesn't seem to like uav framebuffer access
static compute_set           set;

const static int width  = 800;
const static int height = 600;

static void update(void *data) {
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
	                        .texture           = &float_render_target,
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

	kong_set_render_pipeline_pipeline(&list);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

	kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

	kore_gpu_command_list_end_render_pass(&list);

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	kong_set_compute_shader_comp(&list);

	kong_set_descriptor_set_compute(&list, &set);

	copy_constants_type copy_constants;
	copy_constants.size.x = width;
	copy_constants.size.y = height;
	kong_set_root_constants_copy_constants(&list, &copy_constants);

	kore_gpu_command_list_compute(&list, (width + 7) / 8, (height + 7) / 8, 1);

	kore_gpu_image_copy_texture source = {0};
	source.texture                     = &render_target;

	kore_gpu_image_copy_texture destination = {0};
	destination.texture                     = framebuffer;

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width, height, 1);

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif
}

int kickstart(int argc, char **argv) {
	kore_init("08_float_render_targets", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	{
		kore_gpu_texture_parameters texture_parameters;
		texture_parameters.width                 = width;
		texture_parameters.height                = height;
		texture_parameters.depth_or_array_layers = 1;
		texture_parameters.mip_level_count       = 1;
		texture_parameters.sample_count          = 1;
		texture_parameters.dimension             = KORE_GPU_TEXTURE_DIMENSION_2D;
		texture_parameters.format                = KORE_GPU_TEXTURE_FORMAT_RGBA32_FLOAT;
		texture_parameters.usage                 = KORE_GPU_TEXTURE_USAGE_RENDER_ATTACHMENT | copy_source_texture_texture_usage_flags();
		kore_gpu_device_create_texture(&device, &texture_parameters, &float_render_target);
	}

	{
		kore_gpu_texture_parameters texture_parameters;
		texture_parameters.width                 = width;
		texture_parameters.height                = height;
		texture_parameters.depth_or_array_layers = 1;
		texture_parameters.mip_level_count       = 1;
		texture_parameters.sample_count          = 1;
		texture_parameters.dimension             = KORE_GPU_TEXTURE_DIMENSION_2D;
		texture_parameters.format                = kore_gpu_device_framebuffer_format(&device);
		texture_parameters.usage                 = KORE_GPU_TEXTURE_USAGE_COPY_SRC | copy_destination_texture_texture_usage_flags();
		kore_gpu_device_create_texture(&device, &texture_parameters, &render_target);
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

	compute_parameters cparams                         = {0};
	cparams.copy_source_texture.texture                = &float_render_target;
	cparams.copy_source_texture.base_mip_level         = 0;
	cparams.copy_source_texture.mip_level_count        = 1;
	cparams.copy_source_texture.base_array_layer       = 0;
	cparams.copy_source_texture.array_layer_count      = 1;
	cparams.copy_destination_texture.texture           = &render_target;
	cparams.copy_destination_texture.base_mip_level    = 0;
	cparams.copy_destination_texture.mip_level_count   = 1;
	cparams.copy_destination_texture.base_array_layer  = 0;
	cparams.copy_destination_texture.array_layer_count = 1;
	kong_create_compute_set(&device, &cparams, &set);

	kore_start();

	return 0;
}
