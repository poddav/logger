// -*- C++ -*-
//! \file       logger.hpp
//! \date       Sun Aug 05 08:08:57 2012
//! \brief      simple console logger.
//

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>

#ifndef EXT_LOG_LEVEL
#define EXT_LOG_LEVEL logg::info
#endif

#define EXT_LOG_COLOR (logg::fg_white | logg::fg_bright)
#define EXT_ERR_COLOR (logg::bg_red | logg::fg_white | logg::fg_bright)

#define EXT_CLOG_IF(level) \
        if (!EXT_CLOG_ACTIVE (level)); \
        else (EXT_CLOG_STREAM (level))

#define EXT_CERR_IF(level) \
        if (!EXT_CERR_ACTIVE (level)); \
        else (EXT_CERR_STREAM (level))

#define EXT_CLOG_ACTIVE(level) (logg::is_clog_active (logg::level))
#define EXT_CLOG_STREAM(level) (logg::clog_stream (logg::level))
#define EXT_CERR_ACTIVE(level) (logg::is_cerr_active (logg::level))
#define EXT_CERR_STREAM(level) (logg::cerr_stream (logg::level))

#define LTRACE_ EXT_CLOG_IF (trace)
#define LDEBUG_ EXT_CLOG_IF (debug)
#define LINFO_  EXT_CLOG_IF (info)
#define LWARN_  EXT_CERR_IF (warn)
#define LERR_   EXT_CERR_IF (error)
#define LCRIT_  EXT_CERR_IF (crit)

namespace logg {

enum level {
    trace, debug, info, warn, error, crit,
    last = crit,
    everything = trace,
};

typedef unsigned short color_t; // doesn't matter if 'short' type size doesn't match win32 WORD

inline bool is_clog_active (level lv) { return lv >= EXT_LOG_LEVEL; }
inline bool is_cerr_active (level lv) { return lv >= EXT_LOG_LEVEL; }

inline std::ostream& clog_stream (level lv) { return std::clog; }
inline std::ostream& cerr_stream (level lv) { return std::cerr; }

void set_clog_color (color_t color);
void set_cerr_color (color_t color);

enum console_color
{
    fg_black    = 0,
    fg_blue     =               0x0001,
    fg_green    =        0x0002,
    fg_cyan     =        0x0002|0x0001,
    fg_red      = 0x0004,
    fg_magenta  = 0x0004|       0x0001,
    fg_yellow   = 0x0004|0x0002,
    fg_white    = 0x0004|0x0002|0x0001,
    fg_bright   = 0x0008,
    fg_color	= fg_white,
    bg_black    = 0,
    bg_blue     =               0x0010,
    bg_green    =        0x0020,
    bg_cyan     =        0x0020|0x0010,
    bg_red      = 0x0040,
    bg_magenta  = 0x0040|       0x0010,
    bg_yellow   = 0x0040|0x0020,
    bg_white    = 0x0040|0x0020|0x0010,
    bg_bright   = 0x0080,
    bg_color	= bg_white,
};

} // namespace logg

#endif /* LOGGER_HPP */
