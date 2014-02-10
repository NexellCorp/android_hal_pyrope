/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_TYPE_H
#define COMMON_TYPE_H

#include "common/essl_str.h"
#include "common/essl_mem.h"

union _tag_node;
struct _tag_symbol;
typedef struct _tag_single_declarator single_declarator;
typedef struct _tag_type_specifier type_specifier;
typedef struct _tag_qualifier_set qualifier_set;
struct _tag_target_descriptor;

/**  Caches common type specifiers.
*/
typedef struct {
	struct _tag_mempool *pool; /**< Pool to allocate types from */
	const type_specifier *sint_types_8[4]; /**< int, ivec2, ivec3, ivec4 types - int8 size */
	const type_specifier *uint_types_8[4]; /**< unsigned int, ivec2, ivec3, ivec4 types - int8 size */
	const type_specifier *bool_types_8[4]; /**< bool, bvec2, bvec3, bvec4 types - 8 size  */

	const type_specifier *sint_types_16[4]; /**< int, ivec2, ivec3, ivec4 types - int16 size */
	const type_specifier *uint_types_16[4]; /**< unsigned int, ivec2, ivec3, ivec4 types - int16 size */
	const type_specifier *bool_types_16[4]; /**< bool, bvec2, bvec3, bvec4 types - 16 size  */
	const type_specifier *float_types_16[4]; /**< float, vec2, vec3, vec4 types - fp16 size */

	const type_specifier *sint_types_32[4]; /**< int, ivec2, ivec3, ivec4 types - int32 size */
	const type_specifier *uint_types_32[4]; /**< unsigned int, ivec2, ivec3, ivec4 types - int32 size */
	const type_specifier *bool_types_32[4]; /**< bool, bvec2, bvec3, bvec4 types - 32 size  */
	const type_specifier *float_types_32[4]; /**< float, vec2, vec3, vec4 types - fp16 size */


	const type_specifier *sint_types_64[4]; /**< int, ivec2, ivec3, ivec4 types - int64 size */
	const type_specifier *uint_types_64[4]; /**< unsigned int, ivec2, ivec3, ivec4 types - int64 size */
	const type_specifier *float_types_64[4]; /**< float, vec2, vec3, vec4 types - fp16 size */
} typestorage_context;


typedef enum
{
	TYPE_UNKNOWN, /**< unknown type */
	TYPE_VOID, /**< void type. Used only for the return type of functions */
	TYPE_FLOAT, /**< float, vec2, vec3, vec4 types */
	TYPE_INT, /**< int, ivec2, ivec3, ivec4 types */
	TYPE_BOOL, /**< bool, bvec2, bvec3, bvec4 types */
	TYPE_MATRIX_OF, /**< matrix of child type */
	TYPE_SAMPLER_2D, /**< sampler2D type */
	TYPE_SAMPLER_3D, /**< sampler3D type */
	TYPE_SAMPLER_CUBE, /**< samplerCube type */
	TYPE_SAMPLER_2D_SHADOW, /**< sampler2DShadow type */
	TYPE_SAMPLER_EXTERNAL, /**< samplerExternalOES type */
	TYPE_STRUCT,  /**< struct types */
	TYPE_ARRAY_OF, /**< Array of child type */
	TYPE_UNRESOLVED_ARRAY_OF /**< Array of child type with unresolved size */
} type_basic;


/**  Direction for formal parameters
*/
typedef enum
{
	DIR_NONE,
	DIR_IN,
	DIR_OUT,
	DIR_INOUT
} parameter_qualifier;


/** Type qualifier bitfield
*/
typedef enum
{
	TYPE_QUAL_NONE = 0, /**< No type qualifiers */
	TYPE_QUAL_NOMODIFY = 1,  /**< const qualifier  */
	TYPE_QUAL_VOLATILE = 2,  /**< C99 volatile qualifier */
	TYPE_QUAL_RESTRICTED = 4 /**< C99 restricted qualifier */
} type_qualifier;

/** Variable qualifiers
*/
typedef enum
{
	VAR_QUAL_NONE, /**< No variable qualifier */
	VAR_QUAL_CONSTANT, /**< compile-time constant */
	VAR_QUAL_ATTRIBUTE, /**< vertex attribute */
	VAR_QUAL_VARYING, /**< vertex varying or fragment varying */
	VAR_QUAL_UNIFORM, /**< Uniform variable */
	VAR_QUAL_PERSISTENT /**< Persistent global variable, used by the ARM_persistent_globals extension */
} variable_qualifier;

/** Varying qualifiers
*/
typedef enum
{
	VARYING_QUAL_NONE, /**< No varying qualifier */
	VARYING_QUAL_FLAT, /**< flat shaded varying */
	VARYING_QUAL_CENTROID /**< centroid mapped varying */
} varying_qualifier;


/**  Precision qualifiers
*/
typedef enum
{
	PREC_UNKNOWN, /**< Unknown or default precision */
	PREC_LOW, /**< Low precision */
	PREC_MEDIUM, /**< Medium precision */
	PREC_HIGH  /**< High precision. Only used if the target supports high precision */
} precision_qualifier;

/**
	Type size specifier
*/
typedef enum
{
	SIZE_UNKNOWN = 0,
	SIZE_BITS8 = 1,
	SIZE_BITS16 = 2,
	SIZE_BITS32 = 3,
	SIZE_BITS64 = 4,

	SIZE_INT8 = SIZE_BITS8,
	SIZE_INT16 = SIZE_BITS16,
	SIZE_INT32 = SIZE_BITS32,
	SIZE_INT64 = SIZE_BITS64,

	SIZE_FP16 = SIZE_BITS16,
	SIZE_FP32 = SIZE_BITS32,
	SIZE_FP64 = SIZE_BITS64

} scalar_size_specifier;

typedef enum
{
	INT_SIGNED,
	INT_UNSIGNED
} integer_signedness_specifier;

struct _tag_qualifier_set {
	variable_qualifier variable:4;     /**< variable qualifier */
	varying_qualifier varying:3;      /**< varying qualifier */
	precision_qualifier precision:3;   /**< precision specifier */
	parameter_qualifier direction;     /**< For function parameters, the direction qualifier */
	string group;                      /**< Uniform group the variable belongs to, for the ARM_grouped_uniforms extension */
};

/** Single declarator. Used for formal parameters for functions and member variables in structs
*/
struct _tag_single_declarator {
	single_declarator *next;           /**< Next single declarator in the chain */
	const struct _tag_type_specifier *type; /**< The type of the declarator */
	const struct _tag_type_specifier *parent_type; /**< The type of the parent type in the case of structs */
	struct _tag_qualifier_set qualifier; /**< The qualifiers of the declarator */
	string name;                       /**< Name of the declarator */
	int index;                         /**< Index of member when used in a struct */
	int source_offset;                 /**< byte offset into concatenated source strings. */

	/*@null@*/ struct _tag_symbol *sym; /**< For a parameter, holds the symbol of
										   the parameter inside the function */
};


struct _tag_type_specifier {
	type_basic basic_type;           /**< basic type. */
	type_qualifier type_qual;        /**< type qualifiers */
 	const struct _tag_type_specifier *child_type; /* for derived type, pointer to type it modifies. Otherwise NULL */
	union {
		/*@null@*/ union _tag_node *unresolved_array_size; /**< Expression containing the node used to
										  determine array size before type checking  */
		int array_size;                    /**< array size as an integer.
											  Filled after constant folding/type checking */
		struct {
			scalar_size_specifier scalar_size:4;        /**< size specifier for scalar element */
			integer_signedness_specifier int_signedness:2; /**< integer signedness */
			unsigned vec_size;               /**< 1 if scalar or sampler, vector length(2-4) if vector.
												  For matrices, 2-4 is used for mat2-mat4 */
		} basic;
		unsigned matrix_n_columns;
	} u;

	string name;                       /**< If the type is a struct,
										  this holds the name of the struct */
	/*@null@*/ struct _tag_single_declarator *members; /**< Member variables of the struct */
};

/**  Scope kind.
*/
typedef enum {
	SCOPE_UNKNOWN, /**< Unknown kind */
	SCOPE_LOCAL, /**< Local variable */
	SCOPE_FORMAL, /**< Formal parameter of the callee */
	SCOPE_ARGUMENT, /**< Actual argument to a function call */
	SCOPE_GLOBAL /**< Global variable */
} scope_kind;


/**  Variable kind.
*/
typedef enum {
	ADDRESS_SPACE_UNKNOWN, /**< Unknown kind */
	ADDRESS_SPACE_THREAD_LOCAL, /**< Local variable, formal/actual parameter */
	ADDRESS_SPACE_GLOBAL, /**< Global variable */
	ADDRESS_SPACE_UNIFORM, /**< Global uniform variable. Read-only */
	ADDRESS_SPACE_ATTRIBUTE, /**< Vertex attribute. Read-only */
	ADDRESS_SPACE_VERTEX_VARYING, /**< Vertex varying. Output variable, but can be read back */
	ADDRESS_SPACE_FRAGMENT_VARYING, /**< Fragment varying. Read-only */
	ADDRESS_SPACE_FRAGMENT_SPECIAL, /**< Special fragment variable, like gl_FragCoord, gl_PointCoord and gl_FrontFacing. Read-only */
	ADDRESS_SPACE_FRAGMENT_OUT,  /**< Fragment shader output. Output variable, but can be read back */
	ADDRESS_SPACE_REGISTER  
} address_space_kind;


/**  Initialize a new type storage
*/
memerr _essl_typestorage_init(/*@out@*/ typestorage_context *ctx, struct _tag_mempool *pool);


/**  Get a type with the specific parameters. Returns an immutable type.
*/
/*@null@*/ const type_specifier *_essl_get_type(const typestorage_context *ctx, type_basic type,
												unsigned int vec_size);

/**  Get a type with the specific parameters including scalar size. Returns an immutable type.
*/
const type_specifier *_essl_get_type_with_size(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, scalar_size_specifier scalar_size);

/**  Get a type with the specific parameters including default scalar size for high precision of this type
*/
const type_specifier *_essl_get_type_with_default_size_for_target(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, struct _tag_target_descriptor *desc);


/**
   Clones a type. The result is mutable
*/
/*@null@*/ type_specifier *_essl_clone_type(struct _tag_mempool *pool, const type_specifier *t);


/**  Creates a new type. Returns an empty mutable type.
*/
/*@null@*/ type_specifier *_essl_new_type(struct _tag_mempool *pool);


/*@null@*/ const type_specifier *_essl_new_basic_type(mempool *pool, type_basic type, unsigned int vec_size, scalar_size_specifier sz, integer_signedness_specifier int_signedness);

/**  Fetches a type that is like the input type, but without storage or varying qualifiers.
         Returns an immutable type.
*/
/*@null@*/ const type_specifier *_essl_get_unqualified_type(struct _tag_mempool *pool, const type_specifier *t);


/*@null@*/ const type_specifier *_essl_get_single_matrix_column_type(const type_specifier *mat_type);

type_basic _essl_get_nonderived_basic_type(const type_specifier *t);
/**  Creates a new array type.
*/
/*@null@*/ type_specifier *_essl_new_array_of_type(struct _tag_mempool *pool, const type_specifier *child_type, unsigned array_size);

/*@null@*/ type_specifier *_essl_new_unresolved_array_of_type(struct _tag_mempool *pool, const type_specifier *child_type, union _tag_node *unresolved_array_size);

/*@null@*/ type_specifier *_essl_new_matrix_of_type(struct _tag_mempool *pool, const type_specifier *child_type, unsigned n_columns);


/**  Calculate the size of the type in scalars
*/
unsigned int _essl_get_type_size(const type_specifier *t);


essl_bool _essl_type_has_vec_size(const type_specifier *t);

unsigned int _essl_get_type_vec_size(const type_specifier *t);

unsigned int _essl_get_matrix_n_columns(const type_specifier *t);

unsigned int _essl_get_matrix_n_rows(const type_specifier *t);

scalar_size_specifier _essl_get_scalar_size_for_type(const type_specifier *t);
unsigned _essl_get_specified_samplers_num(const type_specifier *t, type_basic sampler_type);

#if defined(NDEBUG) || defined(S_SPLINT_S)
#define GET_VEC_SIZE(t) ((t)->u.basic.vec_size)
#define GET_MATRIX_N_COLUMNS(t) ((t)->u.matrix_n_columns)
#define GET_MATRIX_N_ROWS(t) ((t)->child_type->u.basic.vec_size)

#else

#define GET_VEC_SIZE(t) (_essl_get_type_vec_size(t))
#define GET_MATRIX_N_COLUMNS(t) (_essl_get_matrix_n_columns(t))
#define GET_MATRIX_N_ROWS(t) (_essl_get_matrix_n_rows(t))
#endif

#define GET_NODE_VEC_SIZE(n) (GET_VEC_SIZE(n->hdr.type))

/**  Compare two types.
   @returns 1 if equal, 0 if not equal
*/
int _essl_type_equal(const type_specifier *a, const type_specifier *b);


/* type equal, also takes scalar size into account */
essl_bool _essl_type_with_scalar_size_equal(const type_specifier *a, const type_specifier *b);

/* Like _essl_type_with_scalar_size_equal, but ignores vec_size */
essl_bool _essl_type_scalar_part_equal(const type_specifier *a, const type_specifier *b);

/**  Calculate the offset of a struct member. Answer in scalars.
*/
int _essl_get_type_member_offset(const type_specifier *t, const single_declarator *sd);

/**  Creates a new type qualifier. Returns an empty mutable type qualifier.
*/

void _essl_init_qualifier_set(qualifier_set *qual);

/**  Create a new single declarator
*/
/*@null@*/ single_declarator *_essl_new_single_declarator(struct _tag_mempool *pool,
														  const type_specifier *type, qualifier_set qualifier, /*@null@*/ string *name, const type_specifier *parent_type, int source_offset);

const type_specifier* _essl_get_type_with_given_size(const typestorage_context *ctx, const type_specifier *a, scalar_size_specifier sz);
const type_specifier *_essl_get_type_with_size_and_signedness(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, scalar_size_specifier scalar_size, integer_signedness_specifier int_signedness);

const type_specifier* _essl_get_type_with_given_vec_size(const typestorage_context *ctx, const type_specifier *a, unsigned vec_size);

essl_bool _essl_is_sampler_type(const type_specifier *t);
essl_bool _essl_is_type_control_dependent(const type_specifier *t, essl_bool is_indexed_statically);
int _essl_size_of_scalar(scalar_size_specifier sz);

essl_bool _essl_type_is_or_has_sampler(const type_specifier* type);


#endif /* COMMON_TYPE_H */
