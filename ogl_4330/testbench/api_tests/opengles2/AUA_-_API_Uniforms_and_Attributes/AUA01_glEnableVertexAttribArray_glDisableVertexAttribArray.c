/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * test_glEnableVertexAttribArray_glDisableVertexAttribArray - Tests enabling and disabling of generic attribute arrays.
 *
 * CASE #1 - Enable attribute array N ( glGet( GL_MAX_VERTEX_ATTRIBS ) )
 *           Result: Should give a GL_INVALID_VALUE error
 *
 * CASE #2 - Enable attribtue 0, then retrieve attribute 0's state (GL_VERTEX_ATTRIB_ENABLED).
 *           Result: No errors, attribute 0 should be enabled (GL_VERTEX_ATTRIB_ENABLED is GL_TRUE)
 *
 * CASE #3 - Repeat step 2 from attribute 1 to attriute GL_MAX_VERTEX_ATTRIBS
 *           Result: No errors, attribute 1-GL_MAX_VERTEX_ATTRIBS should be enabled (GL_VERTEX_ATTRIB_ENABLED is GL_TRUE)
*/

#include "../gl2_framework.h"
#include <suite.h>

static void test_glEnableVertexAttribArray_glDisableVertexAttribArray(suite* test_suite)
{
	/*
	* Definitions
	*/

	GLint params;
	GLenum error;
	GLint max_attribs;
	int i;

	/*
	* CASE #01 - Enable attribute array n = glGet( GL_MAX_VERTEX_ATTRIBS )
	*/

	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &params );
	glEnableVertexAttribArray( params );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_INVALID_VALUE );

	/*
	* CASE #02 - Enable attribute 0. Then retrieve attribute 0 state with glGetVertexAttrib( GL_VERTEX_ATTRIB_ARRAY_ENABLED )
	*/

	/* Enable the attribute 0 */
	glEnableVertexAttribArray( 0 );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Check that the GL_VERTEX_ATTRIB_ARRAY_ENABLED flag of 0 is set to GL_TRUE */
	glGetVertexAttribiv( 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
	ASSERT_GLENUMS_EQUAL( params, GL_TRUE );

	/*
	* CASE #03 - Repeat step 2 for each attribute between 1 and the maximum number of vertex attributes
	*/

	/* Get the maximum number of attributes */
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &max_attribs );
	error = glGetError();
	ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

	/* Loop trough the attributes */
	for( i = 1; i < max_attribs; i++ )
	{
		/* Enable the current attribute */
		glEnableVertexAttribArray( i );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );

		/* Check that the GL_VERTEX_ATTRIB_ARRAY_ENABLED flag of the current attribute is set to GL_TRUE */
		glGetVertexAttribiv( i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params );
		error = glGetError();
		ASSERT_GLENUMS_EQUAL( error, GL_NO_ERROR );
		ASSERT_GLENUMS_EQUAL( params, GL_TRUE );
	}

}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_glEnableVertexAttribArray_glDisableVertexAttribArray(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "glEnableVertexAttribArray_glDisableVertexAttribArray", gles2_create_fixture, gles2_remove_fixture, results );

	add_test_with_flags(new_suite, "AUA01-glEnableVertexAttribArray_glDisableVertexAttribArray",
	                    test_glEnableVertexAttribArray_glDisableVertexAttribArray,
	                    FLAG_TEST_ALL | FLAG_TEST_SMOKETEST);

	return new_suite;
}
