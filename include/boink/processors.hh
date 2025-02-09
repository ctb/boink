/* processors.hh -- 
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef BOINK_PROCESSORS_HH
#define BOINK_PROCESSORS_HH

#include <tuple>
#include <memory>
#include <fstream>
#include <iostream>

#include "boink/boink.hh"
#include "boink/parsing/parsing.hh"
#include "boink/parsing/readers.hh"
#include "boink/events.hh"
#include "boink/event_types.hh"
#include "boink/hashing/hashing_types.hh"
#include "boink/hashing/exceptions.hh"
#include "boink/cdbg/compactor.hh"
#include "boink/ukhs_signature.hh"

#include "sourmash/kmer_min_hash.hh"


#define DEFAULT_FINE_INTERVAL 10000
#define DEFAULT_MEDIUM_INTERVAL 100000
#define DEFAULT_COARSE_INTERVAL 1000000

namespace boink {

class IntervalCounter {

    uint64_t interval;
    uint64_t counter;

public:

    IntervalCounter(uint64_t interval)
        : interval(interval),
          counter(0)
    {
    }

    bool poll(uint64_t incr=1) {
        counter += incr;
        if (counter == interval) {
            counter = 0;
            return true;
        } else {
            return false;
        }
    }

};


template <class Derived,
          class ParserType = parsing::FastxReader>
class FileProcessor : public events::EventNotifier {

public:

    struct interval_state {
        bool fine;
        bool medium;
        bool coarse;
        bool end;

        interval_state()
            : fine(false), medium(false), coarse(false), end(false)
        {
        }

        interval_state(bool fine, bool medium, bool coarse, bool end)
            : fine(fine), medium(medium), coarse(coarse), end(end)
        {
        }
    };

protected:

    std::array<IntervalCounter, 3> counters;
    uint64_t _n_reads;

    bool _ticked(interval_state tick) {
        return tick.fine || tick.medium || tick.coarse || tick.end;
    }

    interval_state _notify_tick(uint64_t n_ticks) {
        interval_state result;

        if (counters[0].poll(n_ticks)) {
             //std::cerr << "processed " << _n_reads << " sequences." << std::endl;               
             derived().report();
             auto event = make_shared<events::TimeIntervalEvent>();
             event->level = events::TimeIntervalEvent::FINE;
             event->t = _n_reads;
             notify(event);
             result.fine = true;
        }
        if (counters[1].poll(n_ticks)) {
             auto event = make_shared<events::TimeIntervalEvent>();
             event->level = events::TimeIntervalEvent::MEDIUM;
             event->t = _n_reads;
             notify(event);
             result.medium = true;
        }
        if (counters[2].poll(n_ticks)) {
             auto event = make_shared<events::TimeIntervalEvent>();
             event->level = events::TimeIntervalEvent::COARSE;
             event->t = _n_reads;
             notify(event);
             result.coarse = true;
        }

        result.end = false;
        return result;
    }

    void _notify_stop() {
        auto event = make_shared<events::TimeIntervalEvent>();
        event->level = events::TimeIntervalEvent::END;
        event->t = _n_reads;
        notify(event);
    }


public:

    using events::EventNotifier::register_listener;
    using events::EventNotifier::notify;

    FileProcessor(uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                  uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                  uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        :  events::EventNotifier(),
           counters ({{ fine_interval, 
                        medium_interval,
                        coarse_interval }}),
          _n_reads(0) {

    }

    uint64_t process(const string& left_filename,
                     const string& right_filename,
                     uint32_t min_length=0,
                     bool force_name_match=false) {
        parsing::SplitPairedReader<ParserType> reader(left_filename,
                                             right_filename,
                                             min_length,
                                             force_name_match);
        return process(reader);
    }

    uint64_t process(string const &filename) {
        parsing::ReadParserPtr<ParserType> parser = parsing::get_parser<ParserType>(filename);
        return process(parser);
    }

    uint64_t process(parsing::SplitPairedReader<ParserType>& reader) {
        while(1) {
            auto state = advance(reader);
            if (state.end) {
                break;
            }
        }

        return _n_reads;
    }

    uint64_t process(parsing::ReadParserPtr<ParserType>& parser) {
        while(1) {
            auto state = advance(parser);
            if (state.end) {
                break;
            }
        }

        return _n_reads;
    }

    void process_sequence(parsing::ReadBundle& bundle) {
        if (bundle.has_left) {
            derived().process_sequence(bundle.left);
        }
        if (bundle.has_right) {
            derived().process_sequence(bundle.right);
        }
    }

    interval_state advance(parsing::SplitPairedReader<ParserType>& reader) {
        parsing::ReadBundle bundle;

        while(!reader.is_complete()) {
            bundle = reader.next();
            derived().process_sequence(bundle);

            int _bundle_count = bundle.has_left + bundle.has_right;
            _n_reads += _bundle_count;

            auto tick_result = _notify_tick(_bundle_count);
            if (_ticked(tick_result)) {
                return tick_result;
            }
        }
        _notify_stop();
        return interval_state(false, false, false, true);
    }

    interval_state advance(parsing::ReadParserPtr<ParserType>& parser) {
        parsing::Read read;

        // Iterate through the reads and consume their k-mers.
        while (!parser->is_complete()) {
            try {
                read = parser->get_next_read( );
            } catch (parsing::NoMoreReadsAvailable) {
                break;
            }

            read.set_clean_seq();
            derived().process_sequence(read);

            __sync_add_and_fetch( &_n_reads, 1 );
              auto tick_result = _notify_tick(1);

            bool end = false;
            if (_ticked(tick_result)) {
                return tick_result;
            }

        }
        _notify_stop();
        return interval_state(false, false, false, true);
    }

    uint64_t n_reads() const {
        return _n_reads;
    }

private:

    FileProcessor() : _n_reads(0) {}

    friend Derived;

    Derived& derived() {
        return *static_cast<Derived*>(this);
    }

    const Derived& derived() const {
        return *static_cast<const Derived*>(this);
    }

};


template <class GraphType,
          class ParserType = parsing::FastxReader>
class FileConsumer : public FileProcessor<FileConsumer<GraphType, ParserType>,
                                          ParserType> {

protected:

    shared_ptr<GraphType> graph;
    uint64_t _n_consumed;

    typedef FileProcessor<FileConsumer<GraphType, ParserType>,
                          ParserType> Base;

public:

    using Base::process_sequence;
    
    FileConsumer(shared_ptr<GraphType> graph,
                 uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                 uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                 uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          graph(graph), _n_consumed(0) {

    }

    void process_sequence(const parsing::Read& read) {
        auto this_n_consumed = graph->insert_sequence(read.cleaned_seq);
        __sync_add_and_fetch( &_n_consumed, this_n_consumed );
    }

    void report() {
        std::cerr << "\t and " << _n_consumed << " new k-mers." << std::endl;
    }

    uint64_t n_consumed() const {
        return _n_consumed;
    }

};


class UKHSCountSignatureProcessor : public FileProcessor<UKHSCountSignatureProcessor,
                                                         parsing::FastxReader> {
protected:

    shared_ptr<signatures::UKHSCountSignature> signature;

    typedef FileProcessor<UKHSCountSignatureProcessor, parsing::FastxReader> Base;

public:

    using Base::process_sequence;

    UKHSCountSignatureProcessor(shared_ptr<signatures::UKHSCountSignature> signature,
                                uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                                uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                                uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          signature(signature)
    {
    }

    void process_sequence(const parsing::Read& read) {
        signature->insert_sequence(read.cleaned_seq);
    }

    void report() {
    }
};



class SourmashSignatureProcessor : public FileProcessor<SourmashSignatureProcessor,
                                                        parsing::FastxReader> {

protected:

    KmerMinHash * signature;
    typedef FileProcessor<SourmashSignatureProcessor, parsing::FastxReader> Base;

public:

    using Base::process_sequence;

    SourmashSignatureProcessor(KmerMinHash * signature,
                               uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                               uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                               uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          signature(signature)
    {
    }

    void process_sequence(const parsing::Read& read) {
        signature->add_sequence(read.cleaned_seq.c_str(), false);
    }

    void report() {
    }
};


template <class GraphType,
          class ParserType = parsing::FastxReader>
class DecisionNodeProcessor : public FileProcessor<DecisionNodeProcessor<GraphType, ParserType>,
                                                   ParserType> {

protected:
    typedef FileProcessor<DecisionNodeProcessor<GraphType, ParserType>,
                          ParserType> Base;
    shared_ptr<cdbg::StreamingCompactor<GraphType>> compactor;
    shared_ptr<GraphType> graph;
    std::string _output_filename;
    std::ofstream _output_stream;

public:

    using Base::process_sequence;

    DecisionNodeProcessor(shared_ptr<cdbg::StreamingCompactor<GraphType>> compactor,
                          std::string& output_filename,
                          uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                          uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                          uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          compactor(compactor),
          graph(compactor->dbg),
          _output_filename(output_filename),
          _output_stream(_output_filename.c_str()) {

        _output_stream << "read_n, l_degree, r_degree, position, hash" << std::endl;

    }

    ~DecisionNodeProcessor() {
        _output_stream.close();
    }

    void process_sequence(const parsing::Read& read) {
        uint64_t n_new = graph->insert_sequence(read.cleaned_seq);
        if (n_new > 0) {
            std::vector<uint32_t> decision_positions;
            std::vector<hashing::hash_t> decision_hashes;
            std::vector<NeighborBundle> decision_neighbors;
            std::set<hashing::hash_t> new_kmers;

            compactor->find_decision_kmers(read.cleaned_seq,
                                           decision_positions,
                                           decision_hashes,
                                           decision_neighbors);

            for (size_t i=0; i<decision_positions.size(); ++i) {
                _output_stream << this->n_reads() << ", "
                               << decision_neighbors[i].first.size() << ", "
                               << decision_neighbors[i].second.size() << ", "
                               << decision_positions[i] << ", "
                               << decision_hashes[i] << std::endl;
            }
        }
    }

    void report() {};

};


template <class GraphType,
          class ParserType = parsing::FastxReader>
class StreamingCompactorProcessor : 
    public FileProcessor<StreamingCompactorProcessor<GraphType, ParserType>,
                         ParserType> { //template class names like modern art

protected:

    shared_ptr<cdbg::StreamingCompactor<GraphType>> compactor;
    shared_ptr<GraphType> graph;

    typedef FileProcessor<StreamingCompactorProcessor<GraphType, ParserType>,
                          ParserType> Base;

public:

    using Base::process_sequence;
    using events::EventNotifier::register_listener;
    
    StreamingCompactorProcessor(shared_ptr<cdbg::StreamingCompactor<GraphType>> compactor,
                                uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                                uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                                uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          compactor(compactor),
          graph(compactor->dbg)
    {
    }

    void process_sequence(const parsing::Read& read) {
        try {
            compactor->update_sequence(read.cleaned_seq);
        } catch (hashing::InvalidCharacterException &e) {
            std::cerr << "WARNING: Bad sequence encountered at "
                      << this->_n_reads << ": "
                      << read.cleaned_seq << ", exception was "
                      << e.what() << std::endl;
            return;
        } catch (hashing::SequenceLengthException &e) {
            std::cerr << "NOTE: Skipped sequence that was too short: read "
                      << this->_n_reads << " with sequence "
                      << read.cleaned_seq 
                      << std::endl;
            return;
        } catch (std::exception &e) {
            std::cerr << "ERROR: Exception thrown at " << this->_n_reads 
                      << " with msg: " << e.what()
                      <<  std::endl;
            throw e;
        }
    }

    void report() {
        //std::cerr << "\tcurrently " << compactor->cdbg->n_decision_nodes()
        //          << " d-nodes, " << compactor->cdbg->n_unitig_nodes()
        //          << " u-nodes." << std::endl;
    }

};



template <class ShifterType,
          class ParserType = parsing::FastxReader>
class MinimizerProcessor : public FileProcessor<MinimizerProcessor<ShifterType, ParserType>,
                                                ParserType> {

protected:

    WKMinimizer<ShifterType> M;
    std::string _output_filename;
    std::ofstream _output_stream;

    typedef FileProcessor<MinimizerProcessor<ShifterType, ParserType>,
                                             ParserType> Base;
public:

    using Base::process_sequence;

    MinimizerProcessor(int32_t window_size,
                       uint16_t K,
                       const std::string& output_filename,
                       uint64_t fine_interval=DEFAULT_FINE_INTERVAL,
                       uint64_t medium_interval=DEFAULT_MEDIUM_INTERVAL,
                       uint64_t coarse_interval=DEFAULT_COARSE_INTERVAL)
        : Base(fine_interval, medium_interval, coarse_interval),
          M(window_size, K),
          _output_filename(output_filename),
          _output_stream(_output_filename.c_str()) {
    
    }

    ~MinimizerProcessor() {
        _output_stream.close();
    }

    void process_sequence(const parsing::Read& read) {
        std::vector<typename WKMinimizer<ShifterType>::value_type> minimizers;
        minimizers = M.get_minimizers(read.cleaned_seq);

        for (auto min : minimizers) {
            _output_stream << this->n_reads() << ","
                           << min.second << ","
                           << min.first << ","
                           << read.cleaned_seq.substr(min.second, M.K())
                           << std::endl;
        }
    }

    void report() {}
};

} //namespace boink
#endif
