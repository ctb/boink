# boink/processors.pxd
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

from libc.stdint cimport uint16_t, uint32_t, uint64_t, int64_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.memory cimport shared_ptr

from boink.dbg cimport *
from boink.cdbg cimport *
from boink.compactor cimport *
from boink.events cimport EventNotifier, _EventNotifier, _EventListener
from boink.parsing cimport _ReadParser, _FastxReader, _SplitPairedReader
from boink.utils cimport _bstring
from boink.minimizers cimport _UKHSCountSignature

from sourmash._minhash cimport KmerMinHash


cdef extern from "boink/processors.hh":
    cdef uint64_t DEFAULT_FINE_INTERVAL
    cdef uint64_t DEFAULT_MEDIUM_INTERVAL
    cdef uint64_t DEFAULT_COARSE_INTERVAL


cdef extern from "boink/processors.hh" namespace "boink" nogil:

    cdef cppclass _FileProcessor "boink::FileProcessor" [Derived] (_EventNotifier):
        _FileProcessor(uint64_t, uint64_t, uint64_t)
        _FileProcessor()

        cppclass interval_state:
            bool fine
            bool medium
            bool coarse
            bool end

        uint64_t process(...) except +ValueError
        uint64_t process(const string&) except +ValueError
        uint64_t process_paired "process"(const string&, const string&) except +ValueError
        uint64_t process(const string&,
                         const string&,
                         uint32_t,
                         bool) except +ValueError

        interval_state advance_paired "advance" (_SplitPairedReader[_FastxReader]&) except +ValueError
        interval_state advance(shared_ptr[_ReadParser[_FastxReader]]&) except +ValueError

        uint64_t n_reads() const

    cdef cppclass _FileConsumer "boink::FileConsumer" [GraphType] (_FileProcessor[_FileConsumer[GraphType]]):
        _FileConsumer(GraphType *,
                      uint64_t,
                      uint64_t,
                      uint64_t)
        uint64_t n_consumed()


    cdef cppclass _UKHSCountSignatureProcessor "boink::UKHSCountSignatureProcessor" (_FileProcessor[_UKHSCountSignatureProcessor]):
        _UKHSCountSignatureProcessor(shared_ptr[_UKHSCountSignatureProcessor],
                                     uint64_t,
                                     uint64_t,
                                     uint64_t)

    cdef cppclass _SourmashSignatureProcessor "boink::SourmashSignatureProcessor" (_FileProcessor[_SourmashSignatureProcessor]):
        _SourmashSignatureProcessor(KmerMinHash *,
                                    uint64_t,
                                    uint64_t,
                                    uint64_t)

    cdef cppclass _DecisionNodeProcessor "boink::DecisionNodeProcessor"[GraphType] (_FileProcessor[_DecisionNodeProcessor[GraphType]]):
        _DecisionNodeProcessor(_StreamingCompactor*,
                               string &,
                               uint64_t,
                               uint64_t,
                               uint64_t)

    cdef cppclass _StreamingCompactorProcessor "boink::StreamingCompactorProcessor"[GraphType](_FileProcessor[_StreamingCompactorProcessor[GraphType]]):
        _StreamingCompactorProcessor(shared_ptr[_StreamingCompactor[GraphType]],
                                     uint64_t,
                                     uint64_t,
                                     uint64_t)

    cdef cppclass _MinimizerProcessor "boink::MinimizerProcessor" [ShifterType] (_FileProcessor[_MinimizerProcessor[ShifterType]]):
        _MinimizerProcessor(int32_t,
                            uint16_t,
                            const string&,
                            uint64_t,
                            uint64_t,
                            uint64_t)

cdef extern from "boink/normalization/diginorm.hh" namespace "boink::normalization" nogil:
    cdef cppclass _NormalizingCompactor "boink::normalization::NormalizingCompactor"[GraphType](_FileProcessor[_NormalizingCompactor[GraphType]]):
        _NormalizingCompactor(shared_ptr[_StreamingCompactor[GraphType]],
                              unsigned int,
                              uint64_t,
                              uint64_t,
                              uint64_t)


cdef class FileProcessor:
    cdef public EventNotifier Notifier

cdef class MinimizerProcessor(FileProcessor):
    cdef shared_ptr[_MinimizerProcessor[_RollingHashShifter]] _mp_this

cdef class UKHSCountSignatureProcessor(FileProcessor):
    cdef shared_ptr[_UKHSCountSignatureProcessor] _this

cdef class SourmashSignatureProcessor(FileProcessor):
    cdef shared_ptr[_SourmashSignatureProcessor] _this



include "processors.tpl.pxd.pxi"
