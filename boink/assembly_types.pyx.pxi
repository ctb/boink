from cython.operator cimport dereference as deref

from libc.stdint cimport uint64_t
from libcpp.string cimport string

from boink import dbg
from boink.utils cimport *



cdef class Assembler_BitStorage_DefaultShifter(Assembler_Base):

    def __cinit__(self, dBG_BitStorage_DefaultShifter graph):
        if type(self) is Assembler_BitStorage_DefaultShifter:
            self._this = make_shared[_AssemblerMixin[_dBG[BitStorage,DefaultShifter]]](graph._this)
            self._graph = graph._this
            self.Graph = graph
        self.storage_type = graph.storage_type
        self.shifter_type = graph.shifter_type

    @property
    def cursor(self):
        return deref(self._this).get_cursor()

    def clear_seen(self):
        deref(self._this).clear_seen()

    def degree_left(self):
        return deref(self._this).degree_left()

    def degree_right(self):
        return deref(self._this).degree_right()

    def degree(self):
        return deref(self._this).degree()

    def assemble(self, str seed):
        cdef bytes _seed = _bstring(seed)
        cdef Path path

        deref(self._this).assemble(_seed, path)
        return deref(self._this).to_string(path)


cdef class Assembler_NibbleStorage_DefaultShifter(Assembler_Base):

    def __cinit__(self, dBG_NibbleStorage_DefaultShifter graph):
        if type(self) is Assembler_NibbleStorage_DefaultShifter:
            self._this = make_shared[_AssemblerMixin[_dBG[NibbleStorage,DefaultShifter]]](graph._this)
            self._graph = graph._this
            self.Graph = graph
        self.storage_type = graph.storage_type
        self.shifter_type = graph.shifter_type

    @property
    def cursor(self):
        return deref(self._this).get_cursor()

    def clear_seen(self):
        deref(self._this).clear_seen()

    def degree_left(self):
        return deref(self._this).degree_left()

    def degree_right(self):
        return deref(self._this).degree_right()

    def degree(self):
        return deref(self._this).degree()

    def assemble(self, str seed):
        cdef bytes _seed = _bstring(seed)
        cdef Path path

        deref(self._this).assemble(_seed, path)
        return deref(self._this).to_string(path)


cdef class Assembler_ByteStorage_DefaultShifter(Assembler_Base):

    def __cinit__(self, dBG_ByteStorage_DefaultShifter graph):
        if type(self) is Assembler_ByteStorage_DefaultShifter:
            self._this = make_shared[_AssemblerMixin[_dBG[ByteStorage,DefaultShifter]]](graph._this)
            self._graph = graph._this
            self.Graph = graph
        self.storage_type = graph.storage_type
        self.shifter_type = graph.shifter_type

    @property
    def cursor(self):
        return deref(self._this).get_cursor()

    def clear_seen(self):
        deref(self._this).clear_seen()

    def degree_left(self):
        return deref(self._this).degree_left()

    def degree_right(self):
        return deref(self._this).degree_right()

    def degree(self):
        return deref(self._this).degree()

    def assemble(self, str seed):
        cdef bytes _seed = _bstring(seed)
        cdef Path path

        deref(self._this).assemble(_seed, path)
        return deref(self._this).to_string(path)


cdef object _make_assembler(dBG_Base graph):
    if graph.storage_type == "BitStorage" and graph.shifter_type == "DefaultShifter":
        return Assembler_BitStorage_DefaultShifter(graph)
    elif graph.storage_type == "NibbleStorage" and graph.shifter_type == "DefaultShifter":
        return Assembler_NibbleStorage_DefaultShifter(graph)
    elif graph.storage_type == "ByteStorage" and graph.shifter_type == "DefaultShifter":
        return Assembler_ByteStorage_DefaultShifter(graph)
    else:
        raise TypeError("Invalid dBG type.")