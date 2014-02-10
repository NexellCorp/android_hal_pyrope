/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file vr_osk_locks.c
 * Implemenation of the OS abstraction layer for the kernel device driver
 */

#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>

#include <linux/slab.h>

#include "vr_osk.h"
#include "vr_kernel_common.h"

/* These are all the locks we implement: */
typedef enum
{
	_VR_OSK_INTERNAL_LOCKTYPE_SPIN,            /* Mutex, implicitly non-interruptable, use spin_lock/spin_unlock */
	_VR_OSK_INTERNAL_LOCKTYPE_SPIN_IRQ,        /* Mutex, IRQ version of spinlock, use spin_lock_irqsave/spin_unlock_irqrestore */
	_VR_OSK_INTERNAL_LOCKTYPE_MUTEX,           /* Interruptable, use mutex_unlock()/down_interruptable() */
	_VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT,    /* Non-Interruptable, use mutex_unlock()/down() */
	_VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT_RW, /* Non-interruptable, Reader/Writer, use {mutex_unlock,down}{read,write}() */

	/* Linux supports, but we do not support:
	 * Non-Interruptable Reader/Writer spinlock mutexes - RW optimization will be switched off
	 */

	/* Linux does not support:
	 * One-locks, of any sort - no optimization for this fact will be made.
	 */

} _vr_osk_internal_locktype;

struct _vr_osk_lock_t_struct
{
    _vr_osk_internal_locktype type;
	unsigned long flags;
    union
    {
        spinlock_t spinlock;
	struct mutex mutex;
        struct rw_semaphore rw_sema;
    } obj;
	VR_DEBUG_CODE(
				  /** original flags for debug checking */
				  _vr_osk_lock_flags_t orig_flags;

				  /* id of the thread currently holding this lock, 0 if no
				   * threads hold it. */
				  u32 owner;
				  /* number of owners this lock currently has (can be > 1 if
				   * taken in R/O mode. */
				  u32 nOwners;
				  /* what mode the lock was taken in */
				  _vr_osk_lock_mode_t mode;
	); /* VR_DEBUG_CODE */
};

_vr_osk_lock_t *_vr_osk_lock_init( _vr_osk_lock_flags_t flags, u32 initial, u32 order )
{
    _vr_osk_lock_t *lock = NULL;

	/* Validate parameters: */
	/* Flags acceptable */
	VR_DEBUG_ASSERT( 0 == ( flags & ~(_VR_OSK_LOCKFLAG_SPINLOCK
                                      | _VR_OSK_LOCKFLAG_SPINLOCK_IRQ
                                      | _VR_OSK_LOCKFLAG_NONINTERRUPTABLE
                                      | _VR_OSK_LOCKFLAG_READERWRITER
                                      | _VR_OSK_LOCKFLAG_ORDERED
                                      | _VR_OSK_LOCKFLAG_ONELOCK )) );
	/* Spinlocks are always non-interruptable */
	VR_DEBUG_ASSERT( (((flags & _VR_OSK_LOCKFLAG_SPINLOCK) || (flags & _VR_OSK_LOCKFLAG_SPINLOCK_IRQ)) && (flags & _VR_OSK_LOCKFLAG_NONINTERRUPTABLE))
					 || !(flags & _VR_OSK_LOCKFLAG_SPINLOCK));
	/* Parameter initial SBZ - for future expansion */
	VR_DEBUG_ASSERT( 0 == initial );

	lock = kmalloc(sizeof(_vr_osk_lock_t), GFP_KERNEL);

	if ( NULL == lock )
	{
		return lock;
	}

	/* Determine type of mutex: */
    /* defaults to interruptable mutex if no flags are specified */

	if ( (flags & _VR_OSK_LOCKFLAG_SPINLOCK) )
	{
		/* Non-interruptable Spinlocks override all others */
		lock->type = _VR_OSK_INTERNAL_LOCKTYPE_SPIN;
		spin_lock_init( &lock->obj.spinlock );
	}
	else if ( (flags & _VR_OSK_LOCKFLAG_SPINLOCK_IRQ ) )
	{
		lock->type = _VR_OSK_INTERNAL_LOCKTYPE_SPIN_IRQ;
		lock->flags = 0;
		spin_lock_init( &lock->obj.spinlock );
	}
	else if ( (flags & _VR_OSK_LOCKFLAG_NONINTERRUPTABLE)
			  && (flags & _VR_OSK_LOCKFLAG_READERWRITER) )
	{
		lock->type = _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT_RW;
		init_rwsem( &lock->obj.rw_sema );
	}
	else
	{
		/* Usual mutex types */
		if ( (flags & _VR_OSK_LOCKFLAG_NONINTERRUPTABLE) )
		{
			lock->type = _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT;
		}
		else
		{
			lock->type = _VR_OSK_INTERNAL_LOCKTYPE_MUTEX;
		}

		/* Initially unlocked */
		mutex_init(&lock->obj.mutex);
	}

#ifdef DEBUG
	/* Debug tracking of flags */
	lock->orig_flags = flags;

	/* Debug tracking of lock owner */
	lock->owner = 0;
	lock->nOwners = 0;
#endif /* DEBUG */

    return lock;
}

#ifdef DEBUG
u32 _vr_osk_lock_get_owner( _vr_osk_lock_t *lock )
{
	return lock->owner;
}

u32 _vr_osk_lock_get_number_owners( _vr_osk_lock_t *lock )
{
	return lock->nOwners;
}

u32 _vr_osk_lock_get_mode( _vr_osk_lock_t *lock )
{
	return lock->mode;
}
#endif /* DEBUG */

_vr_osk_errcode_t _vr_osk_lock_wait( _vr_osk_lock_t *lock, _vr_osk_lock_mode_t mode)
{
    _vr_osk_errcode_t err = _VR_OSK_ERR_OK;

	/* Parameter validation */
	VR_DEBUG_ASSERT_POINTER( lock );

	VR_DEBUG_ASSERT( _VR_OSK_LOCKMODE_RW == mode
					 || _VR_OSK_LOCKMODE_RO == mode );

	/* Only allow RO locks when the initial object was a Reader/Writer lock
	 * Since information is lost on the internal locktype, we use the original
	 * information, which is only stored when built for DEBUG */
	VR_DEBUG_ASSERT( _VR_OSK_LOCKMODE_RW == mode
					 || (_VR_OSK_LOCKMODE_RO == mode && (_VR_OSK_LOCKFLAG_READERWRITER & lock->orig_flags)) );

	switch ( lock->type )
	{
	case _VR_OSK_INTERNAL_LOCKTYPE_SPIN:
		spin_lock(&lock->obj.spinlock);
		break;
	case _VR_OSK_INTERNAL_LOCKTYPE_SPIN_IRQ:
		{
			unsigned long tmp_flags;
			spin_lock_irqsave(&lock->obj.spinlock, tmp_flags);
			lock->flags = tmp_flags;
		}
		break;

	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX:
		if (mutex_lock_interruptible(&lock->obj.mutex))
		{
			VR_PRINT_ERROR(("Can not lock mutex\n"));
			err = _VR_OSK_ERR_RESTARTSYSCALL;
		}
		break;

	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT:
		mutex_lock(&lock->obj.mutex);
		break;

	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT_RW:
		if (mode == _VR_OSK_LOCKMODE_RO)
        {
            down_read(&lock->obj.rw_sema);
        }
        else
        {
            down_write(&lock->obj.rw_sema);
        }
		break;

	default:
		/* Reaching here indicates a programming error, so you will not get here
		 * on non-DEBUG builds */
		VR_DEBUG_PRINT_ERROR( ("Invalid internal lock type: %.8X", lock->type ) );
		break;
	}

#ifdef DEBUG
	/* This thread is now the owner of this lock */
	if (_VR_OSK_ERR_OK == err)
	{
		if (mode == _VR_OSK_LOCKMODE_RW)
		{
			/*VR_DEBUG_ASSERT(0 == lock->owner);*/
			if (0 != lock->owner)
			{
				printk(KERN_ERR "%d: ERROR: Lock %p already has owner %d\n", _vr_osk_get_tid(), lock, lock->owner);
				dump_stack();
			}
			lock->owner = _vr_osk_get_tid();
			lock->mode = mode;
			++lock->nOwners;
		}
		else /* mode == _VR_OSK_LOCKMODE_RO */
		{
			lock->owner |= _vr_osk_get_tid();
			lock->mode = mode;
			++lock->nOwners;
		}
	}
#endif

    return err;
}

void _vr_osk_lock_signal( _vr_osk_lock_t *lock, _vr_osk_lock_mode_t mode )
{
	/* Parameter validation */
	VR_DEBUG_ASSERT_POINTER( lock );

	VR_DEBUG_ASSERT( _VR_OSK_LOCKMODE_RW == mode
					 || _VR_OSK_LOCKMODE_RO == mode );

	/* Only allow RO locks when the initial object was a Reader/Writer lock
	 * Since information is lost on the internal locktype, we use the original
	 * information, which is only stored when built for DEBUG */
	VR_DEBUG_ASSERT( _VR_OSK_LOCKMODE_RW == mode
					 || (_VR_OSK_LOCKMODE_RO == mode && (_VR_OSK_LOCKFLAG_READERWRITER & lock->orig_flags)) );

#ifdef DEBUG
	/* make sure the thread releasing the lock actually was the owner */
	if (mode == _VR_OSK_LOCKMODE_RW)
	{
		/*VR_DEBUG_ASSERT(_vr_osk_get_tid() == lock->owner);*/
		if (_vr_osk_get_tid() != lock->owner)
		{
			printk(KERN_ERR "%d: ERROR: Lock %p owner was %d\n", _vr_osk_get_tid(), lock, lock->owner);
			dump_stack();
		}
		/* This lock now has no owner */
		lock->owner = 0;
		--lock->nOwners;
	}
	else /* mode == _VR_OSK_LOCKMODE_RO */
	{
		if ((_vr_osk_get_tid() & lock->owner) != _vr_osk_get_tid())
		{
			printk(KERN_ERR "%d: ERROR: Not an owner of %p lock.\n", _vr_osk_get_tid(), lock);
			dump_stack();
		}

		/* if this is the last thread holding this lock in R/O mode, set owner
		 * back to 0 */
		if (0 == --lock->nOwners)
		{
			lock->owner = 0;
		}
	}
#endif /* DEBUG */

	switch ( lock->type )
	{
	case _VR_OSK_INTERNAL_LOCKTYPE_SPIN:
		spin_unlock(&lock->obj.spinlock);
		break;
	case _VR_OSK_INTERNAL_LOCKTYPE_SPIN_IRQ:
		spin_unlock_irqrestore(&lock->obj.spinlock, lock->flags);
		break;

	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX:
		/* FALLTHROUGH */
	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT:
		mutex_unlock(&lock->obj.mutex);
		break;

	case _VR_OSK_INTERNAL_LOCKTYPE_MUTEX_NONINT_RW:
		if (mode == _VR_OSK_LOCKMODE_RO)
        {
            up_read(&lock->obj.rw_sema);
        }
        else
        {
            up_write(&lock->obj.rw_sema);
        }
		break;

	default:
		/* Reaching here indicates a programming error, so you will not get here
		 * on non-DEBUG builds */
		VR_DEBUG_PRINT_ERROR( ("Invalid internal lock type: %.8X", lock->type ) );
		break;
	}
}

void _vr_osk_lock_term( _vr_osk_lock_t *lock )
{
	/* Parameter validation  */
	VR_DEBUG_ASSERT_POINTER( lock );

	/* Linux requires no explicit termination of spinlocks, semaphores, or rw_semaphores */
    kfree(lock);
}
