// -*- C++ -*-
//! \file       logger.cc
//! \date       Sat Jul 07 07:09:20 2012
//! \brief      win32 console logger.
//

#include "logger.hpp"
#include "sysio.h"
#include <iostream>
#include <string>
#include <cstdio>
#include <boost/thread/tss.hpp>
#ifdef _WIN32
#include "thread_mutex.hpp"
#include <windows.h>
#else
#include <ctime>
#include <unistd.h>
#include <sys/time.h>
#endif

namespace {

using namespace logg;
using boost::thread_specific_ptr;

class logger : public std::streambuf
{
public:
    typedef char				char_type;
    typedef std::char_traits<char_type>		traits_type;
    typedef traits_type::int_type 		int_type;

    struct line_buffer
    {
        static const size_t s_limit = 1000; // max line length

        explicit line_buffer (logger* owner) : m_owner (owner), m_default_color (-1)
            {
#ifdef _WIN32
                m_convert_cp = GetACP() != GetConsoleOutputCP();
#endif
            }

        void append (const char* buf, size_t size);
        void flush ();

    private:
        void append_time ();
        void append_crlf ();
        void set_custom_color ();

        logger*         m_owner;
        std::string     m_text;
        color_t		m_default_color;

#ifdef _WIN32
        // convert text from system ANSI code page to console code page.
        void write_to_console (const std::string& text);
        bool            m_convert_cp;
#endif
    };

    explicit logger (sys::raw_handle console, color_t color = -1)
        : m_con (console)
        , m_custom_color (color)
        , m_use_color (sys::isatty (console))
        , m_prepend_time (true)
        , m_opened_file (false)
        { }
    ~logger ()
        {
            if (m_opened_file)
                sys::close_file (m_con);
        }

    color_t get_color () const { return m_custom_color; }
    void set_color (color_t attr) { m_custom_color = attr; }

    template <typename CharT>
    bool redirect (const CharT* filename);
    void redirect (sys::raw_handle file);

protected:
    virtual int_type overflow (int_type c);
    virtual std::streamsize xsputn (const char_type* buf, std::streamsize size);

private:
    line_buffer* buffer ();
    void write_line (const char* data, size_t size);
    void write_line (const std::string& line) { write_line (line.data(), line.size()); }

private:
    sys::raw_handle	m_con;
    thread_specific_ptr<line_buffer>
                        m_buffer;
    color_t		m_custom_color;
    bool                m_use_color;
    bool                m_prepend_time;
    bool                m_opened_file;
};

inline logger::line_buffer* logger::
buffer ()
{
    line_buffer* bufptr = m_buffer.get();
    if (!bufptr)
        m_buffer.reset (bufptr = new line_buffer (this));
    return bufptr;
}

logger::int_type logger::
overflow (int_type c)
{
    if (traits_type::eq_int_type (c, traits_type::eof()))
	return traits_type::not_eof (c);

    char_type chr = traits_type::to_char_type (c);
    if (traits_type::eq (chr, '\n'))
        buffer()->flush();
    else
        buffer()->append (&chr, 1);
    return (c);
}

std::streamsize logger::
xsputn (const char_type* buf, std::streamsize sz)
{
    line_buffer* bufptr = buffer();
    for (std::streamsize size = sz; size > 0; )
    {
        const char* nl = traits_type::find (buf, size, '\n');
        if (!nl)
        {
            bufptr->append (buf, size);
            break;
        }
        if (nl != buf)
        {
            bufptr->append (buf, nl-buf);
        }
        bufptr->flush();
        ++nl;
        size -= nl - buf;
        buf = nl;
    }
    return sz;
}

void logger::line_buffer::
append (const char* buf, size_t size)
{
    if (m_owner->m_prepend_time && m_text.empty())
        append_time();
    while (size + m_text.size() > s_limit)
    {
        assert (m_text.size() < s_limit);
        size_t chunk = std::min (s_limit - m_text.size(), size);
        m_text.append (buf, chunk);
        flush();
        size -= chunk;
        buf += chunk;
        if (size && m_owner->m_prepend_time)
            append_time();
    }
    if (size)
        m_text.append (buf, size);
}

inline void logger::line_buffer::
append_crlf ()
{
#ifdef _WIN32
    m_text.append ("\r\n", 2);
#else
    m_text.push_back ('\n');
#endif
}

inline void logger::
write_line (const char* data, size_t size)
{
    sys::write_file (m_con, data, size);
}

#ifdef _WIN32

void logger::line_buffer::
append_time ()
{
    char cvtbuf[32];
    SYSTEMTIME time;
    GetLocalTime (&time);
    int rc = _snprintf (cvtbuf, sizeof(cvtbuf), "%02d:%02d:%02d.%03d [%04lu] ",
                        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                        GetCurrentThreadId());
    if (rc < 0 || rc > int (sizeof(cvtbuf)))
	rc = sizeof(cvtbuf);
    m_text.append (cvtbuf, rc);
}

void logger::line_buffer::
write_to_console (const std::string& text)
{
    auto non_ascii = std::find_if (text.begin(), text.end(),
                                   [] (char c) { return (unsigned char)c > 0x7f; });
    if (non_ascii == text.end())
    {
        m_owner->write_line (m_text);
        return;
    }
    sys::local_buffer<wchar_t> wbuf (text.size());
    int wcount = ::MultiByteToWideChar (CP_ACP, 0, text.data(), text.size(),
                                        wbuf.get(), wbuf.size());
    if (!wcount)
    {
        int err = ::GetLastError();
	if (err != ERROR_INSUFFICIENT_BUFFER)
        {
            m_owner->write_line (m_text);
            return;
        }
	wcount = ::MultiByteToWideChar (CP_ACP, 0, text.data(), text.size(),
                                        wbuf.get(), 0);
	wbuf.reserve (wcount);
	wcount = ::MultiByteToWideChar (CP_ACP, 0, text.data(), text.size(),
                                        wbuf.get(), wbuf.size());
	if (!wcount)
        {
            m_owner->write_line (m_text);
            return;
        }
    }
    sys::local_buffer<char> cbuf (wcount);
    int cp = GetConsoleOutputCP();
    int count = ::WideCharToMultiByte (cp, 0, wbuf.get(), wcount,
                                       cbuf.get(), cbuf.size(), 0, 0);
    if (!count)
    {
	int err = ::GetLastError();
	if (err != ERROR_INSUFFICIENT_BUFFER)
        {
            m_owner->write_line (m_text);
            return;
        }
	count = ::WideCharToMultiByte (cp, 0, wbuf.get(), wcount,
				       cbuf.get(), 0, 0, 0);
	cbuf.reserve (count);
	count = ::WideCharToMultiByte (cp, 0, wbuf.get(), wcount,
				       cbuf.get(), cbuf.size(), 0, 0);
	if (!count)
        {
            m_owner->write_line (m_text);
            return;
        }
    }
    m_owner->write_line (cbuf.get(), count);
}

sys::thread::mutex g_console_mutex; // XXX

void logger::line_buffer::
flush ()
{
    if (m_owner->m_use_color)
    {
        sys::thread::scoped_lock lock (g_console_mutex);
        set_custom_color();
        if (m_convert_cp)
            write_to_console (m_text);
        else
            m_owner->write_line (m_text);
        if (m_default_color != (color_t)-1)
            SetConsoleTextAttribute (m_owner->m_con, m_default_color);
        m_owner->write_line ("\r\n", 2); // shielded by scoped lock
    }
    else
    {
        append_crlf();
        m_owner->write_line (m_text);
    }
    m_text.clear();
}

void logger::line_buffer::
set_custom_color ()
{
    const color_t custom_color = m_owner->get_color();
    CONSOLE_SCREEN_BUFFER_INFO info;
    if ((color_t)-1 != custom_color
        && GetConsoleScreenBufferInfo (m_owner->m_con, &info))
    {
        m_default_color = info.wAttributes;
        SetConsoleTextAttribute (m_owner->m_con, custom_color);
    }
    else
        m_default_color = -1;
}

#else // !_WIN32

void logger::line_buffer::
append_time ()
{
    char cvtbuf[32];
    if (m_owner->m_use_color)
        set_custom_color();
    struct timeval sys_time;
    int rc = ::gettimeofday (&sys_time, NULL);
    if (rc != -1)
    {
	struct tm time;
	localtime_r (&sys_time.tv_sec, &time);
	rc = ::snprintf (cvtbuf, sizeof(cvtbuf), "%02d:%02d:%02d.%03d [%08x] ", time.tm_hour,
			 time.tm_min, time.tm_sec, int(sys_time.tv_usec/1000),
			 (unsigned) pthread_self());
	if (rc > int (sizeof(cvtbuf)))
	    rc = sizeof(cvtbuf);
    }
    if (rc != -1)
        m_text.append (cvtbuf, rc);
}

void logger::line_buffer::
flush ()
{
    if (!m_text.empty() && m_default_color != (color_t)-1)
        m_text.append ("\033[0m", 4);
    append_crlf();
    m_owner->write_line (m_text);
    m_text.clear();
}

void logger::line_buffer::
set_custom_color ()
{
    const color_t custom_color = m_owner->get_color();
    if ((color_t)-1 == custom_color)
    {
        m_default_color = -1;
        return;
    }
    m_default_color = custom_color;

    const size_t seq_start = m_text.size();
    m_text.append ("\033[", 2);
    const char* foreground = NULL;
    switch (custom_color & logg::fg_color)
    {
    case logg::fg_black:	foreground = "30"; break;
    case logg::fg_red:	        foreground = "31"; break;
    case logg::fg_green:	foreground = "32"; break;
    case logg::fg_yellow:	foreground = "33"; break;
    case logg::fg_blue:	        foreground = "34"; break;
    case logg::fg_magenta:	foreground = "35"; break;
    case logg::fg_cyan:	        foreground = "36"; break;
    case logg::fg_white:	foreground = "37"; break;
    }
    if (foreground)
        m_text += foreground;
    const char* background = NULL;
    switch (custom_color & logg::bg_color)
    {
    case logg::bg_black:	background = "40"; break;
    case logg::bg_red:	        background = "41"; break;
    case logg::bg_green:	background = "42"; break;
    case logg::bg_yellow:	background = "43"; break;
    case logg::bg_blue:	        background = "44"; break;
    case logg::bg_magenta:	background = "45"; break;
    case logg::bg_cyan:	        background = "46"; break;
    case logg::bg_white:	background = "47"; break;
    }
    if (background)
    {
        if (m_text.size()-seq_start != 2)
            m_text += ';';
        m_text += background;
    }
    if (custom_color & (logg::fg_bright|logg::bg_bright))
    {
        if (m_text.size()-seq_start != 2)
            m_text += ';';
        m_text += '1';
    }
    if (m_text.size()-seq_start > 2)
        m_text += 'm';
    else
    {
        m_text.erase (seq_start);
        m_default_color = -1;
    }
}

#endif // _WIN32

template <typename CharT> bool logger::
redirect (const CharT* filename)
{
    sys::file_handle file (sys::create_file (filename, sys::io::posix_to_sys<O_APPEND|O_CREAT>(),
                                             sys::io::share_all));
    if (!file)
        return false;
    buffer()->flush();
    if (m_opened_file)
	sys::close_file (m_con);
    m_con = file.release();
#ifdef _WIN32
    SetFilePointer (m_con, 0, 0, FILE_END);
#endif
    m_use_color = false;
    m_opened_file = true;
    return true;
}

void logger::
redirect (sys::raw_handle file)
{
    buffer()->flush();
    if (m_opened_file)
        sys::close_file (m_con);
    m_con = file;
    m_opened_file = false;
#ifdef _WIN32
    switch (GetFileType (file))
    {
    case FILE_TYPE_CHAR:
        m_use_color = true;
        break;
    case FILE_TYPE_DISK:
        SetFilePointer (file, 0, 0, FILE_END);
        /* FALL-THROUGH */
    default:
        m_use_color = false;
        break;
    }
#else
    m_use_color = isatty (file);
    if (!m_use_color)
	::lseek (file, 0, SEEK_END);
#endif
}

// ---------------------------------------------------------------------------

class logger_init
{
public:
    logger_init ();
    ~logger_init ();

    void set_clog_color (color_t color) { if (m_clog) m_clog->set_color (color); }
    void set_cerr_color (color_t color) { if (m_cerr) m_cerr->set_color (color); }

private:
    logger*     	m_clog;
    logger*	        m_cerr;
    std::streambuf*     m_clog_native;
    std::streambuf*     m_cerr_native;
};

logger_init::logger_init ()
    : m_clog (0)
    , m_cerr (0)
    , m_clog_native (0)
    , m_cerr_native (0)
{
    sys::raw_handle con = sys::io::err();
    if (!sys::handle::valid (con) || !sys::file_handle::valid (con))
        return;
    m_clog = new logger (con, logg::fg_white|logg::fg_bright);
    m_cerr = new logger (con, logg::bg_red|logg::fg_white|logg::fg_bright);
    m_clog_native = std::clog.rdbuf (m_clog);
    m_cerr_native = std::cerr.rdbuf (m_cerr);
}

logger_init::~logger_init ()
{
    if (m_clog)
    {
        std::cerr.rdbuf (m_cerr_native);
        std::clog.rdbuf (m_clog_native);
        delete m_cerr;
        delete m_clog;
    }
}

logger_init std_stream_logger;

} // anonymous namespace

namespace logg {

void set_clog_color (color_t color)
{
    std_stream_logger.set_clog_color (color);
}

void set_cerr_color (color_t color)
{
    std_stream_logger.set_cerr_color (color);
}

} // namespace logg
