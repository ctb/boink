# processors.pyx
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

from cython.operator cimport dereference as deref
from libcpp.memory cimport make_shared

from boink.dbg cimport *
from boink.cdbg cimport *
from boink.utils cimport _bstring, _ustring
from boink.minimizers cimport UKHSCountSignature
from boink.parsing cimport get_parser

from sourmash._minhash cimport MinHash


class DEFAULT_INTERVALS:
    FINE   = DEFAULT_FINE_INTERVAL
    MEDIUM = DEFAULT_MEDIUM_INTERVAL
    COARSE = DEFAULT_COARSE_INTERVAL


cdef class FileProcessor:

    def __cinit__(self, *args, **kwargs):
        pass

include "processors.tpl.pyx.pxi"

cdef class MinimizerProcessor(FileProcessor):

    def __cinit__(self, int64_t window_size,
                        uint16_t ksize,
                        str output_filename,
                        uint64_t fine_interval,
                        uint64_t medium_interval,
                        uint64_t coarse_interval):

        cdef string _output_filename = _bstring(output_filename)
        self._mp_this = make_shared[_MinimizerProcessor[_RollingHashShifter]](window_size,
                                                                         ksize,
                                                                         _output_filename,
                                                                         fine_interval,
                                                                         medium_interval,
                                                                         coarse_interval)
    def process(self, str input_filename):
        deref(self._mp_this).process(_bstring(input_filename))

        return deref(self._mp_this).n_reads()


cdef class UKHSCountSignatureProcessor(FileProcessor):

    def __cinit__(self, UKHSCountSignature signature,
                        uint64_t fine_interval,
                        uint64_t medium_interval,
                        uint64_t coarse_interval):

        self._this = make_shared[_UKHSCountSignatureProcessor](signature._this,
                                                               fine_interval,
                                                               medium_interval,
                                                               coarse_interval)
        self.Notifier = EventNotifier._wrap(<shared_ptr[_EventNotifier]>self._this)

    def process(self, str input_filename, str right_filename=None):
        if right_filename is None:
            deref(self._this).process(_bstring(input_filename))
        else:
            deref(self._this).process(_bstring(input_filename),
                                      _bstring(right_filename))

    def chunked_process(self, str input_filename, str right_filename=None):
        cdef shared_ptr[_ReadParser[_FastxReader]]       p_single
        cdef _SplitPairedReader[_FastxReader] *          p_paired
        cdef _UKHSCountSignatureProcessor.interval_state state
        
        if right_filename is None:
            p_single = get_parser[_FastxReader](_bstring(input_filename))
            while True:
                state = deref(self._this).advance(p_single)
                if state.end:
                    return deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
                else:
                    yield deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
        else:
            p_paired = new _SplitPairedReader[_FastxReader](_bstring(input_filename),
                                                            _bstring(right_filename))

            while True:
                state = deref(self._this).advance_paired(deref(p_paired))
                if state.end:
                    return deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
                else:
                    yield deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end


cdef class SourmashSignatureProcessor(FileProcessor):

    def __cinit__(self, MinHash signature,
                        uint64_t fine_interval,
                        uint64_t medium_interval,
                        uint64_t coarse_interval):

        self._this = make_shared[_SourmashSignatureProcessor](signature._this.get(),
                                                              fine_interval,
                                                              medium_interval,
                                                              coarse_interval)
        self.Notifier = EventNotifier._wrap(<shared_ptr[_EventNotifier]>self._this)

    def process(self, str input_filename, str right_filename=None):
        if right_filename is None:
            deref(self._this).process(_bstring(input_filename))
        else:
            deref(self._this).process(_bstring(input_filename),
                                      _bstring(right_filename))

    def chunked_process(self, str input_filename, str right_filename=None):
        cdef shared_ptr[_ReadParser[_FastxReader]]      p_single
        cdef _SplitPairedReader[_FastxReader] *         p_paired
        cdef _SourmashSignatureProcessor.interval_state state
        
        if right_filename is None:
            p_single = get_parser[_FastxReader](_bstring(input_filename))
            while True:
                state = deref(self._this).advance(p_single)
                if state.end:
                    return deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
                else:
                    yield deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
        else:
            p_paired = new _SplitPairedReader[_FastxReader](_bstring(input_filename),
                                                            _bstring(right_filename))

            while True:
                state = deref(self._this).advance_paired(deref(p_paired))
                if state.end:
                    return deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
                else:
                    yield deref(self._this).n_reads(), state.fine, state.medium, state.coarse, state.end
