// Generated from '$Id$'
#ifndef __TEXT_BACKENDS_H_DEFINED__
#define __TEXT_BACKENDS_H_DEFINED__
#include <char_type.h>

namespace Text
{
extern const Char ALSA_BACKEND_DEFAULT_DEVICE[] = {
  'D','e','f','a','u','l','t',0
};
extern const Char ALSA_BACKEND_DESCRIPTION[] = {
  'A','L','S','A',' ','s','o','u','n','d',' ','s','y','s','t','e','m',' ','b','a','c','k','e','n','d',0
};
extern const Char AYLPT_BACKEND_DESCRIPTION[] = {
  'A','Y','L','P','T',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d',0
};
extern const Char DSOUND_BACKEND_DESCRIPTION[] = {
  'D','i','r','e','c','t','S','o','u','n','d',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d',
  '.',0
};
extern const Char FILE_BACKEND_DEFAULT_COMMENT[] = {
  'C','r','e','a','t','e','d',' ','u','s','i','n','g',' ','Z','X','T','u','n','e',' ','t','o','o','l','k','i',
  't',0
};
extern const Char FLAC_BACKEND_DESCRIPTION[] = {
  'F','L','A','C',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d','.',0
};
extern const Char MP3_BACKEND_DESCRIPTION[] = {
  'M','P','3',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d','.',0
};
extern const Char NULL_BACKEND_DESCRIPTION[] = {
  'N','u','l','l',' ','o','u','t','p','u','t',' ','b','a','c','k','e','n','d',0
};
extern const Char OGG_BACKEND_AUTHOR_TAG[] = {
  'A','R','T','I','S','T',0
};
extern const Char OGG_BACKEND_COMMENT_TAG[] = {
  'C','O','M','M','E','N','T',0
};
extern const Char OGG_BACKEND_DESCRIPTION[] = {
  'O','G','G',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d','.',0
};
extern const Char OGG_BACKEND_TITLE_TAG[] = {
  'T','I','T','L','E',0
};
extern const Char OSS_BACKEND_DESCRIPTION[] = {
  'O','S','S',' ','s','o','u','n','d',' ','s','y','s','t','e','m',' ','b','a','c','k','e','n','d',0
};
extern const Char SDL_BACKEND_DESCRIPTION[] = {
  'S','D','L',' ','s','u','p','p','o','r','t',' ','b','a','c','k','e','n','d',0
};
extern const Char SOUND_ERROR_ALSA_BACKEND_DEVICE_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','A','L','S','A',' ','b','a','c','k','e','n','d',' ','w','h','i','l','e',
  ' ','w','o','r','k','i','n','g',' ','w','i','t','h',' ','d','e','v','i','c','e',' ','\'','%','1','%','\'',':',
  ' ','%','2','%','.',0
};
extern const Char SOUND_ERROR_ALSA_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','A','L','S','A',' ','b','a','c','k','e','n','d',':',' ','%','1','%','.',0
};
extern const Char SOUND_ERROR_ALSA_BACKEND_INVALID_BUFFERS[] = {
  'A','L','S','A',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','b','u','f','f','e','r','s',
  ' ','c','o','u','n','t',' ','(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g',
  'e',' ','(','%','2','%','.','.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_ALSA_BACKEND_NO_MIXER[] = {
  'F','a','i','l','e','d',' ','t','o',' ','f','o','u','n','d',' ','m','i','x','e','r',' ','\'','%','1','%','\'',
  ' ','f','o','r',' ','A','L','S','A',' ','b','a','c','k','e','n','d','.',0
};
extern const Char SOUND_ERROR_AYLPT_BACKEND_DUMP[] = {
  'S','p','e','c','i','f','i','e','d',' ','m','o','d','u','l','e',' ','d','o','e','s',' ','n','o','t',' ','s',
  'u','p','p','o','r','t','s',' ','d','u','m','p','i','n','g',' ','t','o',' ','A','Y','-','L','P','T',' ','d',
  'e','v','i','c','e','.',0
};
extern const Char SOUND_ERROR_DSOUND_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','D','i','r','e','c','t','S','o','u','n','d',' ','b','a','c','k','e','n',
  'd',':',' ','%','1','%','.',0
};
extern const Char SOUND_ERROR_DSOUND_BACKEND_INVALID_LATENCY[] = {
  'D','i','r','e','c','t','S','o','u','n','d',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ',
  'l','a','t','e','n','c','y',' ','(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n',
  'g','e',' ','(','%','2','%','.','.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_FILE_BACKEND_NO_FILENAME[] = {
  'O','u','t','p','u','t',' ','f','i','l','e','n','a','m','e',' ','t','e','m','p','l','a','t','e',' ','i','s',
  ' ','n','o','t',' ','s','p','e','c','i','f','i','e','d','.',0
};
extern const Char SOUND_ERROR_FLAC_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','f','l','a','c',' ','b','a','c','k','e','n','d','.',0
};
extern const Char SOUND_ERROR_MP3_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','M','P','3',' ','b','a','c','k','e','n','d',':',' ','%','1','%','.',0
};
extern const Char SOUND_ERROR_MP3_BACKEND_INVALID_BITRATE[] = {
  'M','P','3',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','b','i','t','r','a','t','e',' ',
  '(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g','e',' ','(','%','2','%','.',
  '.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_MP3_BACKEND_INVALID_MODE[] = {
  'M','P','3',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','i','n','v','a','l','i','d',' ',
  'm','o','d','e',' ','\'','%','1','%','\'','.',0
};
extern const Char SOUND_ERROR_MP3_BACKEND_INVALID_QUALITY[] = {
  'M','P','3',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','q','u','a','l','i','t','y',' ',
  '(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g','e',' ','(','%','2','%','.',
  '.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_OGG_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','M','P','3',' ','b','a','c','k','e','n','d',':',' ','%','1','%','.',0
};
extern const Char SOUND_ERROR_OGG_BACKEND_INVALID_BITRATE[] = {
  'O','G','G',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','b','i','t','r','a','t','e',' ',
  '(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g','e',' ','(','%','2','%','.',
  '.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_OGG_BACKEND_INVALID_MODE[] = {
  'O','G','G',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','i','n','v','a','l','i','d',' ',
  'm','o','d','e',' ','\'','%','1','%','\'','.',0
};
extern const Char SOUND_ERROR_OGG_BACKEND_INVALID_QUALITY[] = {
  'O','G','G',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','q','u','a','l','i','t','y',' ',
  '(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g','e',' ','(','%','2','%','.',
  '.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_OSS_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','O','S','S',' ','b','a','c','k','e','n','d',' ','w','h','i','l','e',' ',
  'w','o','r','k','i','n','g',' ','w','i','t','h',' ','d','e','v','i','c','e',' ','\'','%','1','%','\'',':',' ',
  '%','2','%','.',0
};
extern const Char SOUND_ERROR_SDL_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','S','D','L',' ','b','a','c','k','e','n','d',':',' ','%','1','%','.',0
};
extern const Char SOUND_ERROR_SDL_BACKEND_INVALID_BUFFERS[] = {
  'S','D','L',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','b','u','f','f','e','r','s',' ',
  'c','o','u','n','t',' ','(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n','g','e',
  ' ','(','%','2','%','.','.','%','3','%',')','.',0
};
extern const Char SOUND_ERROR_SDL_BACKEND_UNKNOWN_ERROR[] = {
  'U','n','k','n','o','w','n',' ','e','r','r','o','r',' ','i','n',' ','S','D','L',' ','b','a','c','k','e','n',
  'd','.',0
};
extern const Char SOUND_ERROR_WIN32_BACKEND_ERROR[] = {
  'E','r','r','o','r',' ','i','n',' ','w','i','n','3','2',' ','b','a','c','k','e','n','d',':',' ','%','1','%',
  '.',0
};
extern const Char SOUND_ERROR_WIN32_BACKEND_INVALID_BUFFERS[] = {
  'W','i','n','3','2',' ','b','a','c','k','e','n','d',' ','e','r','r','o','r',':',' ','b','u','f','f','e','r',
  's',' ','c','o','u','n','t',' ','(','%','1','%',')',' ','i','s',' ','o','u','t',' ','o','f',' ','r','a','n',
  'g','e',' ','(','%','2','%','.','.','%','3','%',')','.',0
};
extern const Char WAV_BACKEND_DESCRIPTION[] = {
  'W','A','V','-','o','u','t','p','u','t',' ','b','a','c','k','e','n','d',0
};
extern const Char WIN32_BACKEND_DESCRIPTION[] = {
  'W','i','n','3','2',' ','s','o','u','n','d',' ','s','y','s','t','e','m',' ','b','a','c','k','e','n','d',0
};
}//namespace Text
#endif //__TEXT_BACKENDS_H_DEFINED__
