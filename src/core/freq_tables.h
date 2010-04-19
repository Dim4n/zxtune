/**
*
* @file     core/freq_tables.h
* @brief    Frequency tables for AY-trackers
* @version  $Id$
* @author   (C) Vitamin/CAIG/2001
*
**/

#ifndef __CORE_FREQ_TABLES_H_DEFINED__
#define __CORE_FREQ_TABLES_H_DEFINED__

//common includes
#include <types.h>
//boost includes
#include <boost/array.hpp>

namespace ZXTune
{
  namespace Module
  {
    //! @brief Frequency table type- 96 words
    typedef boost::array<uint16_t, 96> FrequencyTable;
    
    //! %Sound Tracker frequency table
    const Char TABLE_SOUNDTRACKER[] = {'S','o','u','n','d','T','r','a','c','k','e','r','\0'};
    //! Pro Tracker v2.x frequency table
    const Char TABLE_PROTRACKER2[] = {'P','r','o','T','r','a','c','k','e','r','2','\0'};
    //! Pro Tracker v3.3 native frequency table
    const Char TABLE_PROTRACKER3_3[] = {'P','r','o','T','r','a','c','k','e','r','3','.','3','\0'};
    //! Pro Tracker v3.4 native frequency table
    const Char TABLE_PROTRACKER3_4[] = {'P','r','o','T','r','a','c','k','e','r','3','.','4','\0'};
    //! Pro Tracker v3.3 ASM frequency table
    const Char TABLE_PROTRACKER3_3_ASM[] = {'P','r','o','T','r','a','c','k','e','r','3','.','3','_','A','S','M','\0'};
    //! Pro Tracker v3.4 ASM frequency table
    const Char TABLE_PROTRACKER3_4_ASM[] = {'P','r','o','T','r','a','c','k','e','r','3','.','4','_','A','S','M','\0'};
    //! Pro Tracker v3.3 real frequency table
    const Char TABLE_PROTRACKER3_3_REAL[] = {'P','r','o','T','r','a','c','k','e','r','3','.','3','_','R','e','a','l','\0'};
    //! Pro Tracker v3.4 real frequency table
    const Char TABLE_PROTRACKER3_4_REAL[] = {'P','r','o','T','r','a','c','k','e','r','3','.','4','_','R','e','a','l','\0'};
    //! ASC %Sound Master frequency table
    const Char TABLE_ASM[] = {'A', 'S', 'M', '\0'};
    //! %Sound Tracker Pro frequency table
    const Char TABLE_SOUNDTRACKER_PRO[] = {'S','o','u','n','d','T','r','a','c','k','e','r','P','r','o','\0'};
  }
}

#endif //__CORE_FREQ_TABLES_H_DEFINED__
