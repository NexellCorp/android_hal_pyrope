/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _GL2_FRAMEWORK_
#define _GL2_FRAMEWORK_

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <suite.h>

#include <GLES2/mali_gl2.h>
#include <GLES2/mali_gl2ext.h>
#include <EGL/mali_egl.h>
#include <EGL/eglext.h>

#include "test_helpers.h"
#include "gles_helpers.h"
#include "gl2_framework_parameters.h"
#include "backbufferimage.h"
#include <math.h>

#include "egl_helpers.h"


/* Add this to use binshaders instead of source shaders */
/*#define USING_BINARY_SHADERS*/

/* UNDEFINED BEHAVIOR tests for things that aren't in the spec or things in the spec that
 * are ambiguous. At some point, we declared that "this is how we implement it", and
 * tests using the same behavior will be flagged with this define. Examples are
 * all cases where NULL pointers passed to GL. Our GL driver never crashes on this,
 * but all other implementations do. */
#ifndef TESTBENCH_DESKTOP_GL /* desktop GL should always use strict mode */
#define TESTBENCH_UNDEFINED_BEHAVIOR
#endif

/* shorthand macros */
#define WIDTH (gles2_window_width(test_suite))
#define HEIGHT (gles2_window_height(test_suite))
#define FSAA (gles2_window_multisample(test_suite))
#define ALPHA_BITS (gles2_configuration_alpha_bits(test_suite))

/**
 * Translates an error code to a string
 * @return a constant static-memory-allocated string. Do not free :)
 */
const char* error_string(GLenum errcode);

/**
 * Test if an extension is enabled/supported.
 * @param extension the name of the extension as it appears in glGetString(GL_EXTENSIONS)
 * @return TRUE if the extension is enabled/supported (ie. the name appears in the GL_EXTENSIONS string)
 */
int is_extension_enabled(char * extension);

/**
 * Quick-draw-methods. All of these modify various GL states. This includes
 *  - vertex attributes enabled
 *  - vertex attribute pointers set
 *  - uniforms for nvp / samplers modified
 */

/**
 * Draw a custom simple triangle with custom vertex colors
 * Requires the use of a fragment shader that has a uniform "color" that sets the output fragment
 * @param vertices 9 floats ( 3 vertices * 3 components ) that define the geometry.
 * @param color 3 floats ( 3 components ) that define the color of the triangle
 * @note This functions modify GL states.
 */
void draw_custom_triangle(const float *vertices, const float *color);

/**
 * Draw a square (two triangles) across the entire viewport.
 **/
void draw_square(void);

/**
 * Draw a square (two triangles) beginning at pixel x,y with pixel dimensions w,h
 */
void draw_normalized_square_pos(float x, float y, float w, float h);

/**
 * Draw a square (two triangles) across the left half of the screen (range x [-1, 0])
 */
void draw_left_rectangle(void);

/**
 * Draw a square (two triangles) across the right half of the screen (range x [0, 1])
 */
void draw_right_rectangle(void);

/**
 * Draw a square (two triangles) across the top half of the screen (range y [1, 0])
 */
void draw_top_rectangle(void);

/**
 * Draw a square (two triangles) across the top half of the screen (range y [0, -1])
 */
void draw_bottom_rectangle(void);

/**
 * Draw a point at the specified position, with the specified color and size.
 */
void draw_point(const float *vertices, const float *color, const float size);

/**
 * Draw a line at the specified position, with the specified color and size.
 */
void draw_line(const float *vertices, const float *color, const float size);

/**
 * Draw a texturized square with the given vertices and texcoords using the given texture
 */
void draw_custom_textured_square( const GLfloat* vertices, const GLfloat *texcoords, GLuint texture_id);

/**
 * As draw_square(), but apply a texture across the square. The texture is taken from
 * the texture bound to texture unit 0. Shader must contain a sampler2D called "diffuse".
 */
void draw_textured_square(void);

/**
 * As draw_normalised_square_pos(), but apply a texture across the square. The texture is taken from
 * the texture bound to texture unit 0. Shader must contain a sampler2D called "diffuse".
 */
void draw_normalized_textured_square(float x, float y, float w, float h);

/**
 * As draw_normalized_textured_square, but offset the texture coordinates by s_offset,t_offset.
 * The offsets are normalized, so offseting by 1.0 is the same as offsetting by 2.0 or 0.0
 */
void draw_normalized_textured_square_offset(float x, float y, float w, float h, float s_offset, float t_offset);

/**
 * As draw_normalized_square_pos, but render a cube map face (1st face) to the drawn quad.
 * The bound shader must contain a samplerCube called "cube".
 */
void draw_normalized_cube_textured_square(float x, float y, float w, float h);


/* specify quad in clockwise <xyz> coordinates */
void draw_custom_square(GLfloat vertices[3*4]);
void draw_custom_square_textured_offset(const GLfloat* vertices, GLfloat xoffset, GLfloat yoffset, GLuint texture_id);

/* Loads a file, returns a pointer to the loaded data. */
void* load_file(const char* filename, int* len, const char* fopen_credentials);

/* loads a single shader from a file
 * @param shadername The shadername is given as the input to load_program
 * @param shadertype GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * @return 0 or a compiled GLES shader object
 */
GLuint load_shader(const char* shadername, GLenum shadertype);

/**
 * Loads a glsl program based on the names of a vertex and a fragment shader file
 * Only the base filename (no extension or path) of the shaders are accepted.
 * The function appends .binshader or .glsl depending on which functionality is used.
 * set/unset the USING_BINARY_SHADERS flag on the top of this file and do a full recompile
 * to change the functionality.
 * @return the GL id of the loaded shader program. Exits with full error message dump on error.
 */
GLuint load_program(const char* vshader, const char* fshader);

/**
 * Creates and initializes a fixture
 * @param test_suite the suite to create a fixture for
 * @return NULL on failure, a valid suite pointer else
 */
void* gles2_create_fixture(suite* test_suite);
void* gles1_create_fixture(suite* test_suite);

/**
 * Create and initialize a fixture - with special parameters
 * @note This is used by the multisample test suite, which require special dimensions per test
 * @param width The width of the fixture required
 * @param height The height of the fixture required
 * @param rgba an array of rgba bitdepth. ex {5,6,5,0}
 * @param multisample The multisample coefficient, 1, 4 or 16
 * @param test_suite the suite to create a fixture for
 * @return NULL on failure, a valid suite pointer else
 */
void* gles2_create_fixture_with_special_dimensions(suite* test_suite, int width, int height, char rgba[4], int multisample);


/**
 * Deinitializes and removes a fixture
 * @param test_suite the suite to remove a fixture for
 */
void gles2_remove_fixture(suite* test_suite);
void gles1_remove_fixture(suite* test_suite);

void gles_share_context_remove(suite* test_suite, EGLContext context);
EGLContext gles_share_context_create(suite* test_suite, int version, EGLContext share_list);

void context_make_current( suite* test_suite, EGLContext context );

/**
 * Performs a double buffer swap. Also processes all queued messages (this is a good time as any)
 * @param test_suite the suite to swap buffers on
 */
void swap_buffers( suite *test_suite );

/* retrieve width and height and fsaa-settings from a window */
int gles2_window_width(suite* test_suite);
int gles2_window_height(suite* test_suite);
int gles2_window_multisample(suite* test_suite);
int gles2_configuration_alpha_bits(suite* test_suite);
EGLDisplay gles2_window_get_display(suite* test_suite);
EGLSurface gles2_window_get_surface(suite* test_suite);
EGLContext gles2_window_get_config(suite* test_suite);
EGLContext gles2_window_get_context(suite* test_suite);
egl_helper_windowing_state *gles2_framework_get_windowing_state( suite *test_suite );


/* Timing function. Returns the time in microseconds */
unsigned long long get_time(void);


/* ASSERT helpers */

/*
From the OpenGL 2.0 Spec, section 2.1.1:
"We require simply that numbers
floating-point parts contain enough bits and that their exponent fields are large
enough so that individual results of floating-point operations are accurate to about
1 part in 10^5."
*/
static const float gles_float_epsilon = 1.0e-5f;

struct gles_color
{
	float r, g, b, a;
};

/* float constructors */
struct gles_color gles_color_make_3f(float r, float g, float b);
struct gles_color gles_color_make_4f(float r, float g, float b, float a);

/* unsigned byte constructors */
struct gles_color gles_color_make_3b(u8 r, u8 g, u8 b);
struct gles_color gles_color_make_4b(u8 r, u8 g, u8 b, u8 a);

struct gles_color gles_color_clamp(struct gles_color col);
int gles_colors_rgb_equal(struct gles_color c1, struct gles_color c2, float epsilon);
int gles_fb_color_equal(struct gles_color fb_color, struct gles_color expected, float epsilon);
int gles_fb_color_rgb_equal(struct gles_color fb_color, struct gles_color expected, float epsilon);

struct gles_color gles_fb_get_pixel_color(const BackbufferImage *img, int x, int y);
struct gles_color gles_buffer_4b_get_pixel_color(const u8 *buf, int w, int h, int x, int y);
struct gles_color gles_buffer_3b_get_pixel_color(const u8 *buf, int w, int h, int x, int y);
struct gles_color gles_buffer_2b_get_pixel_color(const u8 *buf, int w, int h, int x, int y);
struct gles_color gles_buffer_1b_get_pixel_color(const u8 *buf, int w, int h, int x, int y);
struct gles_color gles_buffer_us565_get_pixel_color(const u16 *buf, int width, int height, int x, int y);
struct gles_color gles_buffer_us5551_get_pixel_color(const u16 *buf, int width, int height, int x, int y);
struct gles_color gles_buffer_us4444_get_pixel_color(const u16 *buf, int width, int height, int x, int y);

#define ASSERT_GLES_FB_COLORS_EQUAL(img, x, y, expected, eps) \
	assert_fail(1 == gles_fb_color_equal(gles_fb_get_pixel_color(img, x, y), expected, eps), \
				dsprintf(test_suite, \
						 "pixel at <%d, %d> is <%.3f, %.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f, %.3f>, epsilon %f (%s:%d)", \
						 x, y, \
						 gles_fb_get_pixel_color(img, x, y).r, \
						 gles_fb_get_pixel_color(img, x, y).g, \
						 gles_fb_get_pixel_color(img, x, y).b, \
						 gles_fb_get_pixel_color(img, x, y).a, \
						 expected.r, \
						 expected.g, \
						 expected.b, \
						 expected.a, eps, \
						 __FILE__, __LINE__));

#define ASSERT_GLES_FB_COLORS_EQUAL_MSG(img, x, y, expected, eps, msg) \
	assert_fail(1 == gles_fb_color_equal(gles_fb_get_pixel_color(img, x, y), expected, eps), \
				dsprintf(test_suite, \
						 "pixel at <%d, %d> is <%.3f, %.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f, %.3f>, epsilon %f, msg: %s (%s:%d)", \
						 x, y, \
						 gles_fb_get_pixel_color(img, x, y).r, \
						 gles_fb_get_pixel_color(img, x, y).g, \
						 gles_fb_get_pixel_color(img, x, y).b, \
						 gles_fb_get_pixel_color(img, x, y).a, \
						 expected.r, \
						 expected.g, \
						 expected.b, \
						 expected.a, \
						 eps, \
						 msg, \
						 __FILE__, __LINE__));

#define ASSERT_GLES_FB_COLORS_RGB_EQUAL(img, x, y, expected, eps) \
	assert_fail(1 == gles_fb_color_rgb_equal(gles_fb_get_pixel_color(img, x, y), expected, eps), \
				dsprintf(test_suite, \
					 	 "pixel at <%d, %d> is <%.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f>, epsilon %f (%s:%d)", \
						 x, y, \
						 gles_fb_get_pixel_color(img, x, y).r, \
						 gles_fb_get_pixel_color(img, x, y).g, \
						 gles_fb_get_pixel_color(img, x, y).b, \
						 expected.r, \
						 expected.g, \
						 expected.b, \
						 eps, \
						 __FILE__, __LINE__))

#define ASSERT_GLES_FB_COLORS_RGB_EQUAL_MSG(img, x, y, expected, eps, msg) \
	assert_fail(1 == gles_fb_color_rgb_equal(gles_fb_get_pixel_color(img, x, y), expected, eps), \
				dsprintf(test_suite, \
					 	 "pixel at <%d, %d> is <%.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f>, epsilon %f, msg: %s (%s:%d)", \
						 x, y, \
						 gles_fb_get_pixel_color(img, x, y).r, \
						 gles_fb_get_pixel_color(img, x, y).g, \
						 gles_fb_get_pixel_color(img, x, y).b, \
						 expected.r, \
						 expected.g, \
						 expected.b, \
						 eps, \
						 msg, \
						 __FILE__, __LINE__))

#define ASSERT_GLES_FB_COLORS_RGB_NOT_BLACK(img, x, y)                                                 \
{                                                                                                      \
	struct gles_color color = gles_fb_get_pixel_color(img, x, y);                                      \
	assert_fail( color.r > 0 || color.g > 0 || color.b > 0,                                            \
	             dsprintf(test_suite, "pixel at <%d, %d> is black (%s:%d)", x, y, __FILE__, __LINE__)) \
}

#define ASSERT_GLES_BUFFER_4B_COLORS_EQUAL(buf, width, height, x, y, expected, eps) \
	assert_fail(1 == gles_fb_color_equal(gles_buffer_4b_get_pixel_color(buf, width, height, x, y), expected, eps), \
		dsprintf(test_suite, "pixel at <%d, %d> is <%.3f, %.3f, %.3f, %.3f>, expected <%.3f, %.3f, %.3f, %.3f>, epsilon %f (%s:%d)", \
			x, y, \
			gles_buffer_4b_get_pixel_color(buf, width, height, x, y).r, \
			gles_buffer_4b_get_pixel_color(buf, width, height, x, y).g, \
			gles_buffer_4b_get_pixel_color(buf, width, height, x, y).b, \
			gles_buffer_4b_get_pixel_color(buf, width, height, x, y).a, \
			expected.r, expected.g, expected.b, expected.a, eps, __FILE__, __LINE__ \
		) \
	)

#endif /* _GL2_FRAMEWORK_ */

