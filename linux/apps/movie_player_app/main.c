#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>	//	getopt & optarg
#include <pthread.h>
#include <signal.h>

#include <NX_MoviePlay.h>

struct AppData{
	MP_HANDLE hPlayer;
};

typedef struct AppData AppData;

static void callback( void *privateDesc, unsigned int message, unsigned int param1, unsigned int param2 )
{
	if( message == CALLBACK_MSG_EOS )
	{
		printf("App : callback(privateDesc = %p)\n", privateDesc);
		if( privateDesc )
		{
			AppData *appData = (AppData*)privateDesc;
			if( appData->hPlayer )
			{
				printf("NX_MPStop ++\n");
				NX_MPStop(appData->hPlayer);
				printf("NX_MPStop --\n");
			}
		}
		printf("CALLBACK_MSG_EOS\n");
	}
	else if( message == CALLBACK_MSG_PLAY_ERR )
	{
		printf("Cannot Play Contents\n");

	}
}

#define	SHELL_MAX_ARG	32
#define	SHELL_MAX_STR	1024
static int GetArgument( char *pSrc, char arg[][SHELL_MAX_STR] )
{
	int	i, j;

	// Reset all arguments
	for( i=0 ; i<SHELL_MAX_ARG ; i++ )
	{
		arg[i][0] = 0;
	}

	for( i=0 ; i<SHELL_MAX_ARG ; i++ )
	{
		// Remove space char
		while( *pSrc == ' ' )
			pSrc++;
		// check end of string.
		if( *pSrc == 0 || *pSrc == '\n' )
			break;

		j=0;
		while( (*pSrc != ' ') && (*pSrc != 0) && *pSrc != '\n' )
		{
			arg[i][j] = *pSrc++;
			j++;
			if( j > (SHELL_MAX_STR-1) )
				j = SHELL_MAX_STR-1;
		}
		arg[i][j] = 0;
	}
	return i;
}


static void shell_help( void )
{
	//               1         2         3         4         5         6         7
	//      12345678901234567890123456789012345678901234567890123456789012345678901234567890
	printf("\n");
	printf("===============================================================================\n");
	printf("                       Play Control Commands\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("    open( or o) [Moudle] [Port] [filename] : open player for specific file\n");
	printf("    close                                  : close player\n");
	printf("    info                                   : show file information\n");
	printf("    play                                   : play\n");
	printf("    pause                                  : pause playing\n");
	printf("    stop                                   : stop playing\n");
	printf("    seek (or s) [milli seconds]            : seek\n");
	printf("    status (or st)                         : display current player status\n");
	printf("    p                                      : toggle play & pause\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("                        Other Control Commands\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("    pos [module] [port] [x] [y] [w] [h]    : change play position\n");
	printf("    vol [gain(%%), 0~100]                  : change volume(0:mute)\n");
	printf("===============================================================================\n\n");
}


//static int shell_main(const char *scan_path, const char *out_file)
static int shell_main( const char *uri )
{
	static	char cmdstring[SHELL_MAX_ARG * SHELL_MAX_STR];
	static 	char cmd[SHELL_MAX_ARG][SHELL_MAX_STR];
	int idx = 0;
	int volume = 50;
	int cmdCnt;
	int dspPort = DISPLAY_PORT_LCD;						//DISPLAY_PORT_LCD:(0) ,DISPLAY_PORT_HDMI:(1) 
	int dspModule = DISPLAY_MODULE_MLC0;				//DISPLAY_MODULE_MLC0:(0) ,DISPLAY_MODULE_MLC1:(1) 
	MP_RESULT mpResult = ERROR_NONE;

	//	Player Specific Parameters

	MP_HANDLE hPlayer = NULL;
	AppData appData;
	int isPaused = 0;

	shell_help();
	while(1)
	{
		printf("File Player> ");

		memset(cmd, 0, sizeof(cmd));
		fgets( cmdstring, sizeof(cmdstring), stdin );
		cmdCnt = GetArgument( cmdstring, cmd );

		//
		//	Play Control Commands
		//
		if( (0 == strcasecmp( cmd[0], "open" )) | (0 == strcasecmp( cmd[0], "o" )) )
		{
			char *openName;
			if( cmdCnt > 3)
			{
				dspModule = atoi(cmd[1]);
				dspPort = atoi(cmd[2]);
				openName = cmd[3];
			}
			else if( cmdCnt > 2 )
			{
				dspModule = atoi(cmd[1]);
				dspPort = atoi(cmd[2]);
				if( uri )
					openName = uri;
			}
			else if( cmdCnt > 1 )
			{
				dspModule = atoi(cmd[1]);
				if( uri )
					openName = uri;
			}
			else
			{
				if( uri == NULL )
				{
					printf("Error : Invalid argument !!!, Usage : open [filename]\n");
					continue;
				}
				else
				{
					openName = uri;
				}
			}
			if( hPlayer )
			{
				NX_MPClose( hPlayer );
				hPlayer = NULL;
			}

			memset(&appData, 0, sizeof(appData));
			if( ERROR_NONE != (mpResult = NX_MPOpen( &hPlayer, openName, volume, dspModule, dspPort, &callback, &appData )) )
			{
				printf("Error : NX_MPOpen Failed!!!(uri=%s, %d)\n", openName, mpResult);
				continue;
			}
			isPaused = 0;
			appData.hPlayer = hPlayer;

			if( dspModule == 1 && dspPort == 1 )
			{
				if( ERROR_NONE != (mpResult = NX_MPSetDspPosition( hPlayer, dspModule, dspPort, 0, 0, 1920, 1080 ) ) )
				{
					printf("Error : NX_MPSetDspPosition Failed!!!(%d)\n", mpResult);
				}
			}

		}
		else if( 0 == strcasecmp( cmd[0], "close" ) )
		{
			if( hPlayer )
			{
				NX_MPClose( hPlayer );
				isPaused = 0;
				hPlayer = NULL;
			}
			else
			{
				printf("Already closed or not Opened!!!");
			}
		}
		else if( 0 == strcasecmp( cmd[0], "info" ) )
		{
			//	N/A
		}
		else if( 0 == strcasecmp( cmd[0], "play" ) )
		{
			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPPlay( hPlayer, 1.0 )) )
				{
					printf("Error : NX_MPPlay Failed!!!(%d)\n", mpResult);
					continue;
				}
				isPaused = 0;
			}
			else
			{
				printf("Error : not Opened!!!");
			}
		}
		else if( 0 == strcasecmp( cmd[0], "pause" ) )
		{
			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPPause( hPlayer )) )
				{
					printf("Error : NX_MPPause Failed!!!(%d)\n", mpResult);
					continue;
				}
				isPaused = 1;
			}
			else
			{
				printf("Error : not Opened!!!");
			}
		}
		else if( 0 == strcasecmp( cmd[0], "p" ) )
		{
			if( hPlayer )
			{
				if( isPaused )
				{
					if( ERROR_NONE != (mpResult = NX_MPPlay( hPlayer, 1.0 )) )
					{
						printf("Error : NX_MPPlay Failed!!!(%d)\n", mpResult);
						continue;
					}
					isPaused = 0;
					printf("Play~~~\n");
				}
				else
				{
				if( ERROR_NONE != (mpResult = NX_MPPause( hPlayer )) )
					{
						printf("Error : NX_MPPause Failed!!!(%d)\n", mpResult);
						continue;
					}
					isPaused = 1;
					printf("Paused~~~\n");
				}
			}
		}
		else if( 0 == strcasecmp( cmd[0], "stop" ) )
		{
			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPStop( hPlayer )) )
				{
					printf("Error : NX_MPStop Failed!!!(%d)\n", mpResult);
				}
			}
			else
			{
				printf("Error : not Opened!!!");
			}
		}
		else if( (0 == strcasecmp( cmd[0], "seek" )) || (0 == strcasecmp( cmd[0], "s" )) )
		{
			int seekTime;
			if(cmdCnt<2)
			{
				printf("Error : Invalid argument !!!, Usage : seek (or s) [milli seconds]\n");
				continue;
			}

			seekTime = atoi( cmd[1] );

			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPSeek( hPlayer, seekTime )) )
				{
					printf("Error : NX_MPSeek Failed!!!(%d)\n", mpResult);
				}
			}
			else
			{
				printf("Error : not Opened!!!");
			}
		}
		else if( (0 == strcasecmp( cmd[0], "status" )) || (0 == strcasecmp( cmd[0], "st" )) )
		{
			if( hPlayer )
			{
				unsigned int duration, position;
				NX_MPGetCurDuration(hPlayer, &duration);
				NX_MPGetCurPosition(hPlayer, &position);
				printf("Postion : %d / %d msec\n", position, duration);
			}
			else
			{
				printf("\nPlayer does not initialized!!\n");
			}
		}

		//
		//	Other Control Commands
		//
		else if( 0 == strcasecmp( cmd[0], "pos" ) )
		{
			int x, y, w, h;

			if( cmdCnt < 7)
			{
				printf("\nError : Invalid arguments, Usage : pos [module] [port] [x] [y] [w] [h]\n");
				continue;
			}
			dspModule 	= atoi(cmd[1]);
			dspPort 	= atoi(cmd[2]);
			x 			= atoi(cmd[3]);
			y 			= atoi(cmd[4]);
			w 			= atoi(cmd[5]);
			h 			= atoi(cmd[6]);

			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPSetDspPosition( hPlayer, dspModule, dspPort, x, y, w, h ) ) )
				{
					printf("Error : NX_MPSetDspPosition Failed!!!(%d)\n", mpResult);
				}
			}
		}
		else if( 0 == strcasecmp( cmd[0], "vol" ) )
		{
			int vol;
			if(cmdCnt<2)
			{
				printf("Error : Invalid argument !!!, Usage : vol [volume]\n");
				continue;
			}

			vol = atoi( cmd[1] ) % 101;

			if( vol > 100 )
			{
				vol = 100;
			}
			else if( vol < 0 )
			{
				vol = 0;
			}

			if( hPlayer )
			{
				if( ERROR_NONE != (mpResult = NX_MPSetVolume( hPlayer, vol ) ) )
				{
					printf("Error : NX_MPSeek Failed!!!(%d)\n", mpResult);
				}
				//	Save to Application Volume
				volume = vol;
			}
		}

		//
		//	Help & Exit
		//
		else if( (0 == strcasecmp( cmd[0], "exit" )) || (0 == strcasecmp( cmd[0], "q" )) )
		{
			printf("Exit ByeBye ~~~\n");
			break;
		}
		else if( (0 == strcasecmp( cmd[0], "help" )) || (0 == strcasecmp( cmd[0], "h" )) || (0 == strcasecmp( cmd[0], "?" )) )
		{
			shell_help();
		}
		else
		{
			if( cmdCnt != 0 )
				printf("Unknown command : %s, cmdCnt=%d, %d\n", cmd[0], cmdCnt, strlen(cmd[0]) );
		}
	}

	if( hPlayer )
	{
		NX_MPClose( hPlayer );
	}

	return 0;
}

void print_usage(const char *appName)
{
	printf( "usage: %s [options]\n", appName );
	printf( "    -f [file name]     : file name\n" );
	printf( "    -s                 : shell command mode\n" );
}


int main( int argc, char *argv[] )
{	
	int opt = 0, count = 0;
	unsigned int duration = 0, position = 0;
	int level = 1, shell_mode = 0;
	static char uri[2048];
	MP_HANDLE handle = NULL;
	int volume = 1;
	int dspPort = DISPLAY_PORT_LCD;						//DISPLAY_PORT_LCD:(0) ,DISPLAY_PORT_HDMI:(1) 
	int dspModule = DISPLAY_MODULE_MLC0;				//DISPLAY_MODULE_MLC0:(0) ,DISPLAY_MODULE_MLC1:(1) 

	if( 2>argc )
	{
		print_usage( argv[0] );
		return 0;
	}

	while( -1 != (opt=getopt(argc, argv, "hsf:")))
	{
		switch( opt ){
			case 'h':	print_usage(argv[0]);		return 0;
			case 'f':	strcpy(uri, optarg);		break;
			case 's':	shell_mode = 1;				break;
			default:								break;
		}
	}

	if( shell_mode )
	{
		return shell_main( uri );
	}

	NX_MPOpen( &handle, uri, 100, 0, 0, &callback, NULL);
	
	printf("handle_s1 = %p\n",handle);

	if( handle )
	{

		printf("Init Play Done\n");

		if( NX_MPPlay( handle, 1 ) != 0 )
		{
			printf("NX_MPPlay failed\n");
		}
		printf("Start Play Done\n");
	}
	count = 0;

	while(1)
	{
		usleep(300000);

		NX_MPGetCurDuration(handle, &duration);
		NX_MPGetCurPosition(handle, &position);

		printf("Postion : %d / %d msec\n", position, duration);

		count ++;

		if( count == 50 )
		{
//			NX_MPSeek( handle, 90000 );
//			NX_MPPause( handle);
//			NX_MPSetVolume(handle, 0);
		}
	}
}