/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

 #ifdef __cplusplus
extern "C" {
#endif

#include "gles_context.h"
#include "mali_counters.h"

#include "mali_gles_instrumented_types.h"
#include "mali_instrumented_context_types.h"
#ifdef __cplusplus
}
#endif


MALI_EXPORT
mali_err_code _gles_instrumentation_init(mali_instrumented_context* ctx)
{
	_instrumented_register_counter_gles_begin(ctx);

	_instrumented_start_group(ctx, "OpenGL ES", "OpenGL ES stats, including number of calls, timing information, etc.", 0);
		/* glDrawElement Counters */
		_instrumented_start_group(ctx, "glDrawElements", "Statistics for glDrawElements draw calls", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_ELEMENTS_CALLS,
				"Calls to glDrawElements", "Number of calls to glDrawElements",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_ELEMENTS_NUM_INDICES,
				"Indices to glDrawElements", "Number of Indices to glDrawElements",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_ELEMENTS_NUM_TRANSFORMED,
				"Transformed by glDrawElements", "Number of vertices transformed by glDrawElements",
				"num", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);

		/* glDrawArrays Counters */
		_instrumented_start_group(ctx, "glDrawArrays", "Statistics for glDrawArrays draw calls", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_ARRAYS_CALLS,
				"Calls to glDrawArrays", "Number of calls to glDrawArrays",  "num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_ARRAYS_NUM_TRANSFORMED,
				"Transformed by glDrawArrays", "Number of vertices transformed by glDrawArrays",
				"num", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);

		/* common draw counters */
		_instrumented_start_group(ctx, "Draw call stats", "Statistics from draw calls", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_POINTS,
				"Points", "Number of calls to glDraw* with parameter GL_POINTS",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_LINES,
				"Lines", "Number of calls to glDraw* with parameter GL_LINES",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_LINE_LOOP,
				"Lineloop", "Number of calls to glDraw* with parameter GL_LINE_LOOP",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_LINE_STRIP,
				"Linestrip", "Number of calls to glDraw* with parameter GL_LINE_STRIP",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_TRIANGLES,
				"Triangles", "Number of calls to glDraw* with parameter GL_TRIANGLES",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_TRIANGLE_STRIP,
				"Trianglestrip", "Number of calls to glDraw* with parameter GL_TRIANGLE_STRIP",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_DRAW_TRIANGLE_FAN,
				"Trianglefan", "Number of calls to glDraw* with parameter GL_TRIANGLE_FAN",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_NON_VBO_DATA_COPY_TIME,
				"Vertex upload time",
				"Time spent uploading vertex attributes and faceindex data not present in a VBO",
				"us", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_UNIFORM_BYTES_COPIED_TO_MALI,
				"Uniform bytes copied",
				"Number of bytes copied to mali memory as a result of uniforms updating",
				"B", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);

		/* buffer profiling, incl. textures, vbos and fbos */
		_instrumented_start_group(ctx, "Buffer Profiling", "", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_UPLOAD_TEXTURE_TIME,
				"Texture Upload Time", "Time spent uploading textures",
				"us", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_UPLOAD_VBO_TIME,
				"VBO Upload Time", "Time spent uploading vertex buffer objects",
				"us", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_NUM_FLUSHES,
				"FBO flushes", "Number of flushes on framebuffer attachements",
				"num", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);

		/* geometry information */
		_instrumented_start_group(ctx, "Geometry statistics", "", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_TRIANGLES_COUNT,
				"Triangles",
				"The total number of triangles passed to the GLES API per frame. "
				"This includes those added using any of the triangle drawing modes. (GL_TRIANGLES, "
				"GL_TRIANGLE_STRIP or GL_TRIANGLE_STRIP)",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_INDEPENDENT_TRIANGLES_COUNT,
				"Independent triangles",
				"Number of triangles passed to the GLES API using the mode GL_TRIANGLES. "
				"The number of triangles in one call using GL_TRIANGLES is the same as the number "
				"of indices/array elements divided by three.",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_STRIP_TRIANGLES_COUNT,
				"Strip triangles",
				"Number of triangles passed to the GLES API using the mode GL_TRIANGLE_STRIP. "
				"The number of triangles in one triangle strip is the same as the number of "
				"indices/array elements minus two (two elements to get the strip started and then "
				"one element per triangle).",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_FAN_TRIANGLES_COUNT,
				"Fan triangles",
				"Number of triangles passed to the GLES API using the mode GL_TRIANGLE_FAN. "
				"The number of triangles in one triangle fan is the same as the number of indices/array "
				"elements minus two (two elements to get the fan started and then one element per triangle).",
				"num", MALI_SCALE_NONE ));

			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_LINES_COUNT,
				"Lines",
				"The total number of lines passed to the GLES API per frame. "
				"This includes those added using any of the line drawing modes. (GL_LINES, "
				"GL_LINE_STRIP or GL_LINE_LOOP)",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_INDEPENDENT_LINES_COUNT,
				"Independent lines",
				"Number of lines passed to the GLES API using the mode GL_LINES. "
				"The number of lines in one call using GL_LINES is the same as the number "
				"of indices/array elements divided by two",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_STRIP_LINES_COUNT,
				"Strip lines",
				"Number of lines passed to the GLES API using the mode GL_LINE_STRIP. "
				"The number of lines in one line strip is the same as the number of "
				"indices/array elements minus one (one element to get the strip started and "
				"then one element per line).",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_LOOP_LINES_COUNT,
				"Loop lines",
				"Number of lines passed to the GLES API using the mode GL_LINE_LOOP. "
				"The number of lines in one line loop is the same as the number of "
				"indices/array elements.",
				"num", MALI_SCALE_NONE ));

			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_POINTS_COUNT,
				"Points",
				"The total number of points passed to the GLES API per frame. ",
				"num", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);

		/* shader generation (only for OpenGL ES 1.1) */
#if MALI_USE_GLES_1
		_instrumented_start_group(ctx, "Fixed-function emulation", "", 0);
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_NUM_VSHADERS_GENERATED,
				"# vertex shaders generated", "Number of vertex shaders generated",
				"num", MALI_SCALE_NONE ));
			MALI_CHECK_NO_ERROR(_instrumented_register_counter(ctx, INST_GL_NUM_FSHADERS_GENERATED,
				"# fragment shaders generated", "Number of fragment shaders generated",
				"num", MALI_SCALE_NONE ));
		_instrumented_end_group(ctx);
#endif

	_instrumented_end_group(ctx);

	_instrumented_register_counter_gles_end(ctx);

	MALI_SUCCESS;
}

