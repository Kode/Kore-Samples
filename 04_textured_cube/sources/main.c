#include <kore3/image.h>
#include <kore3/io/filereader.h>
#include <kore3/log.h>
#include <kore3/system.h>
#include <kore3/window.h>

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
static kore_gpu_texture      texture;
static kore_gpu_sampler      sampler;
static everything_set        everything;
static kore_gpu_buffer       image_buffer;
static kore_gpu_texture      depth;

static uint32_t vertex_count;

static const int width  = 800;
static const int height = 600;

/* clang-format off */
static float vertices_data[] = {
    -1.0,-1.0,-1.0,
	-1.0,-1.0, 1.0,
	-1.0, 1.0, 1.0,
	 1.0, 1.0,-1.0,
	-1.0,-1.0,-1.0,
	-1.0, 1.0,-1.0,
	 1.0,-1.0, 1.0,
	-1.0,-1.0,-1.0,
	 1.0,-1.0,-1.0,
	 1.0, 1.0,-1.0,
	 1.0,-1.0,-1.0,
	-1.0,-1.0,-1.0,
	-1.0,-1.0,-1.0,
	-1.0, 1.0, 1.0,
	-1.0, 1.0,-1.0,
	 1.0,-1.0, 1.0,
	-1.0,-1.0, 1.0,
	-1.0,-1.0,-1.0,
	-1.0, 1.0, 1.0,
	-1.0,-1.0, 1.0,
	 1.0,-1.0, 1.0,
	 1.0, 1.0, 1.0,
	 1.0,-1.0,-1.0,
	 1.0, 1.0,-1.0,
	 1.0,-1.0,-1.0,
	 1.0, 1.0, 1.0,
	 1.0,-1.0, 1.0,
	 1.0, 1.0, 1.0,
	 1.0, 1.0,-1.0,
	-1.0, 1.0,-1.0,
	 1.0, 1.0, 1.0,
	-1.0, 1.0,-1.0,
	-1.0, 1.0, 1.0,
	 1.0, 1.0, 1.0,
	-1.0, 1.0, 1.0,
	 1.0,-1.0, 1.0
};

static float tex_data[] = {
    0.000059f, 0.000004f,
	0.000103f, 0.336048f,
	0.335973f, 0.335903f,
	1.000023f, 0.000013f,
	0.667979f, 0.335851f,
	0.999958f, 0.336064f,
	0.667979f, 0.335851f,
	0.336024f, 0.671877f,
	0.667969f, 0.671889f,
	1.000023f, 0.000013f,
	0.668104f, 0.000013f,
	0.667979f, 0.335851f,
	0.000059f, 0.000004f,
	0.335973f, 0.335903f,
	0.336098f, 0.000071f,
	0.667979f, 0.335851f,
	0.335973f, 0.335903f,
	0.336024f, 0.671877f,
	1.000004f, 0.671847f,
	0.999958f, 0.336064f,
	0.667979f, 0.335851f,
	0.668104f, 0.000013f,
	0.335973f, 0.335903f,
	0.667979f, 0.335851f,
	0.335973f, 0.335903f,
	0.668104f, 0.000013f,
	0.336098f, 0.000071f,
	0.000103f, 0.336048f,
	0.000004f, 0.671870f,
	0.336024f, 0.671877f,
	0.000103f, 0.336048f,
	0.336024f, 0.671877f,
	0.335973f, 0.335903f,
	0.667969f, 0.671889f,
	1.000004f, 0.671847f,
	0.667979f, 0.335851f
};
/* clang-format on */

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
	kore_matrix4x4 m     = {
        xaxis.x,
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
        1,
    };
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

static bool first_update = true;

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

	if (first_update) {
		kore_gpu_image_copy_buffer source = {
		    .buffer         = &image_buffer,
		    .bytes_per_row  = 512 * 4,
		    .rows_per_image = 512,
		};

		kore_gpu_image_copy_texture destination = {
		    .texture = &texture,
		};

		kore_gpu_command_list_copy_buffer_to_texture(&list, &source, &destination, 512, 512, 1);

		first_update = false;
	}

	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	if (framebuffer->width != depth.width || framebuffer->height != depth.height) {
		kore_gpu_device_wait_until_idle(&device);
		kore_gpu_texture_destroy(&depth);

		const kore_gpu_texture_parameters texture_parameters = {
		    .format                = KORE_GPU_TEXTURE_FORMAT_DEPTH32_FLOAT,
		    .width                 = framebuffer->width,
		    .height                = framebuffer->height,
		    .depth_or_array_layers = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .mip_level_count       = 1,
		    .sample_count          = 1,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_RENDER_ATTACHMENT,
		};

		kore_gpu_device_create_texture(&device, &texture_parameters, &depth);
	}

	kore_gpu_render_pass_parameters parameters = {
	    .color_attachments_count = 1,
	    .color_attachments =
	        {
	            {
	                .load_op                   = KORE_GPU_LOAD_OP_CLEAR,
	                .clear_value               = {0.0f, 0.0f, 0.25f, 1.0f},
	                .texture.texture           = framebuffer,
	                .texture.array_layer_count = 1,
	                .texture.mip_level_count   = 1,
	                .texture.format            = kore_gpu_device_framebuffer_format(&device),
	                .texture.dimension         = KORE_GPU_TEXTURE_VIEW_DIMENSION_2D,
	            },
	        },
	    .depth_stencil_attachment =
	        {
	            .texture           = &depth,
	            .depth_load_op     = KORE_GPU_LOAD_OP_CLEAR,
	            .depth_clear_value = 1.0f,
	        },
	};
	kore_gpu_command_list_begin_render_pass(&list, &parameters);

	kong_set_render_pipeline_pipeline(&list);

	kong_set_vertex_buffer_vertex_in(&list, &vertices);

	kore_gpu_command_list_set_index_buffer(&list, &indices, KORE_GPU_INDEX_FORMAT_UINT16, 0);

	kong_set_descriptor_set_everything(&list, &everything);

	kore_gpu_command_list_draw_indexed(&list, vertex_count, 1, 0, 0, 0);

	kore_gpu_command_list_end_render_pass(&list);

#ifdef SCREENSHOT
	screenshot_take(&device, &list, framebuffer, width, height);
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);
}

int kickstart(int argc, char **argv) {
	kore_init("04_textured_cube", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_buffer_parameters buffer_parameters;
	buffer_parameters.size        = kore_gpu_device_align_texture_row_bytes(&device, 512 * 4) * 512;
	buffer_parameters.usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE | KORE_GPU_BUFFER_USAGE_COPY_SRC;
	kore_gpu_device_create_buffer(&device, &buffer_parameters, &image_buffer);

	kore_image image;
	kore_image_init_from_file_with_stride(&image, kore_gpu_buffer_lock_all(&image_buffer), "uvtemplate.png",
	                                      kore_gpu_device_align_texture_row_bytes(&device, 512 * 4));
	kore_image_destroy(&image);
	kore_gpu_buffer_unlock(&image_buffer);

	{
		kore_gpu_texture_parameters texture_params = {
		    .format                = KORE_GPU_TEXTURE_FORMAT_DEPTH32_FLOAT,
		    .width                 = width,
		    .height                = height,
		    .depth_or_array_layers = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .mip_level_count       = 1,
		    .sample_count          = 1,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_RENDER_ATTACHMENT,
		};
		kore_gpu_device_create_texture(&device, &texture_params, &depth);
	}

	{
		kore_gpu_texture_parameters texture_parameters = {
		    .width                 = 512,
		    .height                = 512,
		    .depth_or_array_layers = 1,
		    .mip_level_count       = 1,
		    .sample_count          = 1,
		    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
		    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
		    .usage                 = KORE_GPU_TEXTURE_USAGE_COPY_DST | pix_texture_texture_usage_flags(),
		};
		kore_gpu_device_create_texture(&device, &texture_parameters, &texture);
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
	    .compare        = KORE_GPU_COMPARE_FUNCTION_UNDEFINED,
	    .max_anisotropy = 1,
	};
	kore_gpu_device_create_sampler(&device, &sampler_parameters, &sampler);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	vertex_count = sizeof(vertices_data) / 3 / 4;
	kong_create_buffer_vertex_in(&device, vertex_count, &vertices);
	{
		vertex_in *v = kong_vertex_in_buffer_lock(&vertices);
		for (uint32_t i = 0; i < vertex_count; ++i) {
			v[i].pos.x = vertices_data[i * 3];
			v[i].pos.y = vertices_data[i * 3 + 1];
			v[i].pos.z = vertices_data[i * 3 + 2];
			v[i].tex.x = tex_data[i * 2];
			v[i].tex.y = tex_data[i * 2 + 1];
		}
		kong_vertex_in_buffer_unlock(&vertices);
	}

	kore_gpu_buffer_parameters params = {
	    .size        = vertex_count * sizeof(uint16_t),
	    .usage_flags = KORE_GPU_BUFFER_USAGE_INDEX | KORE_GPU_BUFFER_USAGE_CPU_WRITE,
	};
	kore_gpu_device_create_buffer(&device, &params, &indices);
	{
		uint16_t *id = (uint16_t *)kore_gpu_buffer_lock_all(&indices);
		for (uint32_t i = 0; i < vertex_count; ++i) {
			id[i] = i;
		}
		kore_gpu_buffer_unlock(&indices);
	}

	constants_type_buffer_create(&device, &constants, 1);

	{
		everything_parameters parameters = {
		    .constants = &constants,
		    .pix_texture =
		        {
		            .texture           = &texture,
		            .base_mip_level    = 0,
		            .mip_level_count   = 1,
		            .base_array_layer  = 0,
		            .array_layer_count = 1,
		        },
		    .pix_sampler = &sampler,
		};
		kong_create_everything_set(&device, &parameters, &everything);
	}

	kore_start();

	return 0;
}
