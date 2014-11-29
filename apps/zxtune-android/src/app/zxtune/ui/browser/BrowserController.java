/**
 * 
 * @file
 *
 * @brief Browser business logic storage
 *
 * @author vitamin.caig@gmail.com
 * 
 */

package app.zxtune.ui.browser;

import java.io.IOException;
import java.lang.ref.WeakReference;

import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.View;
import android.widget.BaseAdapter;
import android.widget.ProgressBar;
import app.zxtune.MainApplication;
import app.zxtune.R;
import app.zxtune.fs.Vfs;
import app.zxtune.fs.VfsDir;
import app.zxtune.fs.VfsFile;
import app.zxtune.fs.VfsRoot;

public class BrowserController {
  
  private final static String TAG = BrowserController.class.getName();
  private final static int LOADER_ID = BrowserController.class.hashCode();
  
  private final LoaderManager loaderManager;
  private final BrowserState state;
  private BreadCrumbsView position;
  private ProgressBar progress;
  private BrowserView listing;

  public BrowserController(Fragment fragment) {
    this.loaderManager = fragment.getLoaderManager();
    this.state = new BrowserState(PreferenceManager.getDefaultSharedPreferences(fragment.getActivity()));
  }
  
  public final void setViews(BreadCrumbsView position, ProgressBar progress, BrowserView listing) {
    this.position = position;
    this.progress = progress;
    this.listing = listing;
  }
  
  public final void loadState() {
    if (!reloadCurrentState()) {
      loadCurrentDir();
    }
  }
  
  public final void search(String query) {
    try {
      final Uri currentPath = state.getCurrentPath();
      final VfsDir currentDir = (VfsDir) Vfs.getRoot().resolve(currentPath);
      loaderManager.destroyLoader(LOADER_ID);
      final LoaderManager.LoaderCallbacks<?> cb = SearchingLoaderCallback.create(this, currentDir, query);
      loaderManager.initLoader(LOADER_ID, null, cb).forceLoad();
    } catch (Exception e) {
      listing.showError(e);
    }
  }
  
  public final boolean isInSearch() {
    final Loader<?> loader = loaderManager.getLoader(LOADER_ID); 
    return loader instanceof SearchingLoader;
  }
  
  public final void setCurrentDir(VfsDir dir) {
    storeCurrentViewPosition();
    state.setCurrentPath(dir.getUri());
    setDirectory(dir);
  }
  
  public final void moveToParent() {
    try {
      final VfsRoot root = Vfs.getRoot();
      final VfsDir curDir = (VfsDir) root.resolve(state.getCurrentPath());
      if (curDir != root) {
        final VfsDir parent = curDir != null ? curDir.getParent() : null;
        setCurrentDir(parent != null ? parent : root);
      }
    } catch (IOException e) {
      listing.showError(e);
    }
  }
  
  public final void storeCurrentViewPosition() {
    state.setCurrentViewPosition(listing.getFirstVisiblePosition());
  }

  private boolean reloadCurrentState() {
    final Loader<?> loader = loaderManager.getLoader(LOADER_ID); 
    if (loader == null) {
      Log.d(TAG, "Expired loader");
      return false;
    }
    if (loader instanceof SearchingLoader) {
      loaderManager.initLoader(LOADER_ID, null, SearchingLoaderCallback.create(this, (SearchingLoader) loader));
    } else {
      loaderManager.initLoader(LOADER_ID, null, ListingLoaderCallback.create(this, (ListingLoader) loader));
    }
    return true;
  }
  
  public final void loadCurrentDir() {
    try {
      final Uri currentPath = state.getCurrentPath();
      final VfsDir currentDir = (VfsDir) Vfs.getRoot().resolve(currentPath);
      setDirectory(currentDir);
    } catch (Exception e) {
      listing.showError(e);
    }
  }
  
  private void setDirectory(VfsDir dir) {
    if (dir != Vfs.getRoot()) {
      position.setDir(dir);
    } else {
      position.setDir(null);
    }
    loadListing(dir, state.getCurrentViewPosition());
  }
  
  //Required to call forceLoad due to bug in support library.
  //Some methods on callback does not called... 
  private void loadListing(VfsDir dir, int viewPosition) {
    loaderManager.destroyLoader(LOADER_ID);
    listing.setTag(viewPosition);
    final LoaderManager.LoaderCallbacks<?> cb = ListingLoaderCallback.create(this, dir);
    loaderManager.initLoader(LOADER_ID, null, cb).forceLoad();
  }
  
  private static abstract class WeakCallback<T> implements LoaderManager.LoaderCallbacks<T> {
    
    private WeakReference<BrowserController> control;
    
    protected synchronized void setControl(BrowserController ctrl) {
      this.control = new WeakReference<BrowserController>(ctrl);
    }
    
    protected synchronized BrowserController getControl() {
      return control.get();
    }
  }
  
  private static class ListingLoaderCallback extends WeakCallback<BrowserViewModel> implements ListingLoader.Callback {
    
    private final VfsDir dir;
    
    private ListingLoaderCallback(VfsDir dir) {
      this.dir = dir;
    }
    
    static LoaderManager.LoaderCallbacks<BrowserViewModel> create(BrowserController ctrl, VfsDir dir) {
      final ListingLoaderCallback cb = new ListingLoaderCallback(dir);
      cb.setControl(ctrl);
      ctrl.listingStarted();
      return cb;
    }
    
    static LoaderManager.LoaderCallbacks<BrowserViewModel> create(BrowserController ctrl, ListingLoader loader) {
      final ListingLoaderCallback cb = (ListingLoaderCallback) loader.getCallback();
      cb.setControl(ctrl);
      if (loader.isStarted()) {
        ctrl.listingStarted();
      }
      ctrl.position.setDir(cb.dir);
      return cb;
    }

    @Override
    public Loader<BrowserViewModel> onCreateLoader(int id, Bundle params) {
      return new ListingLoader(MainApplication.getInstance(), dir, this);
    }

    @Override
    public void onLoadFinished(Loader<BrowserViewModel> loader, BrowserViewModel model) {
      final BrowserController ctrl = getControl();
      if (ctrl == null) {
        return;
      }
      if (model != null) {
        ctrl.loadingFinished();
        ctrl.listing.setModel(model);
        final Integer pos = (Integer) ctrl.listing.getTag();
        if (pos != null) {
          ctrl.listing.setSelection(pos);
          ctrl.listing.setTag(null);
        }
      } else {
        ctrl.hideProgress();
      }
    }

    @Override
    public void onLoaderReset(Loader<BrowserViewModel> loader) {
      final BrowserController ctrl = getControl();
      if (ctrl != null) {
        ctrl.loadingFinished();
      }
    }

    @Override
    public void onProgressInit(int total) {
      final BrowserController ctrl = getControl();
      if (ctrl != null) {
        ctrl.progress.setProgress(0);
        ctrl.progress.setMax(total);
        ctrl.progress.setIndeterminate(false);
      }
    }

    @Override
    public void onProgressUpdate(final int current) {
      if (0 == current % 5) {
        final BrowserController ctrl = getControl();
        if (ctrl != null) {
          ctrl.progress.post(new Runnable() {
            @Override
            public void run() {
              ctrl.progress.setProgress(current);
            }
          });
        }
      }
    }

    @Override
    public void onError(Exception e) {
      final BrowserController ctrl = getControl();
      if (ctrl != null) {
        ctrl.listing.showError(e);
      }
    }
  }
  
  private static class SearchingLoaderCallback extends WeakCallback<Void> implements SearchingLoader.Callback {
    
    private final VfsDir dir;
    private final String query;
    private final RealBrowserViewModel model;
    
    private SearchingLoaderCallback(VfsDir dir, String query) {
      this.dir = dir;
      this.query = query;
      this.model = new RealBrowserViewModel(MainApplication.getInstance());
    }
    
    static LoaderManager.LoaderCallbacks<Void> create(BrowserController ctrl, VfsDir dir, String query) {
      final SearchingLoaderCallback cb = new SearchingLoaderCallback(dir, query);
      synchronized (cb) {
        cb.setControl(ctrl);
        ctrl.listing.setModel(cb.model);
      }
      ctrl.searchingStarted();
      return cb;
    }
    
    static LoaderManager.LoaderCallbacks<Void> create(BrowserController ctrl, SearchingLoader loader) {
      final SearchingLoaderCallback cb = (SearchingLoaderCallback) loader.getCallback();
      synchronized (cb) {
        cb.setControl(ctrl);
        ctrl.listing.setModel(cb.model);
      }
      ctrl.position.setDir(cb.dir);
      if (loader.isStarted()) {
        ctrl.searchingStarted();
      }
      return cb;
    }
    
    @Override
    public Loader<Void> onCreateLoader(int id, Bundle params) {
      return new SearchingLoader(MainApplication.getInstance(), dir, query, this);
    }

    @Override
    public void onLoadFinished(Loader<Void> loader, Void result) {
      loadingFinished();
    }

    @Override
    public void onLoaderReset(Loader<Void> loader) {
      loadingFinished();
    }
    
    @Override
    public void onFileFound(final VfsFile file) {
      final BrowserController ctrl = getControl();
      if (ctrl != null) {
        ctrl.listing.post(new Runnable() {
          @Override
          public void run() {
            model.add(file);
            ((BaseAdapter) ctrl.listing.getAdapter()).notifyDataSetChanged();
          }
        });
      } else {
        model.add(file);
      }
    }
    
    private void loadingFinished() {
      final BrowserController ctrl = getControl();
      if (ctrl != null) {
        ctrl.loadingFinished();
      }
    }
  }
  
  private void listingStarted() {
    showProgress();
    listing.setEmptyViewText(R.string.browser_loading);
  }
  
  private void searchingStarted() {
    showProgress();
    listing.setEmptyViewText(R.string.browser_searching);
  }

  private void loadingFinished() {
    hideProgress();
    listing.setEmptyViewText(R.string.browser_empty);
  }

  private void showProgress() {
    progress.setVisibility(View.VISIBLE);
    progress.setIndeterminate(true);
  }
  
  private void hideProgress() {
    progress.setVisibility(View.INVISIBLE);
  }
}