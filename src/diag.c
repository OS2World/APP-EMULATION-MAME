#define INCL_PM
#include <stdio.h>
#include <os2.h>
#include "tmr0_ioc.h"
#include "dive.h"
#include "joyos2.h"

extern FILE *fp;

#define FOURCC_R565 0x35363552ul
#define FOURCC_LUT8 0x3854554cul
#define FOURCC_SCRN 0
#define WM_VRNENABLE 0x7f

/*extern "C" */ULONG APIENTRY WinSetVisibleRegionNotify( HWND win, BOOL bool );
/*extern "C" */ULONG APIENTRY WinQueryVisibleRegion( HWND win, HRGN hrgn );

ULONG blitdepth;

MRESULT EXPENTRY MameDiag( HWND win, ULONG msg, MPARAM mp1, MPARAM mp2 ) {
	ULONG err;
	static HDIVE diveinst;
	static PVOID framebuffer = NULL;
	static PBYTE imagebuf = NULL;
	static ULONG linebytes, linetot, i, j, bufnum;
	RGNRECT rgnCtl;
	HPS hps;
	RECTL rectl;
	RECTL rcls[50];
	HRGN hrgn;
	SWP swp;
	POINTL pointl;
	static SETUP_BLITTER BlSet;
	DIVE_CAPS cap;
	char buffer[512];

	switch ( msg ) {
		case WM_CREATE:
			fprintf( fp, "Opening a DIVE instance... " );
			fflush(fp);
			err = DiveOpen( &diveinst, FALSE, framebuffer );
			if ( err ) {
				fprintf( fp, "failed!  DiveOpen rc = %ld.\n",err );
				fflush(fp);
				return MRFROMSHORT(-1);
			}
			fprintf( fp, "succeeded.\nQuerying DIVE capabilities...\n" );
			fflush(fp);

			cap.ulStructLen = sizeof( DIVE_CAPS );
			cap.ulFormatLength = 512;
			cap.pFormatData = buffer;
			cap.ulPlaneCount = 0;
			DiveQueryCaps( &cap, DIVE_BUFFER_SCREEN );

			// This wonderful little statement here helped me determine that
			// the color encoding of the 32bpp mode for my Matrox card
			// was BGR4 which was not a valid output format!  Very useful
			// diagnostic.
			fprintf( fp, "  Dive capabilities-\n  Number of planes: %ld\n  Screen direct access: %d\n  Bank switched operations required: %d\n  Bits per pixel: %ld\n  Screen width: %ld\n  Screen height: %ld\n  Screen scan line length: %ld\n  Color encoding: %c%c%c%c\n  VRAM aperture size: %ld\n  Input formats: %ld\n  Output formats: %ld\n  Formats: ",
			    cap.ulPlaneCount, cap.fScreenDirect, cap.fBankSwitched, cap.ulDepth, cap. ulHorizontalResolution,
			    cap.ulVerticalResolution, cap.ulScanLineBytes, cap.fccColorEncoding&0xff,
			    (cap.fccColorEncoding>>8)&0xff, (cap.fccColorEncoding>>16)&0xff, (cap.fccColorEncoding>>24)&0xff,
			    cap.ulApertureSize, cap.ulInputFormats, cap.ulOutputFormats );


			fwrite( cap.pFormatData, cap.ulFormatLength, 1, fp );
			fprintf( fp, "\n" );
			fflush( fp );

			fprintf( fp, "Allocating a 640x480 %d bit image in DIVE memory... ", ((blitdepth==FOURCC_R565)?16:8) );
			fflush(fp);
			err = DiveAllocImageBuffer( diveinst, &bufnum, FOURCC_R565,
			   640, 480, 1280/((blitdepth==FOURCC_R565)?1:2), NULL );
			if ( err ) {
				fprintf( fp, "failed! DiveAllocImageBuffer rc = %ld.\n",err );
				fflush(fp);
				DiveClose( diveinst );
				return MRFROMSHORT(-1);
			}
			fprintf( fp, "done.\nBeginning to access the image buffer..." );
			fflush(fp);
			err = DiveBeginImageBufferAccess( diveinst, bufnum, &imagebuf,
			    &linebytes, &linetot );
			if ( err ) {
				fprintf( fp, "failed! DiveBeginImageBufferAccess rc = %ld.\n",err );
				fflush(fp);
				DiveFreeImageBuffer( diveinst, bufnum );
				DiveClose( diveinst );
				return MRFROMSHORT(-1);
			}
			if ( blitdepth == FOURCC_R565 ) {
			{ unsigned short *tmp; 
			tmp = (unsigned short *)imagebuf;
			for ( i = 0; i < linetot; ++i ) {
				for ( j=0; j < linebytes; j += 2 ) {
					*tmp = ((i*32/linetot) << 11) | ((i*64/linetot) << 5) | (i*32/linetot);
					tmp++;
				}
			} } } else {
				for ( i = 0; i < linetot; ++i ) {
					for ( j=0; j < linebytes; j++ ) {
						*(imagebuf+(i*linebytes)+j) = ((i*linebytes)+j)%256;
					}
				}
			}
			fprintf( fp, "image created.\nEnding access to the image buffer..." );
			fflush(fp);
			err = DiveEndImageBufferAccess( diveinst, bufnum );
			if ( err ) {
				fprintf( fp, "failed! DiveEndImageBufferAccess rc = %ld.\n",err );
				fflush(fp);
				DiveFreeImageBuffer( diveinst, bufnum );
				DiveClose( diveinst );
				break;
			}

			fprintf( fp, "done.\nSetting up for a blit operation... " );
			fflush(fp);

		break;
		case WM_VRNENABLE:
			hps=WinGetPS(win);
			if ( hps == 0 ) { err=1; break; }
			hrgn=GpiCreateRegion(hps, 0L, NULL);
			if (hrgn) {
				err = WinQueryVisibleRegion(win, hrgn);
				rgnCtl.ircStart=0;
				rgnCtl.crc=50;
				rgnCtl.ulDirection=RECTDIR_LFRT_TOPBOT;
				err = GpiQueryRegionRects(hps, hrgn, NULL, &rgnCtl, rcls);
				if ( err ) { 
					err = 0; 
					WinQueryWindowPos(win, &swp);
					pointl.x=swp.x;
					pointl.y=swp.y;
					WinMapWindowPoints(WinQueryWindow( win, QW_PARENT ), HWND_DESKTOP, (POINTL*)&pointl, 1);
					BlSet.ulStructLen = sizeof( SETUP_BLITTER );
					BlSet.fInvert = 0;
					BlSet.fccSrcColorFormat = blitdepth;
					BlSet.ulSrcWidth = 640;
					BlSet.ulSrcHeight = 480;
					BlSet.ulSrcPosX = 0;
					BlSet.ulSrcPosY = 0;
					BlSet.ulDitherType = 1;
					BlSet.fccDstColorFormat = FOURCC_SCRN;
					BlSet.ulDstWidth=swp.cx;
					BlSet.ulDstHeight=swp.cy;
					BlSet.lDstPosX = 0;
					BlSet.lDstPosY = 0;
					BlSet.lScreenPosX=pointl.x;
					BlSet.lScreenPosY=pointl.y;
					BlSet.ulNumDstRects=rgnCtl.crcReturned;
					BlSet.pVisDstRects=rcls;
					if ( rgnCtl.crcReturned == 0 ) { 
						fprintf( fp, "\nThere is no visible region to blit to!\n" );
						fflush(fp);
						DiveSetupBlitter( diveinst, NULL );
						GpiDestroyRegion(hps, hrgn);
						return MRFROMSHORT(-1);
					}
					fprintf( fp, "passing info to DIVE... " );
					fflush(fp);
					err = DiveSetupBlitter(diveinst, &BlSet);
					if ( err ) { fprintf( fp, "failed! DiveSetupBlitter rc = %ld.\n", err ); fflush(fp); return MRFROMSHORT(-1);}
					GpiDestroyRegion(hps, hrgn);
					fprintf( fp, "completed without errors.\n" );
					fflush(fp);
				}
			}
			WinReleasePS( hps );
		break;
		case WM_CLOSE:
			DiveFreeImageBuffer( diveinst, bufnum );
			DiveClose( diveinst );
		break;
		case WM_PAINT:
			err = DiveBlitImage( diveinst, bufnum, DIVE_BUFFER_SCREEN );
			if ( err ) {
				fprintf( fp, "DIVE blit error!  Return code: %d.\n", err );
				fflush( fp );
			}
	}
	return WinDefWindowProc( win, msg, mp1, mp2 );
}

void diag( void ) {
	ULONG rc;

	ULONG action;
	ULONG ulOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS;
	ULONG ulOpenMode = OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE;
	ULONG waittime = 250;
	ULONG size = sizeof( ULONG );
	HFILE timer;

	HAB ab;
	HWND diagframe, diagclient;
	HMQ messq;
	ULONG frameflgs= FCF_TITLEBAR | FCF_SHELLPOSITION | FCF_TASKLIST | FCF_ICON | FCF_SYSMENU | FCF_SIZEBORDER;
	QMSG qmsg;

	fprintf( fp, "Running MAME diagnostics...\n" );

	// Test major systems of MAME OS/2 and spit out a LOT of debug info.

	fprintf( fp, "Testing TIMER0 device driver... " );
	fflush(fp);
	if ( rc = DosOpen( "TIMER0$  ", &timer, &action, 0, 0, ulOpenFlag, ulOpenMode, NULL )) {
		fprintf( fp, "Failed!! DosOpen ret = %ld", rc );
		fflush(fp);
	} else {
		if ( (rc = DosDevIOCtl(timer, HRT_IOCTL_CATEGORY, HRT_BLOCKUNTIL,
		    &waittime, size, &size, NULL, 0, NULL)) != 0) {
			fprintf( fp, "Failed!! DosDevIOCtl ret = %ld.\n", rc );
			fflush(fp);
		} else {
			DosClose( timer );
			fprintf( fp, "passed.\n" );
			fflush(fp);
		}
	}
	fprintf( fp, "Testing for joystick driver... " );
	fflush(fp);
	JoystickInit(0);
	if ( !(rc=JoystickOn()) ) {
		fprintf( fp, "found and initialized successfully.\n" );
		fflush(fp);
	} else {
		fprintf( fp, "not detected.  JoystickOn rc = %ld.\n" ,rc );
		fflush(fp);
	}
	JoystickOff();

	fprintf( fp, "Preparing for DIVE tests... " );

	ab = WinInitialize( 0 );

	messq = WinCreateMsgQueue( ab, 0 );
	WinRegisterClass( ab, "MAME for OS/2 diagnostics", MameDiag, 
	     CS_SIZEREDRAW | CS_MOVENOTIFY, 0 );

	fprintf( fp, "done.\nTesting DIVE in 16 bit color...\n" );
	fflush(fp);
	blitdepth = FOURCC_R565;

	diagframe = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE | WS_ANIMATE,
	    &frameflgs, "MAME for OS/2 diagnostics", "M.A.M.E. for OS/2 diagnostics",
	    0, 0, 1, &diagclient );

	WinSetVisibleRegionNotify( diagclient, TRUE );
	WinPostMsg(diagframe, WM_VRNENABLE, 0L, 0L);

	if ( WinQueryFocus( HWND_DESKTOP != diagclient ) ) {
		WinSetFocus( HWND_DESKTOP, diagclient );
	}

	while (WinGetMsg (ab, &qmsg, NULLHANDLE, 0, 0))
	    WinDispatchMsg (ab, &qmsg) ;
	WinDestroyWindow (diagframe) ;

	fprintf( fp, "done.\nTesting DIVE in 8 bit color...\n" );
	fflush(fp);
	blitdepth = FOURCC_LUT8;
	diagframe = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE | WS_ANIMATE,
	    &frameflgs, "MAME for OS/2 diagnostics", "M.A.M.E. for OS/2 diagnostics",
	    0, 0, 1, &diagclient );

	WinSetVisibleRegionNotify( diagclient, TRUE );
	WinPostMsg(diagframe, WM_VRNENABLE, 0L, 0L);

	if ( WinQueryFocus( HWND_DESKTOP != diagclient ) ) {
		WinSetFocus( HWND_DESKTOP, diagclient );
	}

	while (WinGetMsg (ab, &qmsg, NULLHANDLE, 0, 0))
	    WinDispatchMsg (ab, &qmsg) ;
	WinDestroyWindow (diagframe) ;

	WinDestroyMsgQueue (messq) ;

	fprintf( fp, "Done testing.\n" );
	fflush(fp);
}
