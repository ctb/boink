/* hasher.hh -- k-mer hash functions
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef HASHING_HH
#define HASHING_HH

#include "boink/boink.hh"
#include "oxli/alphabets.hh"
#include "oxli/kmer_hash.hh"

#include <deque>

# ifdef DEBUG_HASHING
#   define pdebug(x) do { std::cerr << std::endl << "@ " << __FILE__ <<\
                          ":" << __FUNCTION__ << ":" <<\
                          __LINE__  << std::endl << x << std::endl;\
                          } while (0)
# else
#   define pdebug(x) do {} while (0)
# endif


namespace boink {

class KmerClient {
protected:
    const uint16_t _K;
public:
    explicit KmerClient(uint16_t K) : _K(K) {}
    const uint16_t K() const { return _K; }
};


template <class Derived,
          const std::string& Alphabet = oxli::alphabets::DNA_SIMPLE>
class HashShifter : public KmerClient {
public:
    static const std::string symbols;
    std::deque<char> symbol_deque;
    
    hash_t set_cursor(const std::string& sequence) {
        if (sequence.length() < _K) {
            throw BoinkException("Sequence must at least length K");
        }
        if (!initialized) {
            load(sequence);
            derived().init();
        } else {
            for (auto c : sequence) {
                shift_right(c);
            }
        }
        return get();
    }

    hash_t set_cursor(const char * sequence) {
        // less safe! does not check length
        if(!initialized) {
            load(sequence);
            derived().init();
        } else {
            for (uint16_t i = 0; i < this->_K; ++i) {
                shift_right(sequence[i]);
            }
        }
        return get();
    }

    bool is_valid(const char c) const {
        for (auto symbol : symbols) {
            if (c == symbol) {
                return true;
            }
        }
        return false;
    }

    bool is_valid(const std::string& sequence) const {
        for (auto c : sequence) {
            if(!is_valid(c)) {
                return false;
            }
        }
        return true;
    }

    bool is_valid(const char * sequence) const {
        for (uint16_t i = 0; i < this->_K; ++i) {
            if(!is_valid(sequence[i])) {
                return false;
            }
        }
        return true;
    }

    void _validate(const char c) const {
        if (!this->is_valid(c)) {
            string msg("Invalid symbol: ");
            msg += c;
            throw InvalidCharacterException(msg.c_str());
        }
    }

    void _validate(const std::string& sequence) const {
        if (!is_valid(sequence)) {
            string msg("Invalid symbol in ");
            msg += sequence;
            throw InvalidCharacterException(msg.c_str());
        }
    }

    // shadowed by derived
    hash_t get() {
        return derived().get();
    }

    hash_t hash(const std::string& sequence) const {
        _validate(sequence);
        return derived()._hash(sequence);
    }

    hash_t hash(const char * sequence) const {
        _validate(sequence);
        return derived()._hash(sequence);
    }

    // shadowed by derived impl
    std::vector<shift_t> gather_left() {
        return derived().gather_left();
    }

    hash_t shift_left(const char c) {
        _validate(c);
        symbol_deque.push_front(c);
        hash_t h = derived().update_left(c);
        symbol_deque.pop_back();
        return h;
    }

    // shadowed by derived impl
    std::vector<shift_t> gather_right() {
        return derived().gather_right();
    }

    hash_t shift_right(const char c) {
        _validate(c);
        symbol_deque.push_back(c);
        hash_t h = derived().update_right(c);
        symbol_deque.pop_front();
        return h;
    }

    std::string get_cursor() const {
        return std::string(symbol_deque.begin(), symbol_deque.end());
    }

    void get_cursor(std::deque<char>& d) const {
        d.insert(d.end(), symbol_deque.begin(), symbol_deque.end());
    }

private:

    HashShifter(const std::string& start, uint16_t K) :
        KmerClient(K), initialized(false) {

        load(start);
    }

    HashShifter(uint16_t K) :
        KmerClient(K), initialized(false) {}

    friend Derived;

    Derived& derived() {
        return *static_cast<Derived*>(this);
    }

    const Derived& derived() const {
        return *static_cast<const Derived*>(this);
    }

protected:

    bool initialized;

    void load(const std::string& sequence) {
        symbol_deque.clear();
        symbol_deque.insert(symbol_deque.begin(),
                            sequence.begin(),
                            sequence.begin()+_K); 
    }

    void load(const char * sequence) {
        symbol_deque.clear();
        for (uint16_t i = 0; i < this->_K; ++i) {
            symbol_deque.push_back(sequence[i]);
        }
    }

};

template<class Derived, const std::string& Alphabet>
const std::string HashShifter<Derived, Alphabet>::symbols = Alphabet;



template <const std::string& Alphabet = oxli::alphabets::DNA_SIMPLE>
class RollingHashShifter : public HashShifter<RollingHashShifter<Alphabet>,
                                              Alphabet> {
protected:
    typedef HashShifter<RollingHashShifter<Alphabet>, Alphabet> BaseShifter;

    CyclicHash<hash_t> hasher;
    using BaseShifter::_K;

public:

    //using BaseShifter::HashShifter;

    RollingHashShifter(const std::string& start, uint16_t K)
        : BaseShifter(start, K), hasher(K)
    {    
        init();
    }

    RollingHashShifter(uint16_t K)
        : BaseShifter(K),
          hasher(K)
    {
    }

    RollingHashShifter(const RollingHashShifter& other)
        : BaseShifter(other.K()),
          hasher(other.K())
    {
        other.get_cursor(this->symbol_deque);
        init();
    }

    void init() {
        if (this->initialized) {
            return;
        }
        for (auto c : this->symbol_deque) {
            this->_validate(c);
            hasher.eat(c);
        }
        this->initialized = true;
    }

    hash_t get() {
        return hasher.hashvalue;
    }

    hash_t _hash(const std::string& sequence) const {
        return oxli::_hash_cyclic_forward(sequence, this->_K);
    }

    hash_t _hash(const char * sequence) const {
        CyclicHash<hash_t> hasher(this->_K);
        for (uint16_t i = 0; i < this->_K; ++i) {
            hasher.eat(sequence[i]);
        }
        return hasher.hashvalue;
    }

    hash_t update_left(const char c) {
        hasher.reverse_update(c, this->symbol_deque.back());
        return get();
    }

    std::vector<shift_t> gather_right() {
        std::vector<shift_t> hashes;
        const char front = this->symbol_deque.front();
        for (auto symbol : Alphabet) {
            hasher.update(front, symbol);
            hashes.push_back(shift_t(hasher.hashvalue, symbol));
            hasher.reverse_update(front, symbol);
        }
        return hashes;
    }

    hash_t update_right(const char c) {
        hasher.update(this->symbol_deque.front(), c);
        return get();
    }

    std::vector<shift_t> gather_left() {
        std::vector<shift_t> hashes;
        const char back = this->symbol_deque.back();
        for (auto symbol : Alphabet) {
            hasher.reverse_update(symbol, back);
            shift_t result(hasher.hashvalue, symbol);
            hashes.push_back(result);
            hasher.update(symbol, back);
        }

        return hashes;
    }
};

typedef RollingHashShifter<oxli::alphabets::DNA_SIMPLE> DefaultShifter;


template <class ShifterType>
class KmerIterator : public KmerClient {
    const std::string _seq;
    unsigned int index;
    unsigned int length;
    bool _initialized, _shifter_owner;

public:

    ShifterType * shifter;

    KmerIterator(const std::string& seq, uint16_t K) :
        KmerClient(K), _seq(seq), 
        index(0), _initialized(false), _shifter_owner(true) {

        shifter = new ShifterType(seq, K);

        if (_seq.length() < _K) {
            throw BoinkException("Sequence must have length >= K");
        }
    }

    KmerIterator(const std::string& seq, ShifterType * shifter) :
        KmerClient(shifter->K()), _seq(seq),
        index(0), _initialized(false),
        _shifter_owner(false), shifter(shifter) {
        
        if(_seq.length() < _K) {
            throw BoinkException("Sequence must have length >= K");
        }

        shifter->set_cursor(seq);
    }

    hash_t first() {
        _initialized = true;

        index += 1;
        return shifter->get();
    }

    hash_t next() {
        if (!_initialized) {
            return first();
        }

        if (done()) {
            throw InvalidCharacterException("past end of iterator");
        }

        shifter->shift_right(_seq[index + _K - 1]);
        index += 1;

        return shifter->get();
    }

    bool done() const {
        return (index + _K > _seq.length());
    }

    unsigned int get_start_pos() const {
        if (!_initialized) { return 0; }
        return index - 1;
    }

    unsigned int get_end_pos() const {
        if (!_initialized) { return _K; }
        return index + _K - 1;
    }
};


/*
class FullRollingHasher {
    const char * _seq;
    const std::string _rev;
    const char _ksize;
    unsigned int index;
    unsigned int length;
    bool _initialized;
    oxli::CyclicHash<uint64_t> fwd_hasher;
    oxli::CyclicHash<uint64_t> bwd_hasher;

public:
    FullRollingHasher(const char * seq, unsigned char k) :
        _seq(seq), _rev(oxli::_revcomp(seq)), _ksize(k), index(0),
        _initialized(false), fwd_hasher(k), bwd_hasher(k)
    {
        length = strlen(_seq);
    };

    full_hash_t first() {
        _initialized = true;

        for (char i = 0; i < _ksize; ++i) {
            fwd_hasher.eat(*(_seq + i));
            bwd_hasher.eat(_rev[length - _ksize + i]);
        }
        index += 1;
        return std::make_pair(fwd_hasher.hashvalue, bwd_hasher.hashvalue);
    }

    full_hash_t next() {
        if (!_initialized) {
            return first();
        }

        if (done()) {
            throw oxli_exception("past end of iterator");
        }
        fwd_hasher.update(*(_seq + index - 1), *(_seq + index + _ksize - 1));

        // first argument is added, second is removed from the hash
        bwd_hasher.reverse_update(
          _rev[length - _ksize - index], _rev[length - index]);

        index += 1;

        return std::make_pair(fwd_hasher.hashvalue, bwd_hasher.hashvalue);
    }

    bool done() const {
        return (index + _ksize > length);
    }

    unsigned int get_start_pos() const {
        if (!_initialized) { return 0; }
        return index - 1;
    }

    unsigned int get_end_pos() const {
        if (!_initialized) { return _ksize; }
        return index + _ksize - 1;
    }
};
*/

}

#undef pdebug
#endif
