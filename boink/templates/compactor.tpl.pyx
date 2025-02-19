{# boink/templates/assembly.tpl.pyx
 # Copyright (C) 2018 Camille Scott
 # All rights reserved.
 #
 # This software may be modified and distributed under the terms
 # of the MIT license.  See the LICENSE file for details.
 #}
{% extends "base.tpl" %}
{% block code %}

from cython.operator cimport dereference as deref

from libc.stdint cimport uint64_t
from libcpp cimport bool, nullptr_t, nullptr
from libcpp.memory cimport make_shared
from libcpp.string cimport string

from boink.prometheus cimport Instrumentation
from boink.utils cimport *


cdef class StreamingCompactor:
    
    @staticmethod
    def build(dBG graph, Instrumentation instrumentation=None):
        {% for type_bundle in type_bundles %}
        if graph.storage_type == "{{type_bundle.storage_type}}" and \
           graph.shifter_type == "{{type_bundle.shifter_type}}":
            return StreamingCompactor_{{type_bundle.suffix}}(graph, instrumentation)
        {% endfor %}

        raise TypeError("Invalid dBG type.")


cdef class SolidStreamingCompactor:
    
    @staticmethod
    def build(StreamingCompactor compactor,
              unsigned int       min_abund,
              uint64_t           abund_table_size,
              uint16_t           n_abund_tables):
        {% for type_bundle in type_bundles %}
        if compactor.storage_type == "{{type_bundle.storage_type}}" and \
           compactor.shifter_type == "{{type_bundle.shifter_type}}":
            return SolidStreamingCompactor_{{type_bundle.suffix}}(compactor,
                                                                  min_abund,
                                                                  abund_table_size,
                                                                  n_abund_tables)
        {% endfor %}

        raise TypeError("Invalid dBG/StreamingCompactor type.")


{% for type_bundle in type_bundles %}
cdef class StreamingCompactor_{{type_bundle.suffix}}(StreamingCompactor):

    def __cinit__(self, dBG_{{type_bundle.suffix}} graph, Instrumentation inst=None):

        self.storage_type    = graph.storage_type
        self.shifter_type    = graph.shifter_type
        self.instrumentation = inst
        
        cdef shared_ptr[_Registry] registry
        if inst is None:
            self.instrumentation = Instrumentation('', expose=False)
        else:
            self.instrumentation = inst
        registry = self.instrumentation.registry

        if type(self) is StreamingCompactor_{{type_bundle.suffix}}:
            self._this = make_shared[_StreamingCompactor[_dBG[{{type_bundle.params}}]]](graph._this,
                                                                                        registry)
            self._graph = graph._this
            self.graph = graph # for reference counting
            self.cdbg = cDBG_{{type_bundle.suffix}}._wrap(deref(self._this).cdbg)
            self.Notifier = EventNotifier._wrap(<shared_ptr[_EventNotifier]>self._this)

    def reset(self):
        self.graph.reset()
        self._this = make_shared[_StreamingCompactor[_dBG[{{type_bundle.params}}]]](self._graph,
                                                                                    self.instrumentation.registry)
        self.cdbg = cDBG_{{type_bundle.suffix}}._wrap(deref(self._this).cdbg)
        self.Notifier = EventNotifier._wrap(<shared_ptr[_EventNotifier]>self._this)

    def find_decision_kmers(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        cdef vector[uint32_t] positions
        cdef vector[hash_t] hashes
        cdef vector[NeighborBundle] neighbors

        deref(self._this).find_decision_kmers(_sequence,
                                               positions,
                                               hashes,
                                               neighbors)

        return positions, hashes

    def update_sequence(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        deref(self._this).update_sequence(_sequence)

    def find_new_segments(self, str sequence):
        cdef string _sequence = _bstring(sequence)

        cdef deque[_compact_segment] _segments
        deref(self._this).find_new_segments(_sequence,
                                            _segments)

        segments = []
        cdef size_t i = 0
        for i in range(_segments.size()):
            if _segments[i].is_null():
                segment = Segment(sequence = '',
                                  is_decision_kmer = False,
                                  left_anchor = 0,
                                  right_anchor = 0,
                                  start = 0,
                                  length = 0,
                                  is_null = True)
            else:
                segment_seq = sequence[_segments[i].start_pos : \
                                       _segments[i].start_pos + _segments[i].length]
                segment = Segment(sequence =           segment_seq,
                                  is_decision_kmer =   _segments[i].is_decision_kmer,
                                  left_anchor =        _segments[i].left_anchor,
                                  right_anchor =       _segments[i].right_anchor,
                                  start =              _segments[i].start_pos,
                                  length =             _segments[i].length,
                                  is_null =            False)
            segments.append(segment)

        return segments

    def reverse_complement_cdbg(self):
        deref(self._this).reverse_complement_cdbg()


cdef class SolidStreamingCompactor_{{type_bundle.suffix}}(SolidStreamingCompactor):

    def __cinit__(self, StreamingCompactor_{{type_bundle.suffix}} compactor,
                        unsigned int                              min_abund,
                        uint64_t                                  abund_table_size,
                        uint16_t                                  n_abund_tables):

        self.storage_type = compactor.storage_type
        self.shifter_type = compactor.shifter_type

        if type(self) is SolidStreamingCompactor_{{type_bundle.suffix}}:
            self._this = make_shared[_SolidStreamingCompactor[_dBG[{{type_bundle.params}}]]](compactor._this,
                                                                                             min_abund,
                                                                                             abund_table_size,
                                                                                             n_abund_tables)
            self.Notifier = EventNotifier._wrap(<shared_ptr[_EventNotifier]>self._this)

    def update_sequence(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        deref(self._this).update_sequence(_sequence)

    def find_solid_segments(self, str sequence):
        cdef string _sequence = _bstring(sequence)
        cdef vector[pair[size_t, size_t]] segments = deref(self._this).find_solid_segments(_sequence)
        return segments



{% endfor %}

{% endblock code %}
