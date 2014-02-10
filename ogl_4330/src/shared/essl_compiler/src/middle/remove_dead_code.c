/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "middle/remove_dead_code.h"
#include "common/essl_node.h"
#include "common/basic_block.h"

#define MASK(n) ((n)->hdr.live_mask)
#define SET_MASK(n,m) ((n)->hdr.live_mask = (m))
#define FULL_MASK(n) (unsigned)(_essl_type_has_vec_size((n)->hdr.type) ? ((1 << GET_NODE_VEC_SIZE(n)) - 1) : 1)
#define BIT_TEST(mask,bit) (((mask) & (1 << (bit))) != 0)
#define UNDEFINED_MASK 31


static unsigned mask_from_combiner(combine_pattern *comb, int active_component) {
	int i;
	unsigned mask = 0;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		if (comb->mask[i] == active_component) {
			mask |= 1 << i;
		}
	}
	return mask;
}

static memerr transfer_liveness(node *n);

static memerr merge_liveness(node *n, unsigned mask)
{
	if (n != 0 && (mask & ~MASK(n)) != 0)
	{
		SET_MASK(n, mask | MASK(n));
		return transfer_liveness(n);
	}
	return MEM_OK;
}

extern memerr transfer_liveness_for_expression(node *n, unsigned int *transfer_masks)
{
	unsigned int i;
	unsigned int input_mask;
	switch (n->expr.operation)
	{
			/* Operations that propagate liveness componentwise */
		case EXPR_OP_IDENTITY:
		case EXPR_OP_SPILL:
		case EXPR_OP_SPLIT:
		case EXPR_OP_SCHEDULER_SPLIT:
		case EXPR_OP_MEMBER:
		case EXPR_OP_NEGATE:
		case EXPR_OP_PLUS:
		case EXPR_OP_ADD:
		case EXPR_OP_SUB:
		case EXPR_OP_MUL:
		case EXPR_OP_DIV:
		case EXPR_OP_LOGICAL_AND:
		case EXPR_OP_LOGICAL_OR:
		case EXPR_OP_LOGICAL_XOR:
		case EXPR_OP_CONDITIONAL_SELECT:
		case EXPR_OP_FUN_RADIANS:
		case EXPR_OP_FUN_DEGREES:
		case EXPR_OP_FUN_SIN:
		case EXPR_OP_FUN_COS:
		case EXPR_OP_FUN_SIN_0_1:
		case EXPR_OP_FUN_COS_0_1:
		case EXPR_OP_FUN_SIN_PI:
		case EXPR_OP_FUN_COS_PI:
		case EXPR_OP_FUN_TAN:
		case EXPR_OP_FUN_ASIN:
		case EXPR_OP_FUN_ACOS:
		case EXPR_OP_FUN_ATAN:
		case EXPR_OP_FUN_POW:
		case EXPR_OP_FUN_EXP:
		case EXPR_OP_FUN_RCP:
		case EXPR_OP_FUN_RCC:
		case EXPR_OP_FUN_LOG:
		case EXPR_OP_FUN_EXP2:
		case EXPR_OP_FUN_LOG2:
		case EXPR_OP_FUN_SQRT:
		case EXPR_OP_FUN_INVERSESQRT:
		case EXPR_OP_FUN_ABS:
		case EXPR_OP_FUN_SIGN:
		case EXPR_OP_FUN_FLOOR:
		case EXPR_OP_FUN_CEIL:
		case EXPR_OP_FUN_FRACT:
		case EXPR_OP_FUN_MOD:
		case EXPR_OP_FUN_MIN:
		case EXPR_OP_FUN_MAX:
		case EXPR_OP_FUN_CLAMP:
		case EXPR_OP_FUN_MIX:
		case EXPR_OP_FUN_STEP:
		case EXPR_OP_FUN_SMOOTHSTEP:
		case EXPR_OP_FUN_LESSTHAN:
		case EXPR_OP_FUN_LESSTHANEQUAL:
		case EXPR_OP_FUN_GREATERTHAN:
		case EXPR_OP_FUN_GREATERTHANEQUAL:
		case EXPR_OP_FUN_EQUAL:
		case EXPR_OP_FUN_NOTEQUAL:
		case EXPR_OP_FUN_NOT:
		case EXPR_OP_FUN_DFDX:
		case EXPR_OP_FUN_DFDY:
		case EXPR_OP_FUN_M200_DERX:
		case EXPR_OP_FUN_M200_DERY:
		case EXPR_OP_FUN_FWIDTH:
		case EXPR_OP_FUN_TRUNC:
		case EXPR_OP_FUN_CLAMP_0_1:
		case EXPR_OP_FUN_CLAMP_M1_1:
		case EXPR_OP_FUN_MAX_0:
		case EXPR_OP_FUN_FMULN:
		case EXPR_OP_FUN_MALIGP2_FIXED_TO_FLOAT:
		case EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED:
		case EXPR_OP_FUN_MALIGP2_FIXED_EXP2:
		case EXPR_OP_FUN_MALIGP2_LOG2_FIXED:
		case EXPR_OP_FUN_MALIGP2_MUL_EXPWRAP:
			{
				unsigned int mask = (unsigned int) MASK(n);

				/* Simply merge the liveness mask into the inputs */
				for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
				{
					/* Scalars get liveness from the whole of the input */
					if (_essl_type_has_vec_size(GET_CHILD(n,i)->hdr.type) && GET_NODE_VEC_SIZE(GET_CHILD(n,i)) > 1) {
						transfer_masks[i] = mask;
					} else {
						transfer_masks[i] = 1;
					}
				}
			}
			break;

			/* Operations that spread liveness out to all components of their inputs */
		case EXPR_OP_NOT:
		case EXPR_OP_LT:
		case EXPR_OP_LE:
		case EXPR_OP_EQ:
		case EXPR_OP_NE:
		case EXPR_OP_GE:
		case EXPR_OP_GT:
		case EXPR_OP_INDEX:
		case EXPR_OP_SUBVECTOR_ACCESS:
		case EXPR_OP_SUBVECTOR_UPDATE:
		case EXPR_OP_FUN_LENGTH:
		case EXPR_OP_FUN_DISTANCE:
		case EXPR_OP_FUN_DOT:
		case EXPR_OP_FUN_NORMALIZE:
		case EXPR_OP_FUN_ANY:
		case EXPR_OP_FUN_ALL:
		case EXPR_OP_FUN_REFLECT:
		case EXPR_OP_FUN_REFRACT:
		case EXPR_OP_FUN_M200_HADD:
		case EXPR_OP_FUN_M200_ATAN_IT1:
		case EXPR_OP_FUN_M200_ATAN_IT2:
		case EXPR_OP_FUN_M200_MOV_CUBEMAP:

			/* If any output components are live, all input components are live */
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				node *child = GET_CHILD(n,i);
				transfer_masks[i] = child ? FULL_MASK(child) : 0;
			}
			break;

			/* texturing, 1-component coordinate */
		case EXPR_OP_FUN_TEXTURE1D:
		case EXPR_OP_FUN_TEXTURE1DLOD:
			/* If any output components are live, all input components are live */
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				node *child = GET_CHILD(n,i);
				transfer_masks[i] = 0;
				if(child != NULL)
				{
					unsigned mask2;
					if(i == 1)
					{
						mask2 = 0x1;
					} else {
						mask2 = FULL_MASK(child);
					}

					transfer_masks[i] = mask2;
				}
			}
			break;

			/* texturing, 2-component coordinate */
		case EXPR_OP_FUN_TEXTURE1DPROJ:
		case EXPR_OP_FUN_TEXTURE1DPROJLOD:
		case EXPR_OP_FUN_TEXTURE2D:
		case EXPR_OP_FUN_TEXTURE2DLOD:
		case EXPR_OP_FUN_TEXTURE2DGRAD_EXT:
		case EXPR_OP_FUN_TEXTURE2DLOD_EXT:
		case EXPR_OP_FUN_TEXTUREEXTERNAL:
			/* If any output components are live, all input components are live */
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				node *child = GET_CHILD(n,i);
				transfer_masks[i] = 0;
				if(child != NULL)
				{
					unsigned mask2;
					if(i == 1)
					{
						mask2 = 0x3;
					} else {
						mask2 = FULL_MASK(child);
					}

					transfer_masks[i] = mask2;
				}
			}
			break;

			/* texturing, 3-component coordinate */
		case EXPR_OP_FUN_TEXTURE2DPROJ:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_EXT:
		case EXPR_OP_FUN_TEXTURE3D:
		case EXPR_OP_FUN_TEXTURE3DLOD:
		case EXPR_OP_FUN_TEXTURECUBE:
		case EXPR_OP_FUN_TEXTURECUBELOD:
		case EXPR_OP_FUN_TEXTURECUBELOD_EXT:
		case EXPR_OP_FUN_SHADOW2D:
		case EXPR_OP_FUN_SHADOW2DLOD:
		case EXPR_OP_FUN_TEXTUREEXTERNALPROJ:
		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_EXT:
		case EXPR_OP_FUN_TEXTURECUBEGRAD_EXT:
			/* If any output components are live, all input components are live */
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				node *child = GET_CHILD(n,i);
				transfer_masks[i] = 0;
				if(child != NULL)
				{
					unsigned mask2;
					if(i == 1)
					{
						mask2 = 0x7;
					} else {
						mask2 = FULL_MASK(child);
					}

					transfer_masks[i] = mask2;
				}
			}
			break;

			/* texturing, 4-component coordinate */
		case EXPR_OP_FUN_TEXTURE2DPROJ_W:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W:
		case EXPR_OP_FUN_TEXTURE2DPROJLOD_W_EXT:
		case EXPR_OP_FUN_TEXTURE3DPROJ:
		case EXPR_OP_FUN_TEXTURE3DPROJLOD:
		case EXPR_OP_FUN_SHADOW2DPROJ:
		case EXPR_OP_FUN_SHADOW2DPROJLOD:
		case EXPR_OP_FUN_TEXTUREEXTERNALPROJ_W:
		case EXPR_OP_FUN_TEXTURE2DPROJGRAD_W_EXT:
			/* If any output components are live, all input components are live */
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				node *child = GET_CHILD(n,i);
				transfer_masks[i] = 0;
				if(child != NULL)
				{
					unsigned mask2;
					if(i == 1)
					{
						mask2 = 0xF;
					} else {
						mask2 = FULL_MASK(child);
					}

					transfer_masks[i] = mask2;
				}
			}
			break;


		case EXPR_OP_SWIZZLE:
			{
				unsigned int mask = (unsigned int) MASK(n);
				input_mask = 0;
				for (i = 0 ; i < GET_NODE_VEC_SIZE(n); i++)
				{
					if (BIT_TEST(mask,i))
					{
						assert(n->expr.u.swizzle.indices[i] != -1);
						input_mask |= 1 << n->expr.u.swizzle.indices[i];
					}
				}
				transfer_masks[0] = input_mask;
			}
			break;
			
		case EXPR_OP_FUN_CROSS:
			{
				unsigned int mask = (unsigned int) MASK(n);
				input_mask = ((mask << 2) | (mask << 1) | (mask >> 1) | (mask >> 2)) & 7;
				transfer_masks[0] = input_mask;
				transfer_masks[1] = input_mask;
			}
			break;

		case EXPR_OP_FUN_FACEFORWARD:
			{
				unsigned int mask = (unsigned int) MASK(n);
				transfer_masks[0] = mask;
				transfer_masks[1] = FULL_MASK(GET_CHILD(n,1));
				transfer_masks[2] = FULL_MASK(GET_CHILD(n,2));
			}
			break;

		case EXPR_OP_FUN_M200_HADD_PAIR:
			{
				unsigned int mask = (unsigned int) MASK(n);
				transfer_masks[0] = mask | (mask << 1);
				transfer_masks[1] = mask;
			}
			break;

		case EXPR_OP_FUN_M200_POS:
		case EXPR_OP_FUN_M200_POINT:
		case EXPR_OP_FUN_M200_MISC_VAL:
		case EXPR_OP_FUN_M200_LD_RGB:
		case EXPR_OP_FUN_M200_LD_ZS:
			/* no sources */
			break;

		case EXPR_OP_UNKNOWN:
		case EXPR_OP_MEMBER_OR_SWIZZLE:
		case EXPR_OP_PRE_INC:
		case EXPR_OP_POST_INC:
		case EXPR_OP_PRE_DEC:
		case EXPR_OP_POST_DEC:
		case EXPR_OP_CHAIN:
		case EXPR_OP_ASSIGN:
		case EXPR_OP_ADD_ASSIGN:
		case EXPR_OP_SUB_ASSIGN:
		case EXPR_OP_MUL_ASSIGN:
		case EXPR_OP_DIV_ASSIGN:
		case EXPR_OP_CONDITIONAL:
		case EXPR_OP_EXPLICIT_TYPE_CONVERT:
		case EXPR_OP_IMPLICIT_TYPE_CONVERT:
		case EXPR_OP_FUN_MATRIXCOMPMULT:

			/* these operations should not occur */
			assert(0);
			return MEM_ERROR;

		}

	return MEM_OK;
}

static memerr transfer_liveness(node *n)
{
	unsigned transfer_masks[N_COMPONENTS];
	unsigned i;
	unsigned mask = MASK(n);
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		transfer_masks[i] = UNDEFINED_MASK;
	}
	/* If no components are live, no liveness is transfered */
	if (mask == 0) return MEM_OK;

	switch (n->hdr.kind)
	{
	case EXPR_KIND_UNARY:
	case EXPR_KIND_BINARY:
	case EXPR_KIND_TERNARY:
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		ESSL_CHECK(transfer_liveness_for_expression(n, transfer_masks));
		
		break;

	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		if (GET_N_CHILDREN(n) == 1 && GET_NODE_VEC_SIZE(GET_CHILD(n,0)) == 1) {
			/* Scalar to vector constructor */
			transfer_masks[0] = 1;
		} else {
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
			{
				unsigned size = GET_NODE_VEC_SIZE(GET_CHILD(n,i));
				transfer_masks[i] = mask & ((1 << size) - 1);
				mask = mask >> size;
			}
		}
		break;
	case EXPR_KIND_MALIGP2_STORE_CONSTRUCTOR:
		for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
		{
			unsigned size = GET_NODE_VEC_SIZE(GET_CHILD(n,i));
			transfer_masks[i] = mask & ((1 << size) - 1);
			mask = mask >> size;
		}
		break;

	case EXPR_KIND_PHI:
		/* Propagate to all sources of the phi node */
	{
		phi_source *phi_s;
		for (phi_s = n->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next)
		{
			ESSL_CHECK(merge_liveness(phi_s->source, mask));
		}
		break;
	}

	case EXPR_KIND_VECTOR_COMBINE:
	{
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			transfer_masks[i] = mask & mask_from_combiner(&n->expr.u.combiner, i);
		}
		break;
	}


	case EXPR_KIND_CONSTANT:
	case EXPR_KIND_DONT_CARE:
	case EXPR_KIND_FUNCTION_CALL:
	case EXPR_KIND_VARIABLE_REFERENCE:
		/* No sources */
		break;

	case EXPR_KIND_TRANSFER:
		for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
		{
			transfer_masks[i] = mask;
		}
		break;

	case EXPR_KIND_TYPE_CONVERT:
		for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
		{
			transfer_masks[i] = mask;
		}
		break;
		
	case EXPR_KIND_LOAD:
	{
		node *address = GET_CHILD(n,0);
		transfer_masks[0] = address ? FULL_MASK(address) : 0; /* addresses are live if any of the components are live */
		break;
	}
	case EXPR_KIND_STORE:
	{
		node *address = GET_CHILD(n,0);
		transfer_masks[0] = address ? FULL_MASK(address) : 0; /* addresses are live if any of the components are live */
		transfer_masks[1] = mask;
		break;
	}

	default:
		/* No other operations should occur */
		assert(0);
		return MEM_ERROR;
	}

	for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
	{
		assert(transfer_masks[i] != UNDEFINED_MASK);
		ESSL_CHECK(merge_liveness(GET_CHILD(n,i), transfer_masks[i]));
	}

	return MEM_OK;
}

static memerr mark_all_liveness(control_flow_graph *cfg) {
	unsigned i;
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		control_dependent_operation *cdo;
		if (block->source) {
			ESSL_CHECK(merge_liveness(block->source, FULL_MASK(block->source)));
		}

		for (cdo = block->control_dependent_ops ; cdo != 0 ; cdo = cdo->next) {
			if (cdo->op->hdr.kind == EXPR_KIND_STORE) {
				ESSL_CHECK(merge_liveness(cdo->op, FULL_MASK(cdo->op)));
			}
		}
	}

	return MEM_OK;
}


static swizzle_pattern make_mask_transfer_swizzle(unsigned from_mask, unsigned to_mask, unsigned *size_ptr) {
	unsigned to_i;
	unsigned from_i = 0;
	unsigned size = 0;
	swizzle_pattern swz = _essl_create_undef_swizzle();
	for (to_i = 0 ; to_i < N_COMPONENTS ; to_i++) {
		if (BIT_TEST(to_mask,to_i) && (unsigned)(1 << from_i) <= from_mask) {
			while (!BIT_TEST(from_mask,from_i)) from_i++;
			swz.indices[to_i] = from_i++;
			size = to_i + 1;
		}
	}
	if(size_ptr != NULL) *size_ptr = size;
	return swz;
}

static memerr detach_dead_nodes(node *n, mempool *pool, typestorage_context *ts_ctx, ptrset *visited) 
{
	node *new_node = NULL;
	ESSL_CHECK(_essl_ptrset_insert(visited, n));
	switch (n->hdr.kind) {
	case EXPR_KIND_VECTOR_COMBINE:
		{
			unsigned c;
			swizzle_pattern live_swizzle = _essl_create_undef_swizzle();
			swizzle_pattern combiner_map = _essl_create_undef_swizzle();
			unsigned n_active_children = 0;

			for (c = 0 ; c < GET_N_CHILDREN(n) ; c++) {
				unsigned combine_mask = mask_from_combiner(&n->expr.u.combiner, c);
				unsigned cmask = MASK(n) & combine_mask;
				if(cmask != 0)
				{
					live_swizzle.indices[n_active_children] = c;
					combiner_map.indices[c] = n_active_children;
					n_active_children++;
				}
			}
			
			if(n_active_children != GET_N_CHILDREN(n)) /* dead components, go kill */
			{
				unsigned i;
				if(n_active_children == 1)
				{
					/* can turn the whole thing into a swizzle */
					n->hdr.kind = EXPR_KIND_UNARY;
					n->expr.operation = EXPR_OP_SWIZZLE;
					for (i = 0 ; i < N_COMPONENTS ; i++) {
						if (MASK(n) & (1 << i)) {
							n->expr.u.swizzle.indices[i] = i;
						} else {
							n->expr.u.swizzle.indices[i] = -1;
						}
					}
					SET_CHILD(n, 0, GET_CHILD(n, live_swizzle.indices[0]));
					ESSL_CHECK(SET_N_CHILDREN(n, 1, pool));
					_essl_swizzle_patch_dontcares(&n->expr.u.swizzle, GET_NODE_VEC_SIZE(n));
				} else {
					/* slim the combiner and patch up with swizzle afterwards */
					unsigned n_comps = 0;
					swizzle_pattern child_swzs[N_COMPONENTS];
					for(i = 0; i < GET_N_CHILDREN(n); ++i)
					{
						child_swzs[i] = _essl_create_undef_swizzle();
					}
					ESSL_CHECK(new_node = _essl_new_vector_combine_expression(pool, n_active_children));
					_essl_ensure_compatible_node(new_node, n);
					for(i = 0; i < N_COMPONENTS; ++i)
					{
						if((MASK(n) & (1<<i)))
						{
							new_node->expr.u.combiner.mask[n_comps] = combiner_map.indices[n->expr.u.combiner.mask[i]];

							child_swzs[combiner_map.indices[n->expr.u.combiner.mask[i]]].indices[n_comps] = i;

							++n_comps;
						}
					}
					
					for(c = 0; c < n_active_children; ++c)
					{
						node *child = GET_CHILD(n, live_swizzle.indices[c]);
						if(!_essl_is_identity_swizzle(child_swzs[c]))
						{
							node *swz_node;
							ESSL_CHECK(swz_node = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, child));
							swz_node->expr.u.swizzle = child_swzs[c];

							_essl_ensure_compatible_node(swz_node, new_node);
							SET_MASK(swz_node, _essl_mask_from_swizzle_output(child_swzs[c]));
							child = swz_node;
						}
						SET_CHILD(new_node, c, child);
					}
					/* Fill in new constructor node */
					ESSL_CHECK(new_node->hdr.type = _essl_get_type_with_given_vec_size(ts_ctx, n->hdr.type, n_comps));
					ESSL_CHECK(SET_N_CHILDREN(new_node, n_active_children, pool));
					SET_MASK(new_node, (1 << n_comps) - 1);

					/* Rewrite old node to swizzle */
					n->hdr.kind = EXPR_KIND_UNARY;
					n->expr.operation = EXPR_OP_SWIZZLE;
					n->expr.u.swizzle = make_mask_transfer_swizzle(MASK(new_node), MASK(n), NULL);
					_essl_swizzle_patch_dontcares(&n->expr.u.swizzle, GET_NODE_VEC_SIZE(n));
					SET_CHILD(n, 0, new_node);
					ESSL_CHECK(SET_N_CHILDREN(n, 1, pool));

				}
			}
		}
		break;
	case EXPR_KIND_BUILTIN_CONSTRUCTOR:
		if (MASK(n) != FULL_MASK(n) && GET_N_CHILDREN(n) > 1) {
			unsigned i;
			unsigned j = 0;
			unsigned n_comps = 0;
			unsigned mask = MASK(n);
			/* Process and compact children */
			ESSL_CHECK(new_node = _essl_clone_node(pool, n));
			for (i = 0 ; i < GET_N_CHILDREN(n) ; i++) {
				node *child = GET_CHILD(n,i);
				unsigned cmask = mask & FULL_MASK(child);
				if (cmask == FULL_MASK(child)) {
					/* Child is fully live. Just refer to it directly. */
					SET_CHILD(new_node, j++, child);
					n_comps += GET_NODE_VEC_SIZE(child);
				} else if (cmask != 0) {
					/* Child is partially live. Swizzle it. */
					node *swz_node;
					unsigned size;
					ESSL_CHECK(swz_node = _essl_new_unary_expression(pool, EXPR_OP_SWIZZLE, child));
					swz_node->expr.u.swizzle = make_mask_transfer_swizzle(cmask, FULL_MASK(child), &size);

					ESSL_CHECK(swz_node->hdr.type = _essl_get_type_with_given_vec_size(ts_ctx, child->hdr.type, size));
					SET_MASK(swz_node, (1 << size) - 1);
					SET_CHILD(new_node, j++, swz_node);
					n_comps += size;
				} else {
					/* Child is dead. Skip it. */
				}
				mask = mask >> GET_NODE_VEC_SIZE(child);
			}

			/* Fill in new constructor node */
			ESSL_CHECK(new_node->hdr.type = _essl_get_type_with_given_vec_size(ts_ctx, n->hdr.type, n_comps));
			ESSL_CHECK(SET_N_CHILDREN(new_node, j, pool));
			SET_MASK(new_node, (1 << n_comps) - 1);

			/* Rewrite old node to swizzle */
			n->hdr.kind = EXPR_KIND_UNARY;
			n->expr.operation = EXPR_OP_SWIZZLE;
			n->expr.u.swizzle = make_mask_transfer_swizzle(MASK(new_node), MASK(n), NULL);
			_essl_swizzle_patch_dontcares(&n->expr.u.swizzle, GET_NODE_VEC_SIZE(n));
			SET_CHILD(n, 0, new_node);
			ESSL_CHECK(SET_N_CHILDREN(n, 1, pool));

		}
		break;
	default:
		break;
	}

	/* Visit children */
	{
		unsigned i;
		for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
		{
			node *child = GET_CHILD(n, i);
			if (child != 0 && !_essl_ptrset_has(visited, child))
			{
				assert(MASK(child) != 0);
				ESSL_CHECK(detach_dead_nodes(child, pool, ts_ctx, visited));
			}
		}
	}

	return MEM_OK;
}

static essl_bool control_dependent_operation_removable(control_dependent_operation *cdo) {
	/* NOTE: When function call is implemented, find out how dead argument and formal loads can be safely removed. */
	node *op = cdo->op;
	return !(op->hdr.kind == EXPR_KIND_FUNCTION_CALL);
}

static memerr remove_dead_nodes(control_flow_graph *cfg, mempool *pool, typestorage_context *ts_ctx)
{
	unsigned i;
	ptrset removed_cdos;
	ptrset visited;
	ESSL_CHECK(_essl_ptrset_init(&removed_cdos, pool));
	ESSL_CHECK(_essl_ptrset_init(&visited, pool));

	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		phi_list **phi_p;
		control_dependent_operation **cdo_p;

		/* Check block source */
		if (block->source) {
			ESSL_CHECK(detach_dead_nodes(block->source, pool, ts_ctx, &visited));
		}

		/* Check phi nodes */
		for (phi_p = &block->phi_nodes ; *phi_p != 0 ;)
		{
			if (MASK((*phi_p)->phi_node) == 0) {
				/* Phi node is dead - remove it */
				*phi_p = (*phi_p)->next;
			} else {
				/* Phi node is not dead - visit all sources */
				phi_source *phi_s;
				for (phi_s = (*phi_p)->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
					ESSL_CHECK(detach_dead_nodes(phi_s->source, pool, ts_ctx, &visited));
				}

				phi_p = &(*phi_p)->next;
			}
		}

		/* Check control dependent operations */
		for (cdo_p = &block->control_dependent_ops ; *cdo_p != 0 ;) {
			if (MASK((*cdo_p)->op) == 0) {
				/* Operation is dead */
				if (control_dependent_operation_removable(*cdo_p)) {
					/* Remove it */
					ESSL_CHECK(_essl_ptrset_insert(&removed_cdos, *cdo_p));
					*cdo_p = (*cdo_p)->next;
				} else {
					cdo_p = &(*cdo_p)->next;
				}
			} else {
				ESSL_CHECK(detach_dead_nodes((*cdo_p)->op, pool, ts_ctx, &visited));
				cdo_p = &(*cdo_p)->next;
			}
		}

	}

	/* Remove control dependencies to dead control dependent operations */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		control_dependent_operation *cdo;
		for (cdo = block->control_dependent_ops ; cdo != 0 ; cdo = cdo->next) {
			operation_dependency **dep_p;
			for (dep_p = &cdo->dependencies ; *dep_p != 0 ;) {
				if (_essl_ptrset_has(&removed_cdos, (*dep_p)->dependent_on)) {
					*dep_p = (*dep_p)->next;
				} else {
					dep_p = &(*dep_p)->next;
				}
			}
		}
	}

	return MEM_OK;
}

memerr _essl_remove_dead_code(mempool *pool, symbol *function, typestorage_context *ts_ctx)
{
	control_flow_graph *cfg = function->control_flow_graph;

	/* Mark liveness on all reachable nodes */
	ESSL_CHECK(mark_all_liveness(cfg));

	/* Remove dead nodes */
	ESSL_CHECK(remove_dead_nodes(cfg, pool, ts_ctx));

	return MEM_OK;
}
