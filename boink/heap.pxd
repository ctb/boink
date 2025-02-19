# boink/heap.pxd
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.


cdef class Weighted:

    cdef public object item
    cdef public float weight

cdef class MinHeap:

    cdef readonly list data
    cdef readonly int n_items
    cdef int capacity

    cpdef void insert(self, Weighted item)
    cdef Weighted root(self)
    cdef int left(self, int i)
    cdef int right(self, int i)
    cdef int parent(self, int i)
    cdef void heapify(self, int i=*)
    cpdef float min(self)
