/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef COMPILER_VERSION_H
#define COMPILER_VERSION_H


#define ONLINE_COMPILER 1
#define OFFLINE_COMPILER 2

#if defined(USING_MALI200) || defined(USING_MALI400) || defined(USING_MALI450)
#define COMPILER_CONFIGURATION ONLINE_COMPILER
#define ENABLE_PILOT_SHADER_SUPPORT
#else
#define COMPILER_CONFIGURATION OFFLINE_COMPILER
#endif

#if defined(USING_MALI450) || defined(USING_MALI450_OFFLINE)
#define ENABLE_MALI450_SUPPORT 
#endif



#define COMPILER_REVISION_NUMBER "96995"

#define OFFLINE_COMPILER_VERSION "2.2"

#define ONLINE_COMPILER_VERSION "r2p1_05rel0"


#if COMPILER_CONFIGURATION == OFFLINE_COMPILER
#define COMPILER_VERSION OFFLINE_COMPILER_VERSION
#endif

#if COMPILER_CONFIGURATION == ONLINE_COMPILER
#define COMPILER_VERSION ONLINE_COMPILER_VERSION
#endif

#define COMPILER_VERSION_STRING COMPILER_VERSION " [Revision " COMPILER_REVISION_NUMBER "]"








#endif /* COMPILER_VERSION_H */
