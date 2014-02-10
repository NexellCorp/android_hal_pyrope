#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2010-2012 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# -*- mode: makefile -*-

# Specify the path/name of the instrumented plug-in, without extension.
# This will be loaded if any instrumented features are enabled.
# 
MALI_INSTRUMENTED_PLUGIN=libmaliinstr

# Enable/disable timeline profiling.
# Requires an instrumented plug-in (see MALI_INSTRUMENTED_PLUGIN).
# 
MALI_TIMELINE_PROFILING_ENABLED=0

# Enable/disable function call profiling.
# 
MALI_TIMELINE_PROFILING_FUNC_ENABLED=0

# Enable/disable software counter gathering with DS-5 Streamline.
# 
MALI_SW_COUNTERS_ENABLED = 0

# Enable/disable framebuffer dumping with DS-5 Streamline.
# 
MALI_FRAMEBUFFER_DUMP_ENABLED = 0

# The Mali driver will at process startup check the following file specified in
# MALI_EXTRA_ENVIRONMENT_FILE and load environment variables from this file.
# Each line in this file should start with a process name (found in /proc/<pid>/cmdline),
# and the rest of the line should be the desired environment variables for the specified
# process.
#
# This is useful for Android, where you can not specify environment variables when you
# launch an application.
#
# For example:
# The following line would set the environment variable MALI_RECORD_ALL to TRUE and
# MALI_DUMP_FILE to dump.ds2 for processes named com.mycompany.myapp:
# com.mycompany.myapp MALI_RECORD_ALL=TRUE MALI_DUMP_FILE=TRUE
# 
# Note;
# Space is not supported when specifying variable names or values, even not when quoted.
# 
# MALI_EXTRA_ENVIRONMENT_FILE=/tmp/malienv.txt
