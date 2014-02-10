/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <base/mali_runtime.h>
#ifndef __USE_XOPEN_EXTENDED
#define  __USE_XOPEN_EXTENDED /* or __USE_BSD or __USE_XOPEN2K to get ftruncate in */
#endif
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include "egl_display.h"
#include "egl_handle.h"
#include "egl_platform.h"

#if EGL_KHR_image_system_ENABLE

static const char shm_client_handles[] = "shared_client_handles";
static const char shm_egl_image_names[] = "shared_image_name_handles";

#define EGL_IMAGE_NAME_ENTRY_FREED             (( EGLClientNameKHR )-1)
#define EGL_IMAGE_NAME_ENTRY_NEVER_ALLOCATED   (( EGLClientNameKHR )0)
/* relies on the semaphore being a binary one (thus just a cross-process mutex) */
#define ASSERT_SEM_TAKEN(sem) MALI_DEBUG_ASSERT((-1) == sem_trywait(sem),("Lock not taken"))

MALI_STATIC_INLINE void __egl_client_handles_sem_wait_sync( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display->sem_client_handles );
	sem_wait( display->sem_client_handles );
}

MALI_STATIC_INLINE void __egl_client_handles_sem_post_sync( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display->sem_client_handles );
	ASSERT_SEM_TAKEN(display->sem_client_handles);
	sem_post( display->sem_client_handles );
}

MALI_STATIC_INLINE void __egl_image_name_handles_sem_wait_sync( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display->sem_egl_image_names );
	sem_wait( display->sem_egl_image_names );
}

MALI_STATIC_INLINE void __egl_image_name_handles_sem_post_sync( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display->sem_egl_image_names );
	ASSERT_SEM_TAKEN(display->sem_egl_image_names);
	sem_post( display->sem_egl_image_names );
}

/*
 * frees the client names shared memory if there are no references after dereference
 * @note : display->sem_client_handles must be locked before entering this function
 */
MALI_STATIC void client_handles_deref( egl_display *display, egl_client_handles *handles )
{
	MALI_DEBUG_ASSERT_POINTER( handles );

	if ( 0 == --handles->references )
	{
		shm_unlink( shm_client_handles );
	}

	munmap( (void *)handles, sizeof( *handles ) );
	display->client_handles = NULL;

	if ( display->sem_client_handles )
	{
		__egl_client_handles_sem_post_sync( display );
		sem_close( display->sem_client_handles );
		display->sem_client_handles = NULL;
	}
}

/*
 * frees the egl image names shared memory if there are no references after dereference
 * @note :display->sem_egl_image_names must be locked before entering this function
 */
MALI_STATIC void image_name_handles_deref( egl_display *display, egl_image_name_handles *handles )
{
	MALI_DEBUG_ASSERT_POINTER( handles );

	if ( 0 == --handles->references )
	{
		shm_unlink( shm_egl_image_names );
	}

	munmap( (void *)handles, sizeof( *handles ) );
	display->shared_image_names = NULL;

	if( display->sem_egl_image_names )
	{
		__egl_image_name_handles_sem_post_sync( display );
		sem_close( display->sem_egl_image_names );
		display->sem_egl_image_names = NULL;
	}
}

MALI_STATIC void * get_shared_memory( const char *name, u32 size)
{
	EGLBoolean creator = EGL_FALSE;
	int fd;
	void *mptr = NULL;

	/* Create shared memory for client handles */
	fd = shm_open( name, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR );
	if ( -1 != fd )
	{
		/*  newly created shared memory object, allocate its memory and set to zero. */
		if ( 0 != ftruncate(fd, size) )
		{
			/*  new created shared memory object, allocate memory. allocate memory by writing to its file descriptor */
			close( fd );
			shm_unlink( name );
			return NULL;
		}
		creator = EGL_TRUE;
	}
	else
	{
		/* Try to open the existing one */
		fd = shm_open( name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
		if ( -1 == fd )
		{
			/* we failed - exit */
			return NULL;
		}

		/* Check its size - we must match what we would have allocated */
		if ( size != (u32)lseek(fd, 0, SEEK_END) )
		{
			shm_unlink( name );
			close( fd );
			return NULL;
		}
	}

	/* map memory to the current process */
	mptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close( fd );

	if ( MAP_FAILED == mptr )
	{
		if ( creator )
		{
			shm_unlink( name );
		}
		return NULL;
	}

	return mptr;
}

MALI_STATIC egl_client_handles* create_open_shared_client_handles( egl_display *display )
{
	egl_client_handles *mptr = NULL;

	__egl_client_handles_sem_wait_sync( display );
	mptr = (egl_client_handles *)get_shared_memory( shm_client_handles, sizeof( egl_client_handles ) );

	if ( mptr )
	{
		++mptr->references;
	}

	__egl_client_handles_sem_post_sync( display );

	return mptr;
}

egl_client_handles* __egl_get_add_client_name_handles( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display );

	if ( !display->client_handles )
	{
		sem_t *client_handles_sem = sem_open( shm_client_handles, O_CREAT, S_IRUSR | S_IWUSR, 1);
		if ( SEM_FAILED != client_handles_sem )
		{
			display->sem_client_handles = client_handles_sem;
			display->client_handles = create_open_shared_client_handles( display );
			if ( NULL == display->client_handles )
			{
				sem_close(client_handles_sem);
				display->sem_client_handles = NULL;
			}
		}
	}

	return display->client_handles;
}

EGLBoolean __egl_verify_client_name_valid( egl_display *display, EGLClientNameKHR target_client )
{
	egl_client_handles *handles  = NULL;
	egl_client_handle_entry *entry = NULL;
	EGLBoolean ret = EGL_FALSE;
	EGLint index = HANDLE_EGL_MASK( target_client );
	MALI_CHECK( HANDLE_IS_EGL_CLIENT_NAME( (u32)target_client ), EGL_FALSE );
	MALI_CHECK( ( index >= 0 ) && ( index < IMAGE_HANDLES_ENTRIES_MAX ), EGL_FALSE );
	MALI_CHECK( NULL != ( handles = __egl_get_add_client_name_handles( display ) ), EGL_FALSE );

	__egl_client_handles_sem_wait_sync( display );
	entry = handles->entry;
	MALI_DEBUG_ASSERT_POINTER( entry );

	if ( ( (PidType)-1 != entry[index].pid ) && ( 0 != entry[index].pid ) && ( display->native_dpy == entry[index].native_dpy ) )
	{
		ret = EGL_TRUE;
	}
	__egl_client_handles_sem_post_sync( display );
	return ret;
}

EGLClientNameKHR __egl_get_client_name( egl_display *display, PidType pid )
{
	int i = 0;
	EGLClientNameKHR ret = EGL_NO_CLIENT_NAME_KHR;
	egl_client_handles *handles = NULL;
	egl_client_handle_entry *entry = NULL;

	handles = display->client_handles;


	__egl_client_handles_sem_wait_sync( display );

	entry = handles->entry;
	MALI_DEBUG_ASSERT_POINTER( entry );

	while( ( i < IMAGE_HANDLES_ENTRIES_MAX ) && ( 0 != entry[i].pid ) )
	{
		/* continue for already free'd entries */
		if( (PidType)-1 == entry[i].pid )
		{
			i++;
			continue;
		}

		if( ( pid == entry[i].pid ) && ( display->native_dpy == entry[i].native_dpy ) )
		{
			/* found the match */
			ret = HANDLE_EGL_CLIENT_NAME( i );
			break;
		}
		i++;
	}
	__egl_client_handles_sem_post_sync( display );
	return ret;
}

void __egl_remove_client_name_handle( egl_display *display )
{
	PidType pid = (PidType)-1;
	int i = 0;
	egl_client_handles *handles = NULL;
	egl_client_handle_entry *entry = NULL;

	if( display && display->client_handles )
	{
		handles = display->client_handles;

		pid = _mali_sys_get_pid();

		entry = handles->entry;
		MALI_DEBUG_ASSERT_POINTER( entry );

		__egl_client_handles_sem_wait_sync( display );

		while ( ( i < IMAGE_HANDLES_ENTRIES_MAX ) && ( 0 != entry[i].pid ) )
		{
			if ( ( pid == entry[i].pid ) && ( display->native_dpy == entry[i].native_dpy ) )
			{
				entry[i].pid = (PidType)-1;		/* mark it as freed slot */
				entry[i].native_dpy = NULL;
				--handles->entries;
				break;
			}
			i++;
		}

		client_handles_deref( display, handles ); /* mutex is unlocked here */
	}
}

EGLClientNameKHR __egl_add_client_name( egl_display *display, PidType pid )
{
	int i = 0;
	EGLClientNameKHR ret = EGL_NO_CLIENT_NAME_KHR;
	egl_client_handles *handles = NULL;
	egl_client_handle_entry *entry = NULL;

	handles = display->client_handles;
	entry = handles->entry;
	MALI_DEBUG_ASSERT_POINTER( entry );

	__egl_client_handles_sem_wait_sync( display );

	while ( i < IMAGE_HANDLES_ENTRIES_MAX )
	{
		/* stop if we find pid of entry with value -1 (freed slot) or 0 (never allocated slot) */
		if ( ( (PidType)-1 == entry[i].pid ) || ( 0 == entry[i].pid ) )
		{
			entry[i].pid = pid;
			entry[i].native_dpy = display->native_dpy;
			handles->entries++;
			ret = HANDLE_EGL_CLIENT_NAME( i );
			break;
		}
		i++;
	}
	__egl_client_handles_sem_post_sync( display );
	return ret;
}

EGLImageNameKHR __egl_add_image_name( egl_display *display, egl_image *image, EGLClientNameKHR target_client )
{
	int i = 0;
	EGLClientNameKHR ret = EGL_NO_CLIENT_NAME_KHR;
	egl_image_name_handles *handles = NULL;
	egl_image_name_entry *entry = NULL;
	void *image_buffer = image->image_buffer;

	MALI_CHECK( NULL != image_buffer, EGL_NO_CLIENT_NAME_KHR );

	handles = display->shared_image_names;
	MALI_DEBUG_ASSERT_POINTER( handles);

	__egl_image_name_handles_sem_wait_sync( display );
	entry = handles->entry;
	MALI_DEBUG_ASSERT_POINTER( entry );

	while ( i < IMAGE_HANDLES_ENTRIES_MAX )
	{
		/* stop if we find slot marker entry with value -1 (freed slot) or 0 (never allocated slot) */
		if ( ( EGL_IMAGE_NAME_ENTRY_FREED == entry[i].target_client ) || ( EGL_IMAGE_NAME_ENTRY_NEVER_ALLOCATED == entry[i].target_client ) )
		{
			switch( image->target )
			{
				case EGL_NATIVE_PIXMAP_KHR:
					/* check that the buffer is UMP memory */
					MALI_CHECK( ((fbdev_pixmap *)image_buffer)->flags | FBDEV_PIXMAP_SUPPORTS_UMP, ret );
					_mali_sys_memcpy( &entry[i].image_data.pixmap.pixmap, image_buffer, sizeof( fbdev_pixmap ) );

					entry[i].image_data.pixmap.ump_unique_secure_id   = ump_secure_id_get( ((fbdev_pixmap *)image_buffer)->data );
					entry[i].image_data.pixmap.pixmap.data            = NULL;
					break;
#if defined( EGL_MALI_GLES) || defined(EGL_MALI_VG)
#ifdef EGL_MALI_GLES
				case EGL_GL_TEXTURE_2D_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
				case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
				case EGL_GL_RENDERBUFFER_KHR:
#endif /* EGL_MALI_GLES */
#ifdef EGL_MALI_VG
				case EGL_VG_PARENT_IMAGE_KHR:

#endif /* EGL_MALI_VG */
					_mali_sys_memcpy( &entry[i].image_data.buffer_properties, image_buffer, sizeof( egl_image_system_buffer_properties ) );
					entry[i].image_data.buffer_properties.ump = NULL;
					break;
#endif /* EGL_MALI_GLES || EGL_MALI_VG */
				default:
					/* unknown image target, return with error */
					__egl_image_name_handles_sem_post_sync( display );
					return ret;
			}
			entry[i].native_dpy                          = display->native_dpy;
			entry[i].target_client                       = target_client;
			entry[i].target_type                         = image->target;
			handles->entries++;
			ret = HANDLE_EGL_IMAGE_NAME( i );
			break;
		}
		i++;
	}

	__egl_image_name_handles_sem_post_sync( display );
	return ret;
}

/* client exiting, remove all image names exported to this client */
void __egl_remove_image_name_handle( egl_display *display )
{
	EGLint i = 0;
	egl_image_name_handles *handles = NULL;
	egl_image_name_entry *entry = NULL;
	PidType pid1, pid2;
	EGLint idx_cl;


	if( display && display->shared_image_names )
	{
		handles = display->shared_image_names;
		entry = handles->entry;
		MALI_DEBUG_ASSERT_POINTER( entry );

		pid1 = _mali_sys_get_pid();

		__egl_image_name_handles_sem_wait_sync( display );

		while ( ( i < IMAGE_HANDLES_ENTRIES_MAX ) && ( EGL_IMAGE_NAME_ENTRY_NEVER_ALLOCATED != entry[i].target_client ) )
		{
			if( EGL_IMAGE_NAME_ENTRY_FREED == entry[i].target_client )
			{
				i++;
				continue;
			}

			idx_cl = HANDLE_EGL_MASK( entry[i].target_client );

			MALI_DEBUG_ASSERT( ( idx_cl >= 0 ) && ( idx_cl < IMAGE_HANDLES_ENTRIES_MAX ), ("invalid target_client entry in shared image names") );

			if( !__egl_get_add_client_name_handles( display ) )
			{
				MALI_DEBUG_PRINT( 2, ("EGL: Unable to get client name handles \n"));
				__egl_image_name_handles_sem_post_sync( display );
				return;
			}

			__egl_client_handles_sem_wait_sync( display );

			/* Verify that the EGLImageName was created to be shared with current client (PID) */
			 pid2 = display->client_handles->entry[idx_cl].pid;

			__egl_client_handles_sem_post_sync( display );

			if ( ( pid1 == pid2 ) && ( display->native_dpy == entry[i].native_dpy ) )
			{
				/* removing display for the egl for one client, any images exported to this clients will be removed from entry  */
				entry[i].target_client = EGL_IMAGE_NAME_ENTRY_FREED;
				--handles->entries;

				/* search for more exported image names to this client, as the client is exiting those image names can be deleted */
			}
			i++;
		}

		image_name_handles_deref( display, handles ); /* mutex is unlocked here */
	}
}


EGLClientBuffer __egl_verify_convert_image_name_to_client_buffer( egl_display *display, EGLImageNameKHR buffer, EGLenum *target_type, EGLint *error )
{
	egl_image_name_handles *handles_img  = NULL;
	egl_image_name_entry *entry = NULL;
	void *data = NULL;
	EGLint idx_cl = 0;
	EGLint idx_img = HANDLE_EGL_MASK( buffer );
	MALI_CHECK( HANDLE_IS_EGL_IMAGE_NAME((u32)buffer), NULL );
	MALI_CHECK( ( idx_img >= 0 ) && ( idx_img < IMAGE_HANDLES_ENTRIES_MAX ), NULL );
	MALI_CHECK( NULL != ( handles_img = __egl_get_add_image_name_handles( display ) ), NULL );
	MALI_CHECK( NULL != ( __egl_get_add_client_name_handles( display ) ), NULL );
	MALI_CHECK( NULL != ( __egl_get_add_image_name_handles( display ) ), NULL );

	entry = handles_img->entry;

	__egl_image_name_handles_sem_wait_sync( display );
	if ( ( EGL_IMAGE_NAME_ENTRY_FREED == entry[idx_img].target_client ) || ( EGL_IMAGE_NAME_ENTRY_NEVER_ALLOCATED == entry[idx_img].target_client ) )
	{
		/* target client does not exist */
		if ( error ) *error = EGL_BAD_ACCESS;
		goto error;
	}

	idx_cl = HANDLE_EGL_MASK( entry[idx_img].target_client );

	if ( !( ( idx_cl >= 0 ) && ( idx_cl < IMAGE_HANDLES_ENTRIES_MAX ) ) )
	{
		if ( error ) *error = EGL_BAD_ACCESS;
		goto error;
	}

	__egl_client_handles_sem_wait_sync( display );

	/* Verify that the EGLImageName was created to be shared with current client (PID) */
	if ( display->client_handles->entry[idx_cl].pid != _mali_sys_get_pid() )
	{
		if ( error ) *error = EGL_BAD_ACCESS;
		__egl_client_handles_sem_post_sync( display );
		goto error;
	}

	__egl_client_handles_sem_post_sync( display );

	switch( entry[idx_img].target_type )
	{
		case EGL_NATIVE_PIXMAP_KHR:
			/* allocated memory will be deleted in __egl_platform_unmap_pixmap while destroying image */
			data = _mali_sys_calloc( 1, sizeof( fbdev_pixmap ) );

			if ( !data )
			{
				if ( error ) *error = EGL_BAD_ALLOC;
				goto error;
			}

			_mali_sys_memcpy( data, (const void *)&entry[idx_img].image_data.pixmap.pixmap, sizeof(fbdev_pixmap) );
			((fbdev_pixmap *)data)->data                = ump_handle_create_from_secure_id( entry[idx_img].image_data.pixmap.ump_unique_secure_id );
			((fbdev_pixmap *)data)->flags              |= ( FBDEV_PIXMAP_SUPPORTS_UMP  | FBDEV_PIXMAP_EGL_MEMORY );

			if ( target_type )	   *target_type   = EGL_NATIVE_PIXMAP_KHR;
			break;
#if defined(EGL_MALI_GLES) || defined(EGL_MALI_VG)
#ifdef EGL_MALI_GLES
		case EGL_GL_TEXTURE_2D_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
		case EGL_GL_RENDERBUFFER_KHR:
#endif /* EGL_MALI_GLES */
#ifdef EGL_MALI_VG
		case EGL_VG_PARENT_IMAGE_KHR:
#endif /* EGL_MALI_VG */
			data = _mali_sys_calloc( 1, sizeof(egl_image_system_buffer_properties) );
			if ( !data )
			{
				if ( error ) *error = EGL_BAD_ALLOC;
				goto error;
			}

			_mali_sys_memcpy( data, (const void *)&entry[idx_img].image_data.buffer_properties, sizeof(egl_image_system_buffer_properties) );

			if ( target_type )	   *target_type   = entry[idx_img].target_type;

			break;
#endif /* EGL_MALI_GLES || EGL_MALI_VG */
		default:
			data = NULL;
	}

error:
	__egl_image_name_handles_sem_post_sync( display );
	return (EGLClientBuffer)data;
}

EGLImageNameKHR __egl_get_image_name( egl_display *display, egl_image *image, EGLClientNameKHR target_client )
{
	int i = 0;
	EGLImageNameKHR ret = EGL_NO_IMAGE_NAME_KHR;
	egl_image_name_handles *handles = NULL;
	egl_image_name_entry *entry = NULL;
	void *image_buffer = image->image_buffer;
	EGLBoolean loop = EGL_TRUE;

	MALI_DEBUG_ASSERT_POINTER ( image_buffer );

	handles = display->shared_image_names;

	__egl_image_name_handles_sem_wait_sync( display );

	entry = handles->entry;
	MALI_DEBUG_ASSERT_POINTER( entry );

	while( loop && ( i < IMAGE_HANDLES_ENTRIES_MAX ) && ( EGL_IMAGE_NAME_ENTRY_NEVER_ALLOCATED != entry[i].target_client ) )
	{
		/* continue to search ahead of already freed entries or if target type does not match the entry */
		if( ( EGL_IMAGE_NAME_ENTRY_FREED == entry[i].target_client ) || ( image->target != entry[i].target_type ) )
		{
			i++;
			continue;
		}

		switch( image->target )
		{
			case EGL_NATIVE_PIXMAP_KHR:
				if( ( ump_secure_id_get( ((fbdev_pixmap *)image_buffer)->data ) == entry[i].image_data.pixmap.ump_unique_secure_id )
						&& ( target_client == entry[i].target_client) && ( display->native_dpy == entry[i].native_dpy )
						&& ( image->target == entry[i].target_type ) )
				{
					/* found the match */
					ret = HANDLE_EGL_IMAGE_NAME( i );
					loop = EGL_FALSE;  /* quit the outer loop */
				}
				break;
#if defined(EGL_MALI_GLES) || defined(EGL_MALI_VG)
#ifdef EGL_MALI_GLES
			case EGL_GL_TEXTURE_2D_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
			case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
			case EGL_GL_RENDERBUFFER_KHR:
#endif /* EGL_MALI_GLES */
#ifdef EGL_MALI_VG
			case EGL_VG_PARENT_IMAGE_KHR:
#endif /* EGL_MALI_VG */
				if( ( ( ((egl_image_system_buffer_properties *)image_buffer)->ump_unique_secure_id ) == entry[i].image_data.buffer_properties.ump_unique_secure_id )
						&& ( target_client == entry[i].target_client) && ( display->native_dpy == entry[i].native_dpy )
						&& ( image->target == entry[i].target_type ) )
				{
					/* found the match */
					ret = HANDLE_EGL_IMAGE_NAME( i );
					loop = EGL_FALSE;  /* quit the outer loop */
				}
				break;
#endif /* EGL_MALI_GLES */
			default:
				loop = EGL_FALSE;/* error, should never be here */
		}
		i++;
	}
	__egl_image_name_handles_sem_post_sync( display );
	return ret;
}

MALI_STATIC egl_image_name_handles* create_open_shared_image_name_handles( egl_display *display)
{
	egl_image_name_handles *mptr = NULL;

	__egl_image_name_handles_sem_wait_sync( display );

	/* Check if shared memory already exists */
	mptr = (egl_image_name_handles *) get_shared_memory( shm_egl_image_names, sizeof( egl_image_name_handles ) );

	if ( mptr )
	{
		++mptr->references;
	}

	__egl_image_name_handles_sem_post_sync( display );

	return mptr;
}

egl_image_name_handles* __egl_get_add_image_name_handles( egl_display *display )
{
	MALI_DEBUG_ASSERT_POINTER( display );

	if ( !display->shared_image_names )
	{
		sem_t *img_name_sem = sem_open( shm_egl_image_names, O_CREAT, S_IRUSR | S_IWUSR, 1);
		if ( SEM_FAILED != img_name_sem )
		{
			display->sem_egl_image_names = img_name_sem;
			display->shared_image_names  = create_open_shared_image_name_handles( display );
			if ( NULL == display->shared_image_names )
			{
				sem_close(img_name_sem);
				display->sem_egl_image_names = NULL;
			}
		}
	}

	return display->shared_image_names;
}

#endif /* EGL_KHR_image_system_ENABLE */
