#include <kore3/io/filereader.h>
#include <kore3/math/matrix.h>
#include <kore3/system.h>

#include <kong.h>

#include <assert.h>
#include <math.h>
#include <string.h>

#ifdef SCREENSHOT
#include "../../screenshot.h"
#endif

static kore_gpu_device       device;
static kore_gpu_command_list list;
static kore_gpu_texture      texture;
static rayset_set            rayset;

static float    quadVtx[] = {-1, 0, -1, -1, 0, 1, 1, 0, 1, -1, 0, -1, 1, 0, -1, 1, 0, 1};
static float    cubeVtx[] = {-1, -1, -1, 1, -1, -1, -1, 1, -1, 1, 1, -1, -1, -1, 1, 1, -1, 1, -1, 1, 1, 1, 1, 1};
static uint16_t cubeIdx[] = {4, 6, 0, 2, 0, 6, 0, 1, 4, 5, 4, 1, 0, 2, 1, 3, 1, 2, 1, 3, 5, 7, 5, 3, 2, 6, 3, 7, 3, 6, 4, 5, 6, 7, 6, 5};

static kore_gpu_buffer quadVB;
static kore_gpu_buffer cubeVB;
static kore_gpu_buffer cubeIB;

static kore_gpu_raytracing_volume cubeBlas;
static kore_gpu_raytracing_volume quadBlas;

static kore_gpu_raytracing_hierarchy hierarchy;

static const uint32_t width  = 800;
static const uint32_t height = 600;

static kore_matrix4x4 transforms[3];

static float time(void) {
#ifdef SCREENSHOT
	return 0.3f;
#else
	return (float)kore_time();
#endif
}

static void update_transforms(void) {
	float t = time();

	{
		kore_matrix4x4 cube = kore_matrix4x4_rotation_y(t / 3);
		kore_matrix4x4 a    = kore_matrix4x4_rotation_x(t / 2);
		cube                = kore_matrix4x4_multiply(&a, &cube);
		kore_matrix4x4 b    = kore_matrix4x4_rotation_z(t / 5);
		cube                = kore_matrix4x4_multiply(&b, &cube);
		kore_matrix4x4 c    = kore_matrix4x4_translation(-1.5, 2, 2);
		cube                = kore_matrix4x4_multiply(&c, &cube);

		transforms[0] = cube;
	}

	{
		kore_matrix4x4 mirror = kore_matrix4x4_rotation_x(-1.8f);
		kore_matrix4x4 a      = kore_matrix4x4_rotation_y(sinf(t) / 8 + 1);
		mirror                = kore_matrix4x4_multiply(&a, &mirror);
		kore_matrix4x4 b      = kore_matrix4x4_translation(2, 2, 2);
		mirror                = kore_matrix4x4_multiply(&b, &mirror);

		transforms[1] = mirror;
	}

	{
		kore_matrix4x4 floor = kore_matrix4x4_scale(5, 5, 5);
		kore_matrix4x4 a     = kore_matrix4x4_translation(0, 0, 2);
		floor                = kore_matrix4x4_multiply(&a, &floor);

		transforms[2] = floor;
	}
}

static bool     first = true;
static uint64_t frame = 0;

void update(void *data) {
	kore_gpu_texture *framebuffer = kore_gpu_device_get_framebuffer(&device);

	if (first) {
		first = false;

		kore_gpu_command_list_prepare_raytracing_volume(&list, &cubeBlas);
		kore_gpu_command_list_prepare_raytracing_volume(&list, &quadBlas);
		kore_gpu_command_list_prepare_raytracing_hierarchy(&list, &hierarchy);
	}

	update_transforms();
	kore_gpu_command_list_update_raytracing_hierarchy(&list, transforms, 3, &hierarchy);

	kong_set_ray_pipeline_ray_pipe(&list);

	kong_set_descriptor_set_rayset(&list, &rayset);

	kore_gpu_command_list_trace_rays(&list, width, height, 1);

	kore_gpu_image_copy_texture source = {0};
	source.texture                     = &texture;

	kore_gpu_image_copy_texture destination = {0};
	destination.texture                     = framebuffer;

	kore_gpu_command_list_copy_texture_to_texture(&list, &source, &destination, width, height, 1);

#ifdef SCREENSHOT
	if (frame == 3) {
		screenshot_take(&device, &list, framebuffer, width, height);
	}
#endif

	kore_gpu_command_list_present(&list);

	kore_gpu_device_execute_command_list(&device, &list);

	++frame;
}

int kickstart(int argc, char **argv) {
	kore_init("raytracing", width, height, NULL, NULL);
	kore_set_update_callback(update, NULL);

	kore_gpu_device_wishlist wishlist = {0};
	kore_gpu_device_create(&device, &wishlist);

	kong_init(&device);

	kore_gpu_texture_parameters render_target_parameters = {
	    .width                 = width,
	    .height                = height,
	    .depth_or_array_layers = 1,
	    .mip_level_count       = 1,
	    .sample_count          = 1,
	    .dimension             = KORE_GPU_TEXTURE_DIMENSION_2D,
	    .format                = KORE_GPU_TEXTURE_FORMAT_RGBA8_UNORM,
	    .usage                 = render_target_texture_usage_flags(),
	};
	kore_gpu_device_create_texture(&device, &render_target_parameters, &texture);

	kore_gpu_device_create_command_list(&device, KORE_GPU_COMMAND_LIST_TYPE_GRAPHICS, &list);

	{
		kore_gpu_buffer_parameters params = {
		    .size        = sizeof(quadVtx),
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE,
		};
		kore_gpu_device_create_buffer(&device, &params, &quadVB);

		void *data = kore_gpu_buffer_lock_all(&quadVB);
		memcpy(data, quadVtx, sizeof(quadVtx));
		kore_gpu_buffer_unlock(&quadVB);
	}

	kore_gpu_device_create_raytracing_volume(&device, &quadVB, sizeof(quadVtx) / 4 / 3, NULL, 0, &quadBlas);

	{
		kore_gpu_buffer_parameters params = {
		    .size        = sizeof(cubeVtx),
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE,
		};
		kore_gpu_device_create_buffer(&device, &params, &cubeVB);

		void *data = kore_gpu_buffer_lock_all(&cubeVB);
		memcpy(data, cubeVtx, sizeof(cubeVtx));
		kore_gpu_buffer_unlock(&cubeVB);
	}

	{
		kore_gpu_buffer_parameters params = {
		    .size        = sizeof(cubeIdx),
		    .usage_flags = KORE_GPU_BUFFER_USAGE_CPU_WRITE,
		};
		kore_gpu_device_create_buffer(&device, &params, &cubeIB);

		void *data = kore_gpu_buffer_lock_all(&cubeIB);
		memcpy(data, cubeIdx, sizeof(cubeIdx));
		kore_gpu_buffer_unlock(&cubeIB);
	}

	kore_gpu_device_create_raytracing_volume(&device, &cubeVB, sizeof(cubeVtx) / 4 / 3, &cubeIB, sizeof(cubeIdx) / 2, &cubeBlas);

	kore_gpu_raytracing_volume *volumes[] = {&cubeBlas, &quadBlas, &quadBlas};

	update_transforms();

	kore_gpu_device_create_raytracing_hierarchy(&device, volumes, transforms, 3, &hierarchy);

	{
		rayset_parameters parameters = {
		    .scene = &hierarchy,
		    .render_target =
		        {
		            .texture           = &texture,
		            .base_mip_level    = 0,
		            .mip_level_count   = 1,
		            .base_array_layer  = 0,
		            .array_layer_count = 1,
		        },
		};
		kong_create_rayset_set(&device, &parameters, &rayset);
	}

	kore_start();

	kong_destroy_rayset_set(&rayset);
	kore_gpu_raytracing_hierarchy_destroy(&hierarchy);
	kore_gpu_raytracing_volume_destroy(&cubeBlas);
	kore_gpu_buffer_destroy(&cubeIB);
	kore_gpu_buffer_destroy(&cubeVB);
	kore_gpu_raytracing_volume_destroy(&quadBlas);
	kore_gpu_buffer_destroy(&quadVB);
	kore_gpu_command_list_destroy(&list);
	kore_gpu_texture_destroy(&texture);
	kore_gpu_device_destroy(&device);

	return 0;
}
