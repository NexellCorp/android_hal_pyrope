#!/usr/bin/env python
# -----------------------------------------------------------------------------
#  This confidential and proprietary software may be used only as authorized
#  by a licensing agreement from ARM Limited.
#          (C) COPYRIGHT 2010, 2012 ARM Limited , ALL RIGHTS RESERVED
#  The entire notice above must be reproduced on all authorized copies and
#  copies may only be made to the extent permitted by a licensing agreement
#  from ARM Limited.
# -----------------------------------------------------------------------------

"""
This script is used to stitch up and include path mangling which is needed when
importing a new baseline release from the QA repo into the Midgard DDK 
directory structure. Just run the command from the UTF directory with no
command line arguments.
"""

import re
import os
import os.path

import posixpath

def fixup_dir( rDir ):

    if( rDir.endswith(".svn") or rDir.endswith("_svn") ):
        return

    for rPath in os.listdir( rDir ):

        # Create the full path
        rPath = os.path.join( rDir, rPath )

        # If a directory then recurse
        if( os.path.isdir( rPath ) ):
            fixup_dir( rPath )

        # If a file then parse it
        elif( os.path.isfile( rPath ) ):
            if( (False == rPath.endswith(".h")) and (False == rPath.endswith(".c")) ):
                continue

            with open( rPath, "r+b" ) as sFile:
                yFixup = False
                rData = sFile.read()
                
                rPath = rPath.replace("\\", "/")

                # Fixup standard includes to other modules
                rDataOld = rData
                rData = rData.replace( '#include "tpi/include/cstd/arm_cstd.h"',  '#include "malisw/mali_malisw.h"' )
                if (rDataOld != rData) and (not yFixup):
                    yFixup = True
                    print "Fixing up " + rPath

                sFile.seek( 0 )
                sFile.truncate( 0 )
                sFile.write( rData )

if __name__ == "__main__":

    fixup_dir( "./" )
