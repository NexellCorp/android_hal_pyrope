/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_texture_object.h"
#include <gles_incremental_rendering.h>
#include <gles_fb_interface.h>

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

mali_err_code _gles_texture_miplevel_assign (
	struct gles_context* ctx,
	struct gles_texture_object *tex_obj,
	int mipchain,
	int miplevel,
	GLenum format,
	GLenum type,
	int surfaces_count,
	mali_surface** surfaces)
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	gles_mipmap_level* front_level;
	struct gles_fb_texture_memory* texmem;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal ); /* the internal object will NEVER be NULL */

	/* update the frontend object. The get_mipmap_level_assure function will allocate and assign a frontend object with the input data, or
	 * re-assign it to the given values if already present. If there is no surface, we're assigning it to default values. The frontend
	 * objects are never deleted unless the texture object is deleted. */

	MALI_DEBUG_ASSERT_POINTER( (surfaces_count>0)?(surfaces[0]):((void*)0xFFFFFFFF) );
	MALI_DEBUG_ASSERT_POINTER( (surfaces_count>1)?(surfaces[1]):((void*)0xFFFFFFFF) );
	MALI_DEBUG_ASSERT_POINTER( (surfaces_count>2)?(surfaces[2]):((void*)0xFFFFFFFF) );

	if(surfaces_count>0)
	{
		front_level = _gles_texture_object_get_mipmap_level_assure( tex_obj, mipchain, miplevel, surfaces[0]->format.width, surfaces[0]->format.height, format, type);
		if( NULL == front_level ) return MALI_ERR_OUT_OF_MEMORY;
	} else {
		front_level = _gles_texture_object_get_mipmap_level_assure( tex_obj, mipchain, miplevel, 0, 0, format, type);
		if( NULL == front_level ) return MALI_ERR_OUT_OF_MEMORY;
	}

	texmem = _gles_fb_texture_object_get_memblock(tex_obj->internal, miplevel, 0, NULL); /* fetch plane 0. Plane 1+ should have the same deps as plane 0 */
	MALI_DEBUG_ASSERT_POINTER(texmem);

	if ( texmem->flag_dont_replace )
	{
		struct gles_fb_texture_object *old_internal = tex_obj->internal;
		struct gles_fb_texture_object *new_internal = NULL;

		/* The old memory has read GLES-scheme dependencies on it. This means assignment is illegal. 
		 *
		 * Before we can assign any surface to this texmem block, we need to make a new backend object
		 * where this state is not true. 
		 */

		new_internal = _gles_fb_texture_object_copy( old_internal);
		if( NULL == new_internal ) return MALI_ERR_OUT_OF_MEMORY;

		tex_obj->internal = new_internal;
		_gles_fb_texture_object_deref(old_internal);
		
	}

	/* it is now safe to upload the new mipmap level. If the input surface is NULL, we're not replacing anything */
	if(surfaces != NULL && surfaces_count > 0)
	{
		/* this call will automatically free any old surface binding at this location. */
		_gles_fb_texture_object_assign(tex_obj->internal, mipchain, miplevel, surfaces_count, surfaces);
		
		/* set texobj state. If num uploaded surfaces is 1, we only have one texobj */
		if(tex_obj->internal->num_uploaded_surfaces == 1)
		{
			tex_obj->internal->red_blue_swap = surfaces[0]->format.red_blue_swap;
			tex_obj->internal->order_invert  = surfaces[0]->format.reverse_order;
			tex_obj->internal->layout = surfaces[0]->format.texel_layout;
			tex_obj->internal->linear_pitch_lvl0 = (surfaces[0]->format.pitch<<miplevel);
#if defined(USING_MALI400) || defined(USING_MALI450)
			tex_obj->internal->using_td_pitch_field = (tex_obj->internal->layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR);
#else 
			tex_obj->internal->using_td_pitch_field = MALI_FALSE;
#endif
		}
	} 
	else 
	{
		/* no assign surfaces given; equivalent to unassign everything */
		_gles_fb_texture_object_assign(tex_obj->internal, mipchain, miplevel, surfaces_count, surfaces);
	}

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	if ( NULL != front_level->fbo_connection )
	{
		_gles_fbo_bindings_surface_changed( front_level->fbo_connection );
	}
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

	/* update td for the changes to addresses and sizes */
	_gles_m200_td_level_change( tex_obj, miplevel );
	tex_obj->dirty = MALI_TRUE;
	tex_obj->completeness_check_dirty = MALI_TRUE;

	return err;
}

mali_err_code _gles_texture_reset(
	struct gles_context* ctx,
	struct gles_texture_object *tex_obj)
{
	int mipchain, miplevel;
	mali_err_code err;
	mali_err_code return_err = MALI_ERR_NO_ERROR;
	
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );

	for(mipchain = 0; mipchain < GLES_MAX_MIPCHAINS; mipchain++)
	{
		gles_mipchain* chain = tex_obj->mipchains[mipchain];
		if( NULL == chain ) continue;

		for(miplevel = 0; miplevel < GLES_MAX_MIPMAP_LEVELS; miplevel++)
		{
			gles_mipmap_level* level = chain->levels[miplevel];
			if( NULL == level ) continue;
			err = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, miplevel, 0, 0, 0, NULL);
			if(err != MALI_ERR_NO_ERROR) return_err = err;
		}
	}
	return return_err;
}

struct mali_surface* _gles_texture_miplevel_allocate(
	struct gles_context* ctx,
	struct gles_texture_object *tex_obj,
	int mipchain,
	int miplevel,
	int width,
	int height,
	GLenum format,
	GLenum type
	)
{

	s32 pitch = 0;
	mali_bool support_rbswap, support_revorder;
	struct mali_surface_specifier sformat;
	struct mali_surface* old_surface;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );
	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* fetch what is already present in the old surface, if anything */
	old_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel );
	
	/* find a surface format matching the GLES specifiers AND the texture object */
	_gles_m200_get_storage_surface_format(&sformat, type, format, width, height);

	/* if there is > 1 surface, or one surface which we are NOT replacing, adhere to the texture object rules 
	 * otherwise, just use what match the input data (as given by _gles_m200_get_storage_surface_format) */
	if(tex_obj->internal->num_uploaded_surfaces > 1 || 
	   (tex_obj->internal->num_uploaded_surfaces == 1 && old_surface == NULL))   /* if there is one texture and that one is at the same location, then old_surface is non-null */
	{
		__m200_texel_format_flag_support(sformat.texel_format, &support_rbswap, &support_revorder);
	
		sformat.red_blue_swap = tex_obj->internal->red_blue_swap & support_rbswap;
		sformat.reverse_order = tex_obj->internal->order_invert & support_revorder;
	
		sformat.texel_layout = tex_obj->internal->layout;
		sformat.pixel_layout = _mali_texel_layout_to_pixel_layout(tex_obj->internal->layout);
		
		if(sformat.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR) 
		{
			int bpp = _mali_surface_specifier_bpp( &sformat ) / 8;
			/* Linear mode: find the perfect new pitch for the new miplevel
			 * 
			 * When using_td_pitch_field is MALI_TRUE, all miplevels have the same pitch. 
			 *
			 * When it is false, all miplevels are half-sized from the last level */
			if( tex_obj->internal->using_td_pitch_field )
			{
				pitch = tex_obj->internal->linear_pitch_lvl0;
			} else {
				pitch = (tex_obj->internal->linear_pitch_lvl0 >> miplevel);
			}

			/* if this pitch requirement means we cannot make the surface writeable, so be it */
			if( pitch % MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT != 0 )
			{
				/* surface not writeable. Stop pretending it is */
				sformat.pixel_format = MALI_PIXEL_FORMAT_NONE;
			}

			/* Also, it is quite possible that this pitch don't FIT the surface we are creating
			 * This can happen when uploading some miplevel that don't fit in the chain 
			 * In this case, just use whatever pitch is available. it will lead to a constraint
			 * resolve almost no matter what. */
			if( tex_obj->internal->using_td_pitch_field )
			{
				if(pitch < width * bpp)
				{
					/* use automatic pitch instead */
					pitch = 0;
				}
			} else {
				/* if we don't use the pitch field, the pitch must be tight.
				 * Failure to meet the tight pitch will hit bugzilla entry 9359, which is unresolvable
				 * unless you use the pitch field. At this point, The miplevels will 
				 * all be allocated as tight as possible, and we'll just sort it out later
				 * whether we have to do a constraint pitch resolve or not. This is 
				 * possibly suboptimal, but at least it means we will not trigger bugzilla entry 9359. */
				if(pitch != width * bpp)
				{
					/* use automatic pitch instead */
					pitch = 0;
				}
			}

			/* if the pitch we decided upon don't fit the HW constraints, lower surface requirements */
			if( !tex_obj->internal->using_td_pitch_field && (pitch % bpp) != 0)
			{
				/* pitch not divisible by bpp, use autopitch */
				pitch = 0;
			}
		}

	}

	sformat.width = width;
	sformat.height = height;
	sformat.pitch = pitch;

	return _gles_fb_surface_alloc( tex_obj->internal, mipchain, miplevel, &sformat);
}

/***** deep CoW handling for partial surface updates requires a special consumer dummy :
       - we get the CoW callback first which needs:
	     . to setup a read-dep on the old resource (surface)
	     . to re-allocate the resource
	   - eventually we get the activation callback, at whose time we are allowed to read from
	     the previous surface and can execute the copy operation
******/
typedef struct _gles_surface_dummy_consumer
{
	mali_ds_consumer_handle          handle;
	mali_surface_deep_cow_descriptor desc;
} _gles_surface_dummy_consumer;

MALI_STATIC mali_ds_resource_handle _gles_surface_direct_write_do_cow(void* resource_owner, void * consumer_owner)
{
	mali_ds_resource_handle retval;
	mali_surface* surface = MALI_STATIC_CAST(mali_surface*)resource_owner;
	_gles_surface_dummy_consumer *surface_consumer = (_gles_surface_dummy_consumer*)consumer_owner;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( surface_consumer );

	/* setup an additional read dependency on the old surface so that we don't start too early */
	if ( MALI_ERR_NO_ERROR != mali_ds_connect(surface_consumer->handle, surface->ds_resource, MALI_DS_READ) )
	{
		return NULL;
	}

	/* only re-allocate, don't copy yet */
	_mali_surface_access_lock(surface);
	retval = _mali_surface_clear_dependencies( surface, &surface_consumer->desc);
	_mali_surface_access_unlock(surface);

	return retval;
}

MALI_STATIC void _gles_surface_dummy_consumer_activate(mali_base_ctx_handle base_ctx, void * consumer_owner, mali_ds_error error_value)
{
	MALI_IGNORE(base_ctx);

	if ( MALI_DS_ERROR != error_value )
	{
		/* We have been activated, now it is safe to do the copy operation */
		_gles_surface_dummy_consumer *surface_consumer = (_gles_surface_dummy_consumer*)consumer_owner;

		if ( surface_consumer->desc.src_mem_ref )
		{
			_mali_mem_copy( surface_consumer->desc.dest_mem_ref->mali_memory,surface_consumer->desc.mem_offset,
							surface_consumer->desc.src_mem_ref->mali_memory, surface_consumer->desc.mem_offset,
							surface_consumer->desc.data_size );
			_mali_shared_mem_ref_owner_deref(surface_consumer->desc.src_mem_ref);
		}
	}
}

struct mali_surface* _gles_texture_miplevel_lock(
	struct gles_context* ctx,
	struct gles_texture_object* tex_obj,
	int mipchain,
	int miplevel )
{
	struct mali_surface* old_surface;
	gles_mipchain* front_mipchain;
	gles_mipmap_level *front_miplevel;
	mali_bool is_renderable;
	
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );

	old_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel );

	/* the calling function need to ensure that the old surface exist */
	MALI_DEBUG_ASSERT_POINTER(old_surface); 
		
	/* if renderable, add write dependencies to the miplevel */
	front_mipchain = _gles_texture_object_get_mipmap_chain( tex_obj, mipchain );
	MALI_DEBUG_ASSERT_POINTER( front_mipchain ); /* if the back object exist, the front object should too! */
	front_miplevel = front_mipchain->levels[miplevel];
	MALI_DEBUG_ASSERT_POINTER( front_miplevel ); /* if the back object exist, the front object should too! */
	is_renderable = _gles_fb_texture_object_get_renderable(tex_obj->internal, mipchain, miplevel);

	/* before locking, there should be no temp consumer */
	MALI_DEBUG_ASSERT(front_miplevel->temp_consumer == MALI_NO_HANDLE, 
	                  ("No consumer should be set now. Double lock?"));

	if(is_renderable)
	{
		struct _gles_surface_dummy_consumer dummy_consumer;
		mali_err_code err;
		
		/* Special concern due to bugzilla entry 6400:
		 *
		 * Pixmaps will use the GLES read dependencies, but not the surface read dependencies.
		 * The reader of a pixmap need to be flushed out. We don't really have that handle, but
		 * since it is a GLES read dependency, we know that it is one of the framebuilders (including the default one)
		 * in GLES that caused the read to occur. Ideally we incremental render all of these, but that's a lot of work.
		 * So the cheap solution is to only incremental render the current framebuilder. It is not perfect,
		 * but then again, it is an open issue and this is just an imperfect workaround.
		 */
		if(    old_surface            /* there is an old surface */
		   &&  (old_surface->flags & MALI_SURFACE_FLAG_DONT_MOVE ) /* is a pixmap or similar non-cow'able surface */
		   &&  (_mali_sys_atomic_get(&tex_obj->internal->ref_count) > 1 ) /* is in use by the GPU right now */
		  )
		{
			err = _gles_incremental_render( ctx, ctx->state.common.framebuffer.current_object );
			 /* If incremental rendering failed we need to perform a copy-on-write (The cap is a soft cap, i.e.
			  * we will try to free memory, but if we can't then we'll try copy-on-write) */
			if(err != MALI_ERR_NO_ERROR) return NULL;

			/* do a blocking wait on the previous frame when doing this incremental render. It is needed, 
			 * otherwise we cannot guarantee that the GPU is done before updating the texture below. */
			_mali_frame_builder_wait_all(ctx->state.common.framebuffer.current_object->draw_frame_builder);

		}

		/* set up a write dependency on the surface, wait for it to be ready */
		/* the write dependency is set up by making a new consumer, adding a write dep
		 * through it and the wait for the consumer to activate */
		dummy_consumer.handle = mali_ds_consumer_allocate(ctx->base_ctx, &dummy_consumer, _gles_surface_dummy_consumer_activate, NULL );
		dummy_consumer.desc.src_mem_ref = NULL;
		if(MALI_NO_HANDLE == dummy_consumer.handle) return NULL;

		/* CoW situations should preserve surface contents */
		mali_ds_consumer_set_callback_replace_resource(dummy_consumer.handle,
		                                               _gles_surface_direct_write_do_cow);
		
		/* set up a write dependency. This might trigger a CoW */
		err = mali_ds_connect(dummy_consumer.handle, old_surface->ds_resource, MALI_DS_WRITE);
		if(MALI_ERR_NO_ERROR != err)
		{
			/* kill all references, free temp consumer. */
			mali_ds_consumer_release_all_connections( dummy_consumer.handle );
			mali_ds_consumer_free(dummy_consumer.handle);
			if(dummy_consumer.desc.src_mem_ref) {
				_mali_shared_mem_ref_owner_deref(dummy_consumer.desc.src_mem_ref);
			}
			return NULL;
		}
		
		/* then wait for the consumer to activate */
		err = mali_ds_consumer_flush_and_wait( dummy_consumer.handle , VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_GLES_WAIT_MIPLEVEL );
		if(MALI_ERR_NO_ERROR != err)
		{
			/* kill all references, free temp consumer. */
			mali_ds_consumer_release_all_connections( dummy_consumer.handle );
			mali_ds_consumer_free(dummy_consumer.handle);
			if(dummy_consumer.desc.src_mem_ref) {
				_mali_shared_mem_ref_owner_deref(dummy_consumer.desc.src_mem_ref);
			}
			return NULL;
		}

		/* store the temp consumer handle */
		front_miplevel->temp_consumer = dummy_consumer.handle;

		/* update td for the changes to addresses and sizes */
		_gles_m200_td_level_change( tex_obj, miplevel );
		tex_obj->dirty = MALI_TRUE;
		tex_obj->completeness_check_dirty = MALI_TRUE;

		/* no GLES style dependencies need special handling, we use external dependencies. So end now */
		return old_surface;
	}

	/* Check if we have GLES dependencies (HW rendering to mipmap)
	 * or if the miplevel has already been flagged as READ ONLY. 
	 * Both cases are resolved by creating a new mali surface.  */
	if ( _mali_sys_atomic_get(&tex_obj->internal->ref_count) > 1 )
	{
		struct gles_fb_texture_object *old_internal = tex_obj->internal;
		struct gles_fb_texture_object *new_internal = NULL;
		struct mali_surface* new_surface = NULL;
		mali_err_code err;
		
		new_internal = _gles_fb_texture_object_copy( old_internal );
		if( NULL == new_internal ) return NULL;

		/* copy the old surfaces and make a new one 
                   TODO: multiplane surfaces? */ 
		_mali_surface_access_lock(old_surface);
		new_surface = _mali_surface_alloc_surface(old_surface, MALI_TRUE);
		_mali_surface_access_unlock(old_surface);
		if( NULL == new_surface) 
		{
			_gles_fb_texture_object_deref(new_internal);
			return NULL;
		}


		/* assign new internal to texobj */
		tex_obj->internal = new_internal;
		_gles_fb_texture_object_deref(old_internal);
	
		/* assign new surface to new internal */
		err = _gles_texture_miplevel_assign (ctx, tex_obj, mipchain, miplevel,  
				tex_obj->mipchains[mipchain]->levels[miplevel]->format,
				tex_obj->mipchains[mipchain]->levels[miplevel]->type,
				1, /* TODO: more than 1? bugzilla #11277*/
				&new_surface);
		if(err != MALI_ERR_NO_ERROR)
		{
			_mali_surface_deref(new_surface);
			return NULL;
		}


		return new_surface;
	}

	/* no outstanding dependencies. Just return the surface directly */
	return old_surface;


}

void _gles_texture_miplevel_unlock(
	struct gles_context* ctx,
	struct gles_texture_object* tex_obj,
	int mipchain,
	int miplevel )
{
	gles_mipchain* front_mipchain;
	gles_mipmap_level *front_miplevel;
	mali_bool is_renderable;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );

#ifndef NDEBUG
	{
		/* debug mode test only */
		struct mali_surface* old_surface;
		old_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel );
		MALI_DEBUG_ASSERT_POINTER(old_surface); 
	}
#endif

	/* if renderable, we need to unlock the dependency */
	front_mipchain = _gles_texture_object_get_mipmap_chain( tex_obj, mipchain );
	MALI_DEBUG_ASSERT_POINTER( front_mipchain ); /* if the back object exist, the front object should too! */
	front_miplevel = front_mipchain->levels[miplevel];
	MALI_DEBUG_ASSERT_POINTER( front_miplevel ); /* if the back object exist, the front object should too! */
	is_renderable = _gles_fb_texture_object_get_renderable(tex_obj->internal, mipchain, miplevel);

	if(is_renderable)
	{
		/* before unlocking, there should be a temp consumer if we are renderable */
		MALI_DEBUG_ASSERT(front_miplevel->temp_consumer != MALI_NO_HANDLE, 
		                  ("A consumer should be set now. Where did it go?"));
		mali_ds_consumer_release_all_connections( front_miplevel->temp_consumer );
		mali_ds_consumer_free(front_miplevel->temp_consumer);
		front_miplevel->temp_consumer = MALI_NO_HANDLE;
	}
}
