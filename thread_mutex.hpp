// -*- C++ -*-
//! \file       thread_mutex.hpp
//! \date       Thu Jul 05 22:56:55 2012
//! \brief      wrapper around win32 critical sections API
//

#ifndef THREAD_MUTEX_HPP
#define THREAD_MUTEX_HPP

#include "syserror.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace sys {
namespace thread {

#ifdef _WIN32
typedef HANDLE handle;
#else
typedef pthread_t handle;
#endif

#ifdef _WIN32
class mutex
{
public:
    mutex () { InitializeCriticalSection (&m_section); }
    ~mutex () { DeleteCriticalSection (&m_section); }

    void lock () { EnterCriticalSection (&m_section); }
    bool try_lock () { return TryEnterCriticalSection (&m_section); }
    void unlock () { LeaveCriticalSection (&m_section); }

private:
    CRITICAL_SECTION		m_section;
};

#else

class mutex
{
public:
    mutex ()
       	{
	    attr mutex_attr;
            mutex_attr.settype (PTHREAD_MUTEX_RECURSIVE_NP);
            if (int rc = pthread_mutex_init (&m_mutex, &mutex_attr.m_attr))
		throw sys::generic_error (rc, "pthread_mutex_init");
       	}
    ~mutex () { pthread_mutex_destroy (&m_mutex); }

    void lock ()
        {
            if (int rc = pthread_mutex_lock (&m_mutex))
                throw sys::generic_error (rc, "pthread_mutex_lock");
        }
    bool try_lock () { return 0 == pthread_mutex_trylock (&m_mutex); }
    void unlock () { pthread_mutex_unlock (&m_mutex); }

    struct attr
    {
        pthread_mutexattr_t m_attr;
        attr () { pthread_mutexattr_init (&m_attr); }
        ~attr () { pthread_mutexattr_destroy (&m_attr); }

        int settype (int type) { return pthread_mutexattr_settype (&m_attr, type); }
    };

private:
    pthread_mutex_t		m_mutex;
};

#endif // _WIN32

class scoped_lock
{
public:
    scoped_lock (mutex& object) : m_mutex (object) { m_mutex.lock(); }
    ~scoped_lock () { m_mutex.unlock(); }

private:
    mutex&	    m_mutex;
};

class scoped_try_lock
{
public:
    scoped_try_lock (mutex& object)
        : m_mutex (object)
        , m_own (m_mutex.try_lock())
        { }
    ~scoped_try_lock () { if (m_own) m_mutex.unlock(); }
    bool owns_lock () const { return m_own; }

private:
    mutex&	    m_mutex;
    const bool      m_own;
};

} // namespace thread
} // namespace sys

#endif /* THREAD_MUTEX_HPP */
