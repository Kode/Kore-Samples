#include <kore3/io/filereader.h>
#include <kore3/math/matrix.h>
#include <kore3/system.h>

#include <kong_cpu_comp.h>

#include <kong.h>

#include <assert.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kore_gpu_device       device;
static kore_gpu_command_list list;
static vertex_in_buffer      vertices;
static kore_gpu_buffer       indices;
static kore_gpu_buffer       constants;
static kore_gpu_buffer       compute_constants;
static kore_gpu_buffer       buffer_source_texture;
static kore_gpu_buffer       buffer_texture;
static everything_set        everything;
static compute_set           compute;
static kore_gpu_buffer       image_buffer;

static const int width  = 800;
static const int height = 600;

static const uint64_t buffer_size = 256 * 256 * sizeof(kore_float4);

void update(void *data) {
	constants_type *constants_data = constants_type_buffer_lock(&constants, 0, 1);
	kore_matrix3x3  matrix         = kore_matrix3x3_rotation_z(0);
	constants_data->mvp            = matrix;
	constants_type_buffer_unlock(&constants);

	compute_constants_type *compute_constants_data = compute_constants_type_buffer_lock(&compute_constants, 0, 1);
	compute_constants_data->roll                   = 0;
	compute_constants_type_buffer_unlock(&compute_constants);

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	kore_float4 *pixel = (kore_float4 *)kore_gpu_buffer_lock_all(&buffer_source_texture);

	compute_constants_type compute_data;
	compute_data.roll = 0;
	set_compute_constants(&compute_data);
	set_comp_texture(pixel);
	comp_on_cpu(256 / 16, 256 / 16, 1);

	kore_gpu_buffer_unlock(&buffer_source_texture);

	kore_gpu_command_list_copy_buffer_to_buffer(&list, &buffer_source_texture, 0, &buffer_texture, 0, buffer_size);

	kore_gpu_render_pass_parameters parameters = {0};
	parameters.color_attachments_count         = 1;
	parameters.color_attachments[0].load_op    = KORE_GPU_LOAD_OP_CLEAR;
	kore_gpu_color clear_color;
	clear_color.r                                             = 0.0f;
	clear_color.g                                             = 0.0f;
	clear_color.b                                             = 0.25f;
	clear_color.a                                             = 1.0f;
	parameters.color_attachments[0].clear_value               = clear_color;
	parameters.color_attachments[0].texture.texture           = framebuffer;
	parameters.color_attachments[0].texture.array_layer_count = 1;
	parameters.color_attachments[0].texture.mip_level_count   = 1;
	parameters.color_attachments[0].texture.format            = KORE_GPU_TEXTURE_FORMAT_BGRA8_UNORM;
	parameters.color_attachments[0].texture.dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D;
	kore_gpu_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline_pipeline(&list);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

	kong_set_descriptor_set_everything(&list, &everything);

	kore_gpu_command_list_draw_indexed(&list, 3, 1, 0, 0, 0);

	kore_gpu_command_list_end_render_pass(&list);

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);
}

int kickstart(int argc, char **argv) {
	kore_init("computeshader_cpu", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	const kore_gpu_buffer_parameters buffer_source_texture_parameters = {
	    .size        = buffer_size,
	    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &buffer_source_texture_parameters, &buffer_source_texture);

	const kore_gpu_buffer_parameters buffer_texture_parameters = {
	    .size        = buffer_size,
	    .usage_flags = KORE_GPU_BUFFER_USAGE_READ_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &buffer_texture_parameters, &buffer_texture);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	kong_create_buffer_vertex_in(&device, 3, &vertices);
	{
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
	}

	kore_gpu_buffer_parameters params;
	params.size        = 3 * sizeof(uint16_t);
	params.usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE;
	kore_gpu_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *id = (uint16_t *)kore_gpu_buffer_lock_all(&indices);
		id[0]        = 0;
		id[1]        = 1;
		id[2]        = 2;
		kore_gpu_buffer_unlock(&indices);
	}

	constants_type_buffer_create(&device, &constants, 1);

	{
		everything_parameters parameters = {0};
		parameters.constants             = &constants;
		parameters.comp_texture          = &buffer_texture;
		kong_create_everything_set(&device, &parameters, &everything);
	}

	compute_constants_type_buffer_create(&device, &compute_constants, 1);

	{
		compute_parameters parameters = {0};
		parameters.compute_constants  = &compute_constants;
		parameters.comp_texture       = &buffer_texture;
		kong_create_compute_set(&device, &parameters, &compute);
	}

	kore_start();

	return 0;
}
