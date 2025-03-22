#include <kore3/io/filereader.h>
#include <kore3/log.h>
#include <kore3/system.h>

#include <kong.h>

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kore_gpu_device       device;
static kore_gpu_command_list list;
static vertex_in_buffer      vertices;
static kore_gpu_buffer       indices;
static kore_gpu_buffer       constants;
static everything_set        everything;

static const int width  = 800;
static const int height = 600;

float vec3_length(kore_float3 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

kore_float3 vec3_normalize(kore_float3 a) {
	float n = vec3_length(a);
	if (n > 0.0) {
		float inv_n = 1.0f / n;
		a.x *= inv_n;
		a.y *= inv_n;
		a.z *= inv_n;
	}
	return a;
}

kore_float3 vec3_sub(kore_float3 a, kore_float3 b) {
	kore_float3 v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	v.z = a.z - b.z;
	return v;
}

kore_float3 vec3_cross(kore_float3 a, kore_float3 b) {
	kore_float3 v;
	v.x = a.y * b.z - a.z * b.y;
	v.y = a.z * b.x - a.x * b.z;
	v.z = a.x * b.y - a.y * b.x;
	return v;
}

float vec3_dot(kore_float3 a, kore_float3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

kore_matrix4x4 matrix4x4_perspective_projection(float fovy, float aspect, float zn, float zf) {
	float          uh = 1.0f / tanf(fovy / 2);
	float          uw = uh / aspect;
	kore_matrix4x4 m  = {uw, 0, 0, 0, 0, uh, 0, 0, 0, 0, (zf + zn) / (zn - zf), -1, 0, 0, 2 * zf * zn / (zn - zf), 0};
	return m;
}

kore_matrix4x4 matrix4x4_look_at(kore_float3 eye, kore_float3 at, kore_float3 up) {
	kore_float3    zaxis = vec3_normalize(vec3_sub(at, eye));
	kore_float3    xaxis = vec3_normalize(vec3_cross(zaxis, up));
	kore_float3    yaxis = vec3_cross(xaxis, zaxis);
	kore_matrix4x4 m     = {xaxis.x,
	                        yaxis.x,
	                        -zaxis.x,
	                        0,
	                        xaxis.y,
	                        yaxis.y,
	                        -zaxis.y,
	                        0,
	                        xaxis.z,
	                        yaxis.z,
	                        -zaxis.z,
	                        0,
	                        -vec3_dot(xaxis, eye),
	                        -vec3_dot(yaxis, eye),
	                        vec3_dot(zaxis, eye),
	                        1};
	return m;
}

kore_matrix4x4 matrix4x4_identity(void) {
	kore_matrix4x4 m;
	memset(m.m, 0, sizeof(m.m));
	for (unsigned x = 0; x < 4; ++x) {
		kore_matrix4x4_set(&m, x, x, 1.0f);
	}
	return m;
}

static void update(void *data) {
	kore_matrix4x4 projection = matrix4x4_perspective_projection(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	kore_float3    v0         = {4, 3, 3};
	kore_float3    v1         = {0, 0, 0};
	kore_float3    v2         = {0, 1, 0};
	kore_matrix4x4 view       = matrix4x4_look_at(v0, v1, v2);
	kore_matrix4x4 model      = matrix4x4_identity();
	kore_matrix4x4 mvp        = matrix4x4_identity();
	mvp                       = kore_matrix4x4_multiply(&mvp, &projection);
	mvp                       = kore_matrix4x4_multiply(&mvp, &view);
	mvp                       = kore_matrix4x4_multiply(&mvp, &model);

	constants_type *constants_data = constants_type_buffer_lock(&constants, 0, 1);
	constants_data->mvp            = mvp;
	constants_type_buffer_unlock(&constants);

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
	                        .b = 0.25f,
	                        .a = 1.0f,
	                    },
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
	kore_gpu_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline_pipeline(&list);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0, 3 * sizeof(uint16_t));

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
	kore_init("02_matrix", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	kong_create_buffer_vertex_in(&device, 3, &vertices);
	vertex_in *v = kong_vertex_in_buffer_lock(&vertices);

	v[0].pos.x = -1.0f;
	v[0].pos.y = -1.0f;
	v[0].pos.z = 0.5f;

	v[1].pos.x = 1.0f;
	v[1].pos.y = -1.0f;
	v[1].pos.z = 0.5f;

	v[2].pos.x = 0.0f;
	v[2].pos.y = 1.0f;
	v[2].pos.z = 0.5f;

	kong_vertex_in_buffer_unlock(&vertices);

	{
		kore_gpu_buffer_parameters params;
		params.size        = 3 * sizeof(uint16_t);
		params.usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE;
		kore_gpu_device_create_buffer(&device, &params, &indices);
		{
			uint16_t *i = (uint16_t *)kore_gpu_buffer_lock_all(&indices);
			i[0]        = 0;
			i[1]        = 1;
			i[2]        = 2;
			kore_gpu_buffer_unlock(&indices);
		}
	}

	constants_type_buffer_create(&device, &constants, 1);

	{
		everything_parameters parameters;
		parameters.constants = &constants;
		kong_create_everything_set(&device, &parameters, &everything);
	}

	kore_start();

	return 0;
}
