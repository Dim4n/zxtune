/**
 * 
 * @file
 *
 * @brief
 *
 * @author vitamin.caig@gmail.com
 * 
 */

package app.zxtune.fs.dbhelpers;

import java.io.IOException;

public interface QueryCommand {

  Timestamps.Lifetime getLifetime();
  
  Transaction startTransaction() throws IOException;

  boolean queryFromCache();

  void queryFromRemote() throws IOException;
}
