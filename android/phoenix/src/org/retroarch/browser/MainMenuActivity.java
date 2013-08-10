package org.retroarch.browser;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.retroarch.R;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.util.Log;
import android.widget.Toast;

public class MainMenuActivity extends PreferenceActivity {
	static private final String TAG = "MainMenu";
	@SuppressWarnings("deprecation")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.prefs);
		PreferenceManager.setDefaultValues(this, R.xml.prefs, false);
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
		
		// Extracting assets appears to take considerable amount of time, so
		// move extraction to a thread.
		Thread assetThread = new Thread(new Runnable() {
			public void run() {
				extractAssets();
			}
		});
		assetThread.start();
		
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		
		if (!prefs.getBoolean("first_time_refreshrate_calculate", false)) {
			prefs.edit().putBoolean("first_time_refreshrate_calculate", true).commit();
			
			if (!detectDevice(false))
			{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
				.setTitle("Two modes of play - pick one")
				.setMessage("RetroArch has two modes of play: synchronize to refreshrate, and threaded video.\n\nSynchronize to refreshrate gives the most accurate results and can produce the smoothest results. However, it is hard to configure right and might result in unpleasant audio crackles when it has been configured wrong.\n\nThreaded video should work fine on most devices, but applies some adaptive video jittering to achieve this.\n\nChoose which of the two you want to use. (If you don't know, go for Threaded video). ")
				.setPositiveButton("Threaded video", new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
						SharedPreferences.Editor edit = prefs.edit();
						edit.putBoolean("video_threaded", true);
						edit.commit();
					}
				})
				.setNegativeButton("Synchronize to refreshrate", new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int which) {
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
						SharedPreferences.Editor edit = prefs.edit();
						edit.putBoolean("video_threaded", false);
						edit.commit();
						Intent i = new Intent(getBaseContext(), DisplayRefreshRateTest.class);
						startActivity(i);
					}
				});
			alert.show();
			}
		}
	}
	
	private byte[] loadAsset(String asset) throws IOException {
		String path = asset;
		InputStream stream = getAssets().open(path);
		int len = stream.available();
		byte[] buf = new byte[len];
		stream.read(buf, 0, len);
		return buf;
	}
	
	private void extractAssets(AssetManager manager, String dataDir, String relativePath, int level) throws IOException {
		final String[] paths = manager.list(relativePath);
		if (paths != null && paths.length > 0) { // Directory
			//Log.d(TAG, "Extracting assets directory: " + relativePath);
			for (final String path : paths)
				extractAssets(manager, dataDir, relativePath + (level > 0 ? File.separator : "") + path, level + 1);
		} else { // File, extract.
			//Log.d(TAG, "Extracting assets file: " + relativePath);
			
			String parentPath = new File(relativePath).getParent();
			if (parentPath != null) {
				File parentFile = new File(dataDir, parentPath);
				parentFile.mkdirs(); // Doesn't throw.
			}
			
			byte[] asset = loadAsset(relativePath);
			BufferedOutputStream writer = new BufferedOutputStream(
					new FileOutputStream(new File(dataDir, relativePath)));

			writer.write(asset, 0, asset.length);
			writer.flush();
			writer.close();
		}
	}
	
	private void extractAssets() {
		int version = 0;
		try {
			version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
		} catch(NameNotFoundException e) {
			// weird exception, shouldn't happen
		}
		
		try {
			AssetManager assets = getAssets();
			String dataDir = getApplicationInfo().dataDir;
			File cacheVersion = new File(dataDir, ".cacheversion");
			if (cacheVersion != null && cacheVersion.isFile() && cacheVersion.canRead() && cacheVersion.canWrite())
			{
				DataInputStream cacheStream = new DataInputStream(new FileInputStream(cacheVersion));

				int currentCacheVersion = 0;
				try {
					currentCacheVersion = cacheStream.readInt();
				} catch (IOException e) {}
			    cacheStream.close();
			    
				if (currentCacheVersion == version)
				{
					Log.i("ASSETS", "Assets already extracted, skipping...");
					return;
				}
			}
			
			//extractAssets(assets, cacheDir, "", 0);
			Log.i("ASSETS", "Extracting shader assets now ...");
			try {
				extractAssets(assets, dataDir, "shaders_glsl", 1);
			} catch (IOException e) {
				Log.i("ASSETS", "Failed to extract shaders ...");
			}
			
			Log.i("ASSETS", "Extracting overlay assets now ...");
			try {
				extractAssets(assets, dataDir, "overlays", 1);
			} catch (IOException e) {
				Log.i("ASSETS", "Failed to extract overlays ...");
			}
			
			DataOutputStream outputCacheVersion = new DataOutputStream(new FileOutputStream(cacheVersion, false));
			outputCacheVersion.writeInt(version);
			outputCacheVersion.close();
		} catch (IOException e) {
			Log.e(TAG, "Failed to extract assets to cache.");			
		}
	}
	
	boolean detectDevice(boolean show_dialog)
	{
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		SharedPreferences.Editor edit = prefs.edit();
		edit.putBoolean("video_threaded", true);
		edit.commit();
		
		Log.i("Device MODEL", android.os.Build.MODEL);
		if (android.os.Build.MODEL.equals("SHIELD"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
			.setTitle("NVidia Shield detected")
			.setMessage("Would you like to set up the ideal configuration options for your device?\nNOTE: For optimal performance, turn off Google Account sync, GPS and Wifi in your Android settings menu.\n\nNOTE: If you know what you are doing, it is advised to turn 'Threaded Video' off afterwards and try running games without it. In theory it will be smoother, but your mileage varies depending on your device and configuration.")
			.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.valueOf(60.00d).toString());
					edit.putBoolean("input_overlay_enable", false);
					edit.putBoolean("input_autodetect_enable", true);
					edit.commit();
				}
			})
			.setNegativeButton("No", null);
			alert.show();
			return true;
		}
		else if (android.os.Build.ID.equals("JSS15J"))
		{
			AlertDialog.Builder alert = new AlertDialog.Builder(this)
			.setTitle("Nexus 7 2013 detected")
			.setMessage("Would you like to set up the ideal configuration options for your device?\nNOTE: For optimal performance, turn off Google Account sync, GPS and Wifi in your Android settings menu. \n\nNOTE: If you know what you are doing, it is advised to turn 'Threaded Video' off afterwards and try running games without it. In theory it will be smoother, but your mileage varies depending on your device and configuration.")
			.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
					SharedPreferences.Editor edit = prefs.edit();
					edit.putString("video_refresh_rate", Double.valueOf(59.65).toString());
					edit.commit();
				}
			})
			.setNegativeButton("No", null);
			alert.show();
			return true;
		}
		
		if (show_dialog) {
		Toast.makeText(this,
				"Device either not detected in list or doesn't have any optimal settings in our database.",
				Toast.LENGTH_SHORT).show();
		}
		
		return false;
	}
	
	@Override
	protected void onStart() {
		super.onStart();
	}
}
