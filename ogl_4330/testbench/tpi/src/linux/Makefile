#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2011-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

ifneq ($(TPI_OS),) 
	TPI_SRCS += $(TPI_SRC_DIR)/$(TPI_OS)/mali_tpi_$(TPI_OS).c

    TPI_COMMON_BACKEND = dummy fbdev
    
    ifneq ($(TPI_BACKEND),) 
        TPI_SRCS += $(TPI_SRC_DIR)/$(TPI_OS)/mali_tpi_$(TPI_OS)_egl_$(TPI_BACKEND).c
        
#    	ifneq ($(filter $(TPI_BACKEND), $(TPI_COMMON_BACKEND)),)
#        	TPI_SRCS += $(TPI_SRC_DIR)/$(TPI_OS)/mali_tpi_$(TPI_OS)_egl_common.c
#        endif
    endif
endif
		


