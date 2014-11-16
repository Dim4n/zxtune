/**
 *
 * @file
 *
 * @brief Real sorted file browser model
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.ui;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import app.zxtune.R;
import app.zxtune.fs.DefaultComparator;
import app.zxtune.fs.VfsDir;
import app.zxtune.fs.VfsFile;
import app.zxtune.fs.VfsObject;

//TODO: synchronize?
class RealBrowserViewModel implements BrowserViewModel {
  
  private final LayoutInflater inflater;
  private final ArrayList<VfsDir> dirs;
  private final ArrayList<VfsFile> files;
  
  RealBrowserViewModel(Context context) {
    this.inflater = LayoutInflater.from(context);
    this.dirs = new ArrayList<VfsDir>();
    this.files = new ArrayList<VfsFile>();
  }
  
  final void add(VfsDir dir) {
    dirs.add(dir);
  }
  
  final void add(VfsFile file) {
    files.add(file);
  }
  
  final void sort() {
    sort(DefaultComparator.instance());
  }
  
  final void sort(Comparator<VfsObject> comparator) {
    Collections.sort(dirs, comparator);
    Collections.sort(files, comparator);
  }
  
  @Override
  public int getCount() {
    return dirs.size() + files.size();
  }

  @Override
  public boolean isEmpty() {
    return dirs.isEmpty() && files.isEmpty();
  }
    
  @Override
  public Object getItem(int position) {
    final int dirsCount = dirs.size();
    if (position < dirsCount) {
      return dirs.get(position);
    } else {
      return files.get(position - dirsCount);
    }
  }

  @Override
  //TODO: use explicit multiple view types
  public View getView(int position, View convertView, ViewGroup parent) {
    View result;
    BrowserViewHolder holder;
    if (convertView == null) {
      result = inflater.inflate(R.layout.browser_item_detailed, parent, false);
      holder = new BrowserViewHolder(result);
      result.setTag(holder);
    } else {
      result = convertView;
      holder = (BrowserViewHolder) convertView.getTag();
    }

    final int dirsCount = dirs.size();
    if (position < dirsCount) {
      holder.makeView(dirs.get(position));
    } else {
      holder.makeView(files.get(position - dirsCount));
    }

    return result;
  }
}