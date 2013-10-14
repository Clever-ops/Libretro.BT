package org.retroarch.browser.preferences.fragments;

import org.retroarch.R;
import org.retroarch.browser.preferences.fragments.util.PreferenceListFragment;

import android.os.Bundle;

/**
 * A {@link PreferenceListFragment} that handles the path preferences.
 */
public final class PathPreferenceFragment extends PreferenceListFragment
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Add path preferences from the XML.
		addPreferencesFromResource(R.xml.path_preferences);
	}
}
