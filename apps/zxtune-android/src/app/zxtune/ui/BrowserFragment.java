/**
 *
 * @file
 *
 * @brief Vfs browser fragment component
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.ui;

import java.io.IOException;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.support.v7.widget.SearchView;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import app.zxtune.PlaybackServiceConnection;
import app.zxtune.R;
import app.zxtune.fs.Vfs;
import app.zxtune.fs.VfsDir;
import app.zxtune.fs.VfsFile;
import app.zxtune.fs.VfsObject;
import app.zxtune.fs.VfsRoot;
import app.zxtune.playback.PlaybackService;
import app.zxtune.playback.PlaybackServiceStub;

public class BrowserFragment extends Fragment implements PlaybackServiceConnection.Callback {

  private static final String TAG = BrowserFragment.class.getName();
  private static final String SEARCH_QUERY_KEY = "search_query";
  private static final String SEARCH_FOCUSED_KEY = "search_focused";
  
  private final VfsRoot root;
  private PlaybackService service;
  private BrowserState state;
  private SearchView search;
  private View sources;
  private BreadCrumbsView position;
  private BrowserView listing;

  public static BrowserFragment createInstance() {
    return new BrowserFragment();
  }
  
  public BrowserFragment() {
    this.root = Vfs.getRoot();
    this.service = PlaybackServiceStub.instance();
  }

  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);
    
    state = new BrowserState(PreferenceManager.getDefaultSharedPreferences(activity));
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return container != null ? inflater.inflate(R.layout.browser, container, false) : null;
  }

  @Override
  public synchronized void onViewCreated(View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);
    
    sources = view.findViewById(R.id.browser_sources);
    search = (SearchView) view.findViewById(R.id.browser_search);
    search.setOnSearchClickListener(new SearchView.OnClickListener() {
      @Override
      public void onClick(View view) {
        final LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) search.getLayoutParams();
        params.width = LayoutParams.MATCH_PARENT;
        search.setLayoutParams(params);
      }
    });
    search.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
      @Override
      public boolean onQueryTextSubmit(String query) {
        loadSearch(query);
        search.clearFocus();
        return true;
      }
      
      @Override
      public boolean onQueryTextChange(String query) {
        return false;
      }
    });
    search.setOnCloseListener(new SearchView.OnCloseListener() {
      @Override
      public boolean onClose() {
        final LinearLayout.LayoutParams params = (LinearLayout.LayoutParams) search.getLayoutParams();
        params.width = LayoutParams.WRAP_CONTENT;
        search.setLayoutParams(params);
        search.post(new Runnable() {
          @Override
          public void run() {
            search.clearFocus();
            reloadFileBrowser();
          }
        });
        return false;
      }
    });
    search.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {
        if (0 == search.getQuery().length()) {
          search.setIconified(true);
        }
      }
    });
    
    final View roots = view.findViewById(R.id.browser_roots);
    roots.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v) {
        setCurrentDir(root);
      }
    });
    position = (BreadCrumbsView) view.findViewById(R.id.browser_breadcrumb);
    position.setDirSelectionListener(new BreadCrumbsView.DirSelectionListener() {
      @Override
      public void onDirSelection(VfsDir dir) {
        setCurrentDir(dir);
      }
    });
    listing = (BrowserView) view.findViewById(R.id.browser_content);
    listing.setOnItemClickListener(new OnItemClickListener());
    listing.setEmptyView(view.findViewById(R.id.browser_stub));
    listing.setProgressView(view.findViewById(R.id.browser_loading));
    listing.setMultiChoiceModeListener(new MultiChoiceModeListener());

    final Uri currentPath = state.getCurrentPath(); 
    if (savedInstanceState == null) {
      Log.d(TAG, "Load persistent state");
      loadBrowser(currentPath);
    } else {
      if (!reloadListing()) {
        loadBrowser(currentPath);
      } else {
        loadNavigation(currentPath);
      }
    }
  }
  
  private void reloadFileBrowser() {
    if (getSearchQuery() != null) {
      loadBrowser(state.getCurrentPath());
    }
  }
  
  @Override
  public void onSaveInstanceState(Bundle state) {
      super.onSaveInstanceState(state);
      final String query = search.getQuery().toString();
      if (query.length() != 0) {
        state.putString(SEARCH_QUERY_KEY, query);
        state.putBoolean(SEARCH_FOCUSED_KEY, search.hasFocus());
      }
  }

  @Override
  public void onViewStateRestored(Bundle state) {
      super.onViewStateRestored(state);
      if (state == null || !state.containsKey(SEARCH_QUERY_KEY)) {
        return;
      }
      final String query = state.getString(SEARCH_QUERY_KEY);
      final boolean isFocused = state.getBoolean(SEARCH_FOCUSED_KEY);
      search.post(new Runnable() {
        @Override
        public void run() {
          search.setIconified(false);
          search.setQuery(query, false);
          if (!isFocused) {
            search.clearFocus();
          }
        }
      });
  }  
  
  @Override
  public synchronized void onDestroyView() {
    super.onDestroyView();

    Log.d(TAG, "Saving persistent state");
    storeCurrentViewPosition();
    service = PlaybackServiceStub.instance();
  }
  
  public final void moveUp() {
    if (!search.isIconified()) {
      search.setIconified(true);
    } else {
      try {
        final VfsDir curDir = (VfsDir) root.resolve(state.getCurrentPath());
        if (curDir != root) {
          final VfsDir parent = curDir != null ? curDir.getParent() : null;
          setCurrentDir(parent != null ? parent : root);
        }
      } catch (IOException e) {
        listing.showError(e);
      }
    }
  }

  @Override
  public void onServiceConnected(PlaybackService service) {
    this.service = service;
  }
  
  private String getActionModeTitle() {
    final int count = listing.getCheckedItemsCount();
    return getResources().getQuantityString(R.plurals.items, count, count);
  }

  class OnItemClickListener implements AdapterView.OnItemClickListener {

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
      final Object obj = parent.getItemAtPosition(position);
      if (obj instanceof VfsDir) {
        setCurrentDir((VfsDir) obj);
      } else if (obj instanceof VfsFile) {
        final Uri[] toPlay = getUrisFrom(position);
        service.setNowPlaying(toPlay);
      }
    }

    private Uri[] getUrisFrom(int position) {
      final ListAdapter adapter = listing.getAdapter();
      final Uri[] result = new Uri[adapter.getCount() - position];
      for (int idx = 0; idx != result.length; ++idx) {
        final VfsObject obj = (VfsObject) adapter.getItem(position + idx);
        result[idx] = obj.getUri();
      }
      return result;
    }
  }

  private class MultiChoiceModeListener implements ListViewCompat.MultiChoiceModeListener {

    @Override
    public boolean onCreateActionMode(ListViewCompat.ActionMode mode, Menu menu) {
      setEnabledRecursive(sources, false);
      final MenuInflater inflater = mode.getMenuInflater();
      inflater.inflate(R.menu.selection, menu);
      inflater.inflate(R.menu.browser, menu);
      mode.setTitle(getActionModeTitle());
      return true;
    }

    @Override
    public boolean onPrepareActionMode(ListViewCompat.ActionMode mode, Menu menu) {
      return false;
    }

    @Override
    public boolean onActionItemClicked(ListViewCompat.ActionMode mode, MenuItem item) {
      if (listing.processActionItemClick(item.getItemId())) {
        return true;
      } else {
        switch (item.getItemId()) {
          case R.id.action_play:
            service.setNowPlaying(getSelectedItemsUris());
            break;
          case R.id.action_add:
            service.getPlaylistControl().add(getSelectedItemsUris());
            break;
          default:
            return false;
        }
        mode.finish();
        return true;
      }
    }

    @Override
    public void onDestroyActionMode(ListViewCompat.ActionMode mode) {
      setEnabledRecursive(sources, true);
    }

    @Override
    public void onItemCheckedStateChanged(ListViewCompat.ActionMode mode, int position, long id,
        boolean checked) {
      mode.setTitle(getActionModeTitle());
    }
    
    private Uri[] getSelectedItemsUris() {
      final Uri[] result = new Uri[listing.getCheckedItemsCount()];
      final SparseBooleanArray selected = listing.getCheckedItemPositions();
      final ListAdapter adapter = listing.getAdapter();
      int pos = 0;
      for (int i = 0, lim = selected.size(); i != lim; ++i) {
        if (selected.valueAt(i)) {
          final VfsObject obj = (VfsObject) adapter.getItem(selected.keyAt(i));
          result[pos++] = obj.getUri();
        }
      }
      return result;
    }
  }
  
  private static void setEnabledRecursive(View view, boolean enabled) {
    if (view instanceof ViewGroup) {
      final ViewGroup group = (ViewGroup) view;
      for (int idx = 0, lim = group.getChildCount(); idx != lim; ++idx) {
        setEnabledRecursive(group.getChildAt(idx), enabled);
      }
    } else {
      view.setEnabled(enabled);
    }
  }

  private void setCurrentDir(VfsDir dir) {
    storeCurrentViewPosition();
    setNewState(dir.getUri());
    loadBrowser(dir);
  }

  private void storeCurrentViewPosition() {
    state.setCurrentViewPosition(listing.getFirstVisiblePosition());
  }

  private void setNewState(Uri uri) {
    Log.d(TAG, "Set current path to " + uri);
    state.setCurrentPath(uri);
  }
  
  private void loadBrowser(Uri path) {
    try {
      final VfsObject obj = root.resolve(path);
      if (obj instanceof VfsDir) {
        loadBrowser((VfsDir) obj);
      } else {
        throw new IOException(getString(R.string.failed_resolve, path));
      }
    } catch (IOException e) {
      listing.showError(e);
    }
  }
  
  private void loadBrowser(VfsDir dir) {
    loadNavigation(dir);
    loadListing(dir);
  }
  
  private void loadNavigation(Uri path) {
    try {
      final VfsObject obj = root.resolve(path);
      if (obj instanceof VfsDir) {
        loadNavigation((VfsDir) obj);
      } else {
        throw new IOException(getString(R.string.failed_resolve, path));
      }
    } catch (IOException e) {
      listing.showError(e);
    }
  }

  private boolean reloadListing() {
    return listing.loadCurrent(getLoaderManager());
  }
  
  private void loadNavigation(VfsDir dir) {
    if (dir == root) {
      position.setDir(null);
    } else {
      position.setDir(dir);
    }
  }

  private void loadListing(VfsDir dir) {
    listing.loadNew(getLoaderManager(), dir, state.getCurrentViewPosition());
  }
  
  private void loadSearch(String query) {
    listing.loadSearch(getLoaderManager(), state.getCurrentPath(), query);
  }
  
  private String getSearchQuery() {
    return listing.getSearchQuery(getLoaderManager());
  }
}
