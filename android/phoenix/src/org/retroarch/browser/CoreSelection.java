package org.retroarch.browser;

import org.retroarch.R;
import org.retroarch.browser.preferences.ConfigFile;
import org.retroarch.browser.preferences.UserPreferences;

import java.io.*;

import android.app.*;
import android.media.AudioManager;
import android.os.*;
import android.widget.*;
import android.util.Log;
import android.view.*;

// JELLY_BEAN_MR1 = 17

public final class CoreSelection extends ListActivity {
	private IconAdapter<ModuleWrapper> adapter;
	private static final String TAG = "CoreSelection";

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		ConfigFile core_config = new ConfigFile();
		try {
			core_config.append(getAssets().open("libretro_cores.cfg"));
		} catch (IOException e) {
			Log.e(TAG, "Failed to load libretro_cores.cfg from assets.");
		}

		final String cpuInfo = UserPreferences.readCPUInfo();
		final boolean cpuIsNeon = cpuInfo.contains("neon");

		setContentView(R.layout.line_list);

		// Setup the list
		adapter = new IconAdapter<ModuleWrapper>(this, R.layout.line_list_item);
		ListView list = getListView();
		list.setAdapter(adapter);

		setTitle(R.string.select_libretro_core);

		// Populate the list
		final String modulePath = getApplicationInfo().nativeLibraryDir;
		final File[] libs = new File(modulePath).listFiles();
		for (final File lib : libs) {
			String libName = lib.getName();

			Log.i(TAG, "Libretro core: " + libName);

			// Allow both libretro-core.so and libretro_core.so.
			if (!libName.startsWith("libretroarch")) {
				try {
					adapter.add(new ModuleWrapper(this, lib, core_config));
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}

		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);
	}

	@Override
	public void onListItemClick(ListView listView, View view, int position, long id) {
		final ModuleWrapper item = adapter.getItem(position);
		MainMenuActivity.getInstance().setModule(item.file.getAbsolutePath(), item.getText());
		UserPreferences.updateConfigFile(this);
		finish();
	}
}
