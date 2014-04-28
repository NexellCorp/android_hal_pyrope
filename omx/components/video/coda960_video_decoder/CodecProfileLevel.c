#include <OMX_Video.h>

#include "CodecProfileLevel.h"

//
//  H.264(AVC) Supported Profile & Level
//

const OMX_VIDEO_AVCPROFILETYPE gstDecSupportedAVCProfiles[MAX_DEC_SUPPORTED_AVC_PROFILES] =
{
    OMX_VIDEO_AVCProfileBaseline,   /**< Baseline profile */
    OMX_VIDEO_AVCProfileMain    ,   /**< Main profile */
    OMX_VIDEO_AVCProfileHigh    ,   /**< High profile */
};

const OMX_VIDEO_AVCLEVELTYPE gstDecSupportedAVCLevels[MAX_DEC_SUPPORTED_AVC_LEVELS] =
{
    OMX_VIDEO_AVCLevel1 ,	/**< Level 1 */
    OMX_VIDEO_AVCLevel1b,	/**< Level 1b */
    OMX_VIDEO_AVCLevel11,	/**< Level 1.1 */
    OMX_VIDEO_AVCLevel12,	/**< Level 1.2 */
    OMX_VIDEO_AVCLevel13,	/**< Level 1.3 */
    OMX_VIDEO_AVCLevel2 ,	/**< Level 2 */
    OMX_VIDEO_AVCLevel21,	/**< Level 2.1 */
    OMX_VIDEO_AVCLevel22,	/**< Level 2.2 */
    OMX_VIDEO_AVCLevel3 ,	/**< Level 3 */
    OMX_VIDEO_AVCLevel31,	/**< Level 3.1 */
    OMX_VIDEO_AVCLevel32,	/**< Level 3.2 */
    OMX_VIDEO_AVCLevel4 ,	/**< Level 4 */
    OMX_VIDEO_AVCLevel41,	/**< Level 4.1 */
    OMX_VIDEO_AVCLevel42,	/**< Level 4.2 */
};


//
//  Mpeg4 Supported Profile & Level
//
const OMX_VIDEO_MPEG4PROFILETYPE gstDecSupportedMPEG4Profiles[MAX_DEC_SUPPORTED_MPEG4_PROFILES] = 
{
    OMX_VIDEO_MPEG4ProfileSimple,
    OMX_VIDEO_MPEG4ProfileAdvancedSimple,
};

const OMX_VIDEO_MPEG4LEVELTYPE gstDecSupportedMPEG4Levels[MAX_DEC_SUPPORTED_MPEG4_LEVELS] = 
{
    OMX_VIDEO_MPEG4Level0 ,
    OMX_VIDEO_MPEG4Level0b,
    OMX_VIDEO_MPEG4Level1 ,
    OMX_VIDEO_MPEG4Level2 ,
    OMX_VIDEO_MPEG4Level3 ,
    OMX_VIDEO_MPEG4Level4 ,
    OMX_VIDEO_MPEG4Level4a,
    OMX_VIDEO_MPEG4Level5 ,
};


//
//  Mpeg2 Supported Profile & Level
//
const OMX_VIDEO_MPEG2PROFILETYPE gstDecSupportedMPEG2Profiles[MAX_DEC_SUPPORTED_MPEG2_PROFILES] = 
{
    OMX_VIDEO_MPEG2ProfileSimple,
    OMX_VIDEO_MPEG2ProfileMain,
    OMX_VIDEO_MPEG2ProfileHigh,
};

const OMX_VIDEO_MPEG2LEVELTYPE gstDecSupportedMPEG2Levels[MAX_DEC_SUPPORTED_MPEG2_LEVELS] = 
{
    OMX_VIDEO_MPEG2LevelLL,
    OMX_VIDEO_MPEG2LevelML,
    OMX_VIDEO_MPEG2LevelHL,
};

