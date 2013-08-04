/*
 * @file
 * @brief Currently playing activity
 * @version $Id:$
 * @author (C) Vitamin/CAIG
 */

package app.zxtune;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import app.zxtune.playback.PlaybackService;
import app.zxtune.ui.BrowserFragment;
import app.zxtune.ui.NowPlayingFragment;
import app.zxtune.ui.PlaylistFragment;

public class MainActivity extends FragmentActivity implements PlaybackServiceConnection.Callback {
  
  private static final int QUIT_ID = Menu.FIRST;
  
  private PlaybackService service;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.main_activity);

    fillPages();
  }
  
  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    super.onCreateOptionsMenu(menu);
    
    menu.add(0, QUIT_ID, Menu.NONE, R.string.menu_quit);
    return true;
  }
  
  @Override
  public boolean onMenuItemSelected(int featureId, MenuItem item) {
    super.onMenuItemSelected(featureId, item);
    
    switch (item.getItemId()) {
      case QUIT_ID:
        quit();
        break;
    }
    return true;
  }
  
  @Override
  public void onServiceConnected(PlaybackService service) {
    this.service = service;
    for (Fragment f : getSupportFragmentManager().getFragments()) {
      if (f instanceof PlaybackServiceConnection.Callback) {
        ((PlaybackServiceConnection.Callback) f).onServiceConnected(service);
      }
    }
  }
  
  private void fillPages() { 
    final FragmentManager manager = getSupportFragmentManager();
    final FragmentTransaction transaction = manager.beginTransaction();
    if (null == manager.findFragmentById(R.id.now_playing)) {
      transaction.replace(R.id.now_playing, NowPlayingFragment.createInstance());
    }
    if (null == manager.findFragmentById(R.id.browser_view)) {
      transaction.replace(R.id.browser_view, BrowserFragment.createInstance());
    }
    if (null == manager.findFragmentById(R.id.playlist_view)) {
      transaction.replace(R.id.playlist_view, PlaylistFragment.createInstance());
    }
    PlaybackServiceConnection.register(manager, transaction);
    transaction.commit();
    final ViewPager pager = (ViewPager) findViewById(R.id.view_pager);
    if (null != pager) {
      final int childs = pager.getChildCount();
      pager.setOffscreenPageLimit(childs);
      pager.setAdapter(new Adapter(childs));
    }
  }
  
  private void quit() {
    if (service != null) {
      //TODO: service.shutdown();
      service.getPlaybackControl().stop();
      finish();
    }
  }
  
  private static class Adapter extends PagerAdapter {

    private final int count;

    public Adapter(int count) {
      this.count = count;
    }

    @Override
    public int getCount() {
      return count;
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
      return view == object;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
      return container.getChildAt(position);
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
    }
  }
}
