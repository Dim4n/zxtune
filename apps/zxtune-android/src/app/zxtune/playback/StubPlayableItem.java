/**
 * @file
 * @brief
 * @version $Id:$
 * @author
 */
package app.zxtune.playback;

import app.zxtune.ZXTune;

public class StubPlayableItem extends StubItem implements PlayableItem {
  
  private StubPlayableItem() {
  }
  
  @Override
  public ZXTune.Player createPlayer() {
    throw new IllegalStateException("Should not be called");
  }

  @Override
  public void release() {
  }

  public static PlayableItem instance() {
    return Holder.INSTANCE;
  }

  //onDemand holder idiom
  private static class Holder {
    public static final PlayableItem INSTANCE = new StubPlayableItem();
  }  
}