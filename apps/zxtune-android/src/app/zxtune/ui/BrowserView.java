/*
 * @file
 * @brief BrowserView class
 * @version $Id:$
 * @author (C) Vitamin/CAIG
 */

package app.zxtune.ui;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.AsyncTaskLoader;
import android.support.v4.content.Loader;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;
import app.zxtune.R;
import app.zxtune.fs.VfsDir;
import app.zxtune.fs.VfsFile;

public class BrowserView extends CheckableListView {

  private static final int LOADER_ID = BrowserView.class.hashCode();

  private View loadingView;
  private TextView emptyView;

  public BrowserView(Context context) {
    super(context);
    initView();
  }

  public BrowserView(Context context, AttributeSet attr) {
    super(context, attr);
    initView();
  }

  public BrowserView(Context context, AttributeSet attr, int defaultStyles) {
    super(context, attr, defaultStyles);
    initView();
  }
  
  private void initView() {
    setAdapter(new BrowserViewAdapter());
  }

  @Override
  public void setEmptyView(View stub) {
    super.setEmptyView(stub);
    loadingView = stub.findViewById(R.id.browser_loading);
    emptyView = (TextView) stub.findViewById(R.id.browser_loaded);
  }
  
  //Required to call forceLoad due to bug in support library.
  //Some methods on callback does not called... 
  final void load(LoaderManager manager, VfsDir dir, Integer pos) {
    final ModelLoaderCallback cb = new ModelLoaderCallback(dir, pos);
    if (pos != null) {
      manager.restartLoader(LOADER_ID, null, cb).forceLoad();
    } else {
      assert manager.getLoader(LOADER_ID) != null;
      manager.initLoader(LOADER_ID, null, cb);
    }
  }
  
  final void showError(Exception e) {
    setModel(null);
    final Throwable cause = e.getCause();
    final String msg = cause != null ? cause.getMessage() : e.getMessage();
    emptyView.setText(msg);
  }
  
  private void setModel(BrowserViewModel model) {
    ((BrowserViewAdapter) getAdapter()).setModel(model);
  }

  //TODO: use ViewFlipper?
  private void showProgress() {
    loadingView.setVisibility(VISIBLE);
    emptyView.setVisibility(INVISIBLE);
  }

  private void hideProgress() {
    loadingView.setVisibility(INVISIBLE);
    emptyView.setVisibility(VISIBLE);
  }
  
  private static class BrowserViewAdapter extends BaseAdapter {
    
    private BrowserViewModel model;
    
    BrowserViewAdapter() {
      this.model = getSafeModel(null);
    }

    @Override
    public int getCount() {
      return model.getCount();
    }

    @Override
    public Object getItem(int position) {
      return model.getItem(position);
    }

    @Override
    public long getItemId(int position) {
      return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
      return model.getView(position, convertView, parent);
    }

    @Override
    public boolean isEmpty() {
      return model.isEmpty();
    }
    
    final void setModel(BrowserViewModel model) {
      if (this.model == model) {
        return;
      }
      this.model = getSafeModel(model);
      if (model != null) {
        notifyDataSetChanged();
      } else {
        notifyDataSetInvalidated();
      }
    }
    
    private BrowserViewModel getSafeModel(BrowserViewModel model) {
      if (model != null) {
        return model;
      } else {
        return new EmptyBrowserViewModel();
      }
    }
  }
  
  private class ModelLoaderCallback implements LoaderManager.LoaderCallbacks<BrowserViewModel> {
    
    private final VfsDir dir;
    private final Integer pos;
    
    ModelLoaderCallback(VfsDir dir, Integer pos) {
      this.dir = dir;
      this.pos = pos;
    }
    
    @Override
    public Loader<BrowserViewModel> onCreateLoader(int id, Bundle params) {
      assert id == LOADER_ID;
      showProgress();
      setModel(null);
      return new ModelLoader(getContext(), dir, BrowserView.this);
    }

    @Override
    public void onLoadFinished(Loader<BrowserViewModel> loader, BrowserViewModel model) {
      hideProgress();
      setModel(model);
      if (pos != null) {
        setSelection(pos);
      }
    }

    @Override
    public void onLoaderReset(Loader<BrowserViewModel> loader) {
      hideProgress();
    }
  }
  
  //Typical AsyncTaskLoader workflow from
  //http://developer.android.com/intl/ru/reference/android/content/AsyncTaskLoader.html
  //Must be static!!!
  private static class ModelLoader extends AsyncTaskLoader<BrowserViewModel> {
    
    private final VfsDir dir;
    private final BrowserView view;

    public ModelLoader(Context context, VfsDir dir, BrowserView view) {
      super(context);
      this.dir = dir;
      this.view = view;
      view.emptyView.setText(R.string.playlist_empty);
    }
    
    @Override
    public BrowserViewModel loadInBackground() {
      try {
        final RealBrowserViewModel model = new RealBrowserViewModel(getContext());
        dir.enumerate(new VfsDir.Visitor() {
          
          @Override
          public Status onFile(VfsFile file) {
            model.add(file);
            return VfsDir.Visitor.Status.CONTINUE;
          }
          
          @Override
          public Status onDir(VfsDir dir) {
            model.add(dir);
            return VfsDir.Visitor.Status.CONTINUE;
          }
        });
        model.sort();
        return model;
      } catch (final Exception e) {
        view.post(new Runnable() {
          @Override
          public void run() {
            view.showError(e);
          }
        });
      }
      return null;
    }
  }
}