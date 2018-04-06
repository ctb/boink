# boink/tests/test_assembly.py
# Copyright (C) 2018 Camille Scott
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the MIT license.  See the LICENSE file for details.

import pytest

from boink.dbg import make_dBG
from boink.assembly import make_assembler
from boink.tests.utils import *


@pytest.fixture
def asm(request, ksize, graph, graph_type):
    _graph_type, AdapterType = graph_type

    return make_assembler(graph)


def test_assembler_type(asm):

    assert asm.storage_type == asm.Graph.storage_type
    assert asm.shifter_type == asm.Graph.shifter_type

    _, _, asm_suffix = type(asm).__name__.partition('_')

    assert asm_suffix == asm.Graph.suffix 


def test_assembler_cursor(asm, ksize):
    seed = 'A' * ksize

    asm.cursor = seed
    assert asm.cursor == seed


def test_assembler_cursor_wrong_size(asm, ksize):
    seed = 'A' * (ksize - 1)

    with pytest.raises(ValueError):
        asm.cursor = seed


class TestNonBranching:

    def test_all_start_positions(self, ksize, linear_path, asm, consumer):
        # assemble entire contig, starting from wherever
        contig = linear_path()

        for start in range(0, len(contig), 150):
            if len(contig) - start < ksize:
                continue
            asm.clear_seen()
            path = asm.assemble(contig[start:start + ksize])
            assert path == contig, (len(path), len(contig), start)

    def test_all_left_to_beginning(self, ksize, linear_path, asm, consumer):
        # assemble directed left
        contig = linear_path()

        for start in range(0, len(contig), 150):
            if len(contig) - start < ksize:
                continue
            asm.clear_seen()
            path = asm.assemble_left(contig[start:start + ksize])
            print(path, ', ', contig[:start])
            assert path == contig[:start + ksize], start

    def test_all_right_to_end(self, ksize, linear_path, asm, consumer):
        # assemble directed right
        contig = linear_path()

        for start in range(0, len(contig), 150):
            if len(contig) - start < ksize:
                continue
            asm.clear_seen()
            path = asm.assemble_right(contig[start:start + ksize])
            print(path, ', ', contig[:start])
            assert path == contig[start:], start

    def test_circular(self, ksize, circular, asm, consumer):
        contig = circular()

        path = asm.assemble_right(contig[:ksize])
        print(path, ',', contig)
        assert path == contig[:len(path)]


class TestBranchingBasic:

    def test_stops_at_fork(self, ksize, right_fork, asm, consumer):
        (sequence, branch), S = right_fork()

        path = asm.assemble_right(sequence[:ksize])
        assert path == sequence[:S+ksize]
        assembled_branch = asm.assemble(branch[-ksize:])
        assert branch == assembled_branch
    
    def test_assembles_through_fork_from_right(self, ksize, right_fork, asm, consumer):
        '''Test that we assemble through fork from the right when we haven't
        assembled the core path already.'''
        (sequence, branch), S = right_fork()

        assert sequence[:S+1] + branch == asm.assemble(branch[-ksize:])


    def test_stops_at_triple_fork(self, ksize, right_triple_fork,
                                  asm, consumer):
        (core, top, bottom), S = right_triple_fork()
        
        path = asm.assemble_right(core[:ksize])
        assert path == core[:S+ksize]
        
        assert top == asm.assemble(top[-ksize:])
        assert bottom == asm.assemble(bottom[-ksize:])
