/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

package com.mali.testjava;

import com.mali.testjava.R;

import android.app.Activity;
import android.app.KeyguardManager;
import android.app.KeyguardManager.KeyguardLock;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;

public class MaliTestMain extends Activity implements SurfaceHolder.Callback {

	private native void malitestmain(int argc, String argv[]);
	private Surface mSurface;
	private Boolean sDone = false;
	private Boolean appReady = false;
	private Boolean appTerminating = false;
	private int windowFullscreenWidth;
	private int windowFullscreenHeight;
	private static String MSG_TYPE = "MSG_TYPE";
	private static int MSG_TYPE_FINISH = 0;
	private static int MSG_TYPE_CREATE_SURFACE = 1;
	private static String SURFACE_HEIGHT = "SURFACE_HEIGHT";
	private static String SURFACE_WIDTH = "SURFACE_WIDTH";
	private static String SURFACE_FORMAT = "SURFACE_FORMAT";
	private static final String TAG = "mali-test-framework";
	private static final String DEFAULT_LIBRARY = "/data/data/com.mali.testjava/lib/libmali-testjava.so";
	private static final int DEFAULT_FORMAT = android.graphics.PixelFormat.RGB_565;
	private String library;
	private String argv;
	private KeyguardManager mKeyGuardManager;
	private KeyguardLock mLock;
	private int currentFormat = android.graphics.PixelFormat.UNKNOWN;
	private int requiredFormat = android.graphics.PixelFormat.UNKNOWN;
	private int requiredWidth = 16;
	private int requiredHeight = 16;
	private Boolean changePending = false;
	private int numSurfaces = 0;


	@Override
	/** Called when the activity is first created. */
	public void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		Log.i(TAG, "mali-test-framework created");

		/* Remote window decoration */
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);

		/* Handle dynamic library loading and command line options */
		Intent intent = getIntent();
		final String action = intent.getAction();
		library = DEFAULT_LIBRARY;

		/* The first argument is always the command itself */
		argv = library;

		if(action.equals(Intent.ACTION_RUN))
		{
			final String lib = intent.getStringExtra("TEST_LIB");
			final String command_line = intent.getStringExtra("COMMAND_LINE");

			/* If no library was passed then we won't process the command line */
			if(lib != null && !lib.isEmpty())
			{
				argv = library = lib;
				if(command_line != null)
				{
					Log.i(TAG, String.format("Commands %s", command_line));
					argv = argv + " " + command_line;
				}
			}
			else
			{
				Log.w(TAG, "RUN Action requested but no library specified");
			}
		}

		System.load(library);

		setContentView(R.layout.main);
		appReady = false;
		SurfaceView newView = (SurfaceView) findViewById(R.id.malisurfaceview);
		SurfaceHolder holder = newView.getHolder();
		holder.addCallback(com.mali.testjava.MaliTestMain.this);
		holder.setFormat(MaliTestMain.DEFAULT_FORMAT);

		/* This code is used to disable the lockscreen while the application is running */
		mKeyGuardManager = (KeyguardManager) getSystemService(KEYGUARD_SERVICE);
		mLock = mKeyGuardManager.newKeyguardLock("MaliTestMain");
		mLock.disableKeyguard();
	}

	@Override
	protected void onRestart() {
		super.onRestart();
		Log.i(TAG, "mali-test-framework restart");
	}

	@Override
	protected void onStart() {
		super.onStart();
		Log.i(TAG, "mali-test-framework start");
	}

	@Override
	protected void onPause() {
		super.onPause();
		Log.i(TAG, "mali-test-framework pause");
	}

	@Override
	protected void onStop() {
		super.onStop();
		Log.i(TAG, "mali-test-framework stop");
	}
		
	@Override
	public void onDestroy() {
		/* This is supposed to be the last method called - however experience has shown
		 * that there can be a fairly big pause between finish() being called and
		 * onDestroy() being called.
		 */
		super.onDestroy();
		Log.i(TAG, "mali-test-framework complete");
		appTerminating = true;
		if( numSurfaces <= 0 ) {
			/* No surfaces remain but onDestroy should return.
			 * So initiate a delayed termination
			 */
			suicide();
		}
	}

	public Surface getWindowSurface(int width, int height, int pixel_format) {
		/* Called by the native test app (running within the test thread below) */
		Message a = uiHandler.obtainMessage();
		Bundle b = new Bundle();

		/* Create a message to pass to the ui thread specifying surface required */
		b.putInt(MaliTestMain.MSG_TYPE, MaliTestMain.MSG_TYPE_CREATE_SURFACE);
		b.putInt(MaliTestMain.SURFACE_WIDTH, width);
		b.putInt(MaliTestMain.SURFACE_HEIGHT, height);
		b.putInt(MaliTestMain.SURFACE_FORMAT, pixel_format);
		a.setData(b);

		sDone = false;
		uiHandler.sendMessage(a);

		Log.i(TAG, "Waiting on surface creation/modification");
		while(false == sDone)
		{
			Thread.yield();
		}
		Log.i(TAG, "Surface creation/modification complete");
		return mSurface;
	}

	@Override
	protected void onResume() {

		super.onResume();
		Log.i(TAG, "mali-test-framework resume");

		/* Create a thread which will run the native test application */
		Thread testThread = new Thread() {
			public void run() {
				while(appReady == false)
				{
					Thread.yield();
				}

				/* Construct arguments for main */

				/* Split the string up according to whitespaces */
				String commands[] = argv.split("\\s");

				Log.i(TAG, "Set-up complete. Starting test.");
				malitestmain(commands.length, commands);
				Log.i(TAG, "Test complete");

				Message a = uiHandler.obtainMessage();
				Bundle b = new Bundle();

				b.putInt(MaliTestMain.MSG_TYPE, MaliTestMain.MSG_TYPE_FINISH);
				a.setData(b);
				uiHandler.sendMessage(a);
			}
		};

		testThread.start();
	}

	final Handler uiHandler = new Handler() {
		public void handleMessage(Message msg) {
			Bundle data = msg.getData();
			int msgType = data.getInt(MaliTestMain.MSG_TYPE);

			if(msgType == MaliTestMain.MSG_TYPE_FINISH)
			{
				Log.i(TAG, "Finishing");
				finish();
				return;
			}

			requiredWidth = data.getInt(MaliTestMain.SURFACE_WIDTH);
			requiredHeight = data.getInt(MaliTestMain.SURFACE_HEIGHT);
			requiredFormat = data.getInt(MaliTestMain.SURFACE_FORMAT);

			if(requiredFormat == 0)
			{
				requiredFormat = MaliTestMain.DEFAULT_FORMAT;
			}

			/* Check if fullscreen has been requested */
			if(requiredWidth == 0)
			{
				requiredWidth = windowFullscreenWidth;
			}

			if(requiredHeight == 0)
			{
				requiredHeight = windowFullscreenHeight;
			}

			SurfaceView newView = (SurfaceView) findViewById(R.id.malisurfaceview);
			SurfaceHolder holder = newView.getHolder();
			android.view.ViewGroup.LayoutParams lp = newView.getLayoutParams();

			/* Take a local copy of the format recorded at the last surfaceChange */
			int curFormat = currentFormat;

			if (requiredFormat != curFormat) {
				/* We will have to request a format change (below) */
				changePending = true;
			}

			if((requiredWidth != newView.getWidth()) ||
				(requiredHeight != newView.getHeight()) ||
				(requiredWidth != lp.width) ||
				(requiredHeight != lp.height) )
			{
				/* dimensions and/or layout are WRONG */
				changePending = true;

				lp.width = requiredWidth;
				lp.height = requiredHeight;

				/* Set correct dimensions */
				Log.i(TAG, String.format("Set Width: %d, Height %d\n", lp.width, lp.height));
				newView.setLayoutParams(lp);
				holder.setFixedSize(requiredWidth,requiredHeight);

				if (requiredFormat == curFormat)
				{
					/* Format is already correct. We need to force a format change,
					 * specify a different format.
					 */
					if( requiredFormat == android.graphics.PixelFormat.A_8 )
					{
						curFormat = MaliTestMain.DEFAULT_FORMAT;
					}
					else
					{
						curFormat = android.graphics.PixelFormat.A_8;
					}

					/* Set the format - this immediately causes the surface to change */
					Log.i(TAG, String.format("Set incorrect format: %d\n", curFormat));
					holder.setFormat(curFormat);

					/* proceed to following 'if' which will set format back to the correct
					 * format. Note that the 'if' uses curFormat instead of currentFormat
					 * because currentFormat might not yet have been changed by
					 * surfaceChanged().
					 */
				}
			}

			if (requiredFormat != curFormat)
			{
				Log.i(TAG, String.format("Set format: %d\n", requiredFormat));
				holder.setFormat(requiredFormat);
			}
			else
			{
				/* Handling the case where the dimensions and format haven't changed
				 * (and therefore we don't get a surfaceChaged callback
				 */
				mSurface = holder.getSurface();

				/* changePending should already be false but just to be sure */
				changePending = false;

				/* Signal test thread to continue. */
				sDone = true;
			}
		}
	};

	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

		Log.i(TAG, "mali-test-framework surfaceChanged");

		mSurface = holder.getSurface();

		/* Note that on ICS we have seen surfaceChanged being called when we haven't
		 * yet issued a request to change the dimensions or format. To overcome this
		 * we use the changePending flag (which is only set to true once all the
		 * 'required' variables have been set)
		 */

		/* record the current format */
		currentFormat = format;

		if(appReady == true)
		{
			SurfaceView newView = (SurfaceView) findViewById(R.id.malisurfaceview);
			android.view.ViewGroup.LayoutParams lp = newView.getLayoutParams();

			if( changePending &&
					(format == requiredFormat) &&
					(height == requiredHeight) &&
					(width == requiredWidth) &&
					(lp.height == requiredHeight) &&
					(lp.width == requiredWidth) )
			{
				/* Dimensions, layout and format are all correct. */

				/* No longer expecting any further changes */
				changePending = false;

				 /* Signal test thread to continue. */
				sDone = true;
			}
			else
			{
				/* Dimensions, layout or format are not correct, or we haven't
				 * requested any change. Assume corrective action will follow
				 * (nothing else we can do).
				 */
			}		
		}
		else
		{
			/* The test application has started so we store the fullscreen width and height
			 * so that we can make fullscreen windows when requested
			 */
			appReady = true;
			windowFullscreenWidth = width;
			windowFullscreenHeight = height;
		}
	}


	public void surfaceCreated(SurfaceHolder holder) {

		Log.i(TAG, "mali-test-framework surfaceCreated");
		numSurfaces++;

		/* Note that surfaceCreated is always followed by a call to surfaceChanged() */
		mSurface = holder.getSurface();
	}

	public void surfaceDestroyed(SurfaceHolder holder) {

		Log.i(TAG, "mali-test-framework surfaceDestroyed");
		numSurfaces--;

		currentFormat = android.graphics.PixelFormat.UNKNOWN;

		if( numSurfaces == 0 && appTerminating )
			/* surfaceDestroyed has been called after onDestroy. Typically
			 * it is the last method in the app that Android calls. It must
			 * return. Initiate a delayed termination.
			 */
			suicide();
	}

	protected void suicide() {
		/* Delayed app termination. */
		
		Log.i(TAG, "mali-test-framework suicide");

		if( !appTerminating )
			Log.i(TAG, "App is not supposed to be terminating!");
		else {		
			/* Create a thread which will pause and then call exit */
			Thread suicideThread = new Thread() {
				public void run() {
					try {
						sleep(500);
					}
					catch (InterruptedException ie){
					}
					 Log.i(TAG, "mali-test-framework exit");
					System.exit(0);
				}
			};
			suicideThread.start();
		}
	}
}
