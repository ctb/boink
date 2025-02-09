# events.pxd
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

from libc.stdint cimport uint8_t, uint32_t, uint64_t
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string

cdef extern from "boink/event_types.hh" namespace "boink::events" nogil:

    ctypedef enum event_t:
        MSG_EXIT_THREAD
        MSG_TIMER

        MSG_WRITE_CDBG_STATS
        MSG_SAVE_CDBG

        MSG_ADD_DNODE
        MSG_ADD_UNODE
        MSG_DELETE_DNODE
        MSG_DELETE_UNODE
        MSG_ADD_EDGE
        MSG_INCR_DNODE_COUNT

    cdef cppclass _Event "Event":
        _Event(event_t)
        _Event(event_t, void *)


cdef extern from "boink/events.hh" namespace "boink::events" nogil:
    cdef cppclass _EventListener "boink::events::EventListener":
        _EventListener(const string&) except +RuntimeError
        
        void exit_thread()
        void notify(shared_ptr[_Event])
        void wait_on_processing(uint64_t)

    cdef cppclass _EventNotifier "boink::events::EventNotifier":
        _EventNotifier()
        void notify(shared_ptr[_Event])
        void register_listener(_EventListener*)
        void stop_listeners()
        void clear_listeners()


cdef class EventListener:
    cdef shared_ptr[_EventListener] _listener
    @staticmethod
    cdef EventListener _wrap(shared_ptr[_EventListener])


cdef class EventNotifier:
    cdef shared_ptr[_EventNotifier] _notifier
    @staticmethod
    cdef EventNotifier _wrap(shared_ptr[_EventNotifier]) 
