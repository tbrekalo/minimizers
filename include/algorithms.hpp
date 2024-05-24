#pragma once

#include <cmath>
#include <numbers>
#include <complex>
#include <iomanip>

#include "external/fastmod/fastmod.h"

#include "util.hpp"
#include "enumerator.hpp"

namespace minimizers {

double redundancy_in_density_in_perc(const double density, const double lower_bound) {
    return (density / lower_bound - 1) * 100.0;
}

double redundancy_in_density_as_factor(const double density, const double lower_bound) {
    return density / lower_bound;
}

bool is_not_forward(const uint64_t k, const uint64_t w, const uint64_t t) {
    assert(w >= 2);
    assert(t <= k);
    /*
        We know a scheme is *not* foward when there exist x and y
        such that x mod w + 1 < y mod w, where x and y are the
        positions of the smallest t-mer in window i and i-1 respectively,
        for some i > 0. So we derive: x mod w < w - 2.
        Since x is in [0..l - t] = [0..w + k - 1 - t], then x is at most
        w + k - t - 1, i.e., w + k - t - 1 mod w < w - 2.

        All possible backward jumps (y mod w, x mod w), of length y-x-1,
        are for y in [x+1..w-1].

        Note: in math, we would write (k - t - 1) mod w < w - 2,
        but here we always sum w to avoid having to take the
        modulo of negative integers when t = k.
    */
    return (w + k - t - 1) % w < w - 2;
}

/* This ignores (asymptotic) lower order terms. */
double closed_form_density(std::string const& scheme_name,                        //
                           const uint64_t k, const uint64_t w, const uint64_t t)  //
{
    if (scheme_name == "miniception") {
        return 1.67 / w;
    } else if (scheme_name == "mod_sampling") {
        bool ok = (w + k - 1 - t) % w == w - 1;
        double correction = ok ? 0 : floor(1.0 + double(k - 1.0 - t) / w) / (w + k - t);
        return double(floor(1.0 + double(k - t - 1.0) / w) + 2.0 - correction) / (w + k - t + 1.0);
    } else {
        throw std::runtime_error("unknown scheme name");
    }
}

/*
    Each algorithm returns a position p in [0..w-1], corresponding
    to the position of the kmer selected as the window's fingerprint.
    Note: in case of ties, we return the *leftmost* kmer.
*/

template <typename Hasher>
struct mod_sampling {
    static std::string name() { return "mod_sampling"; }

    mod_sampling(uint64_t w, uint64_t k, uint64_t t, uint64_t seed)
        : m_w(w), m_k(k), m_t(t), m_seed(seed), m_enum_tmers(w + k - t, t, seed) {
        m_M_w = fastmod::computeM_u32(m_w);
    }

    /// Sample from a single window.
    uint64_t sample(char const* window) const {
        const uint64_t num_tmers = (m_w + m_k - 1) - m_t + 1;
        uint64_t p = -1;
        typename Hasher::hash_type min_hash(-1);
        // Find the leftmost tmer with minimal hash.
        for (uint64_t i = 0; i != num_tmers; ++i) {
            char const* tmer = window + i;
            auto hash = Hasher::hash(tmer, m_w, m_t, m_seed);
            if (hash < min_hash) {
                min_hash = hash;
                p = i;
            }
        }
        assert(p < num_tmers);
        uint64_t pos = fastmod::fastmod_u32(p, m_M_w, m_w);  // p % m_w

        // if (p == pos) {
        //     uint64_t i = 0;
        //     for (; i != p; ++i) { std::cout << "="; }
        //     std::cout << "|";
        //     for (; i != p + m_t; ++i) { std::cout << "*"; }
        //     for (; i != p + m_k; ++i) { std::cout << "="; }
        //     std::cout << "|";
        //     for (; i != m_w + m_k - 1; ++i) { std::cout << "="; }
        //     std::cout << std::endl;
        // } else {
        //     assert(pos < p);
        //     uint64_t i = 0;
        //     for (; i != pos; ++i) { std::cout << "="; }
        //     std::cout << "|";
        //     for (; i != p; ++i) { std::cout << "="; }
        //     for (; i != p + m_t; ++i) { std::cout << "*"; }
        //     for (; i != pos + m_k; ++i) { std::cout << "="; }
        //     std::cout << "|";
        //     for (; i != m_w + m_k - 1; ++i) { std::cout << "="; }
        //     std::cout << std::endl;
        // }

        return pos;
    }

    /// Sample from a stream.
    /// If `clear`, this is the first call.
    uint64_t sample(char const* window, bool clear) {
        m_enum_tmers.eat(window, clear);
        uint64_t p = m_enum_tmers.next();
        return fastmod::fastmod_u32(p, m_M_w, m_w);  // p % m_w
    }

private:
    uint64_t m_w, m_k, m_t, m_seed;
    uint64_t m_M_w;
    enumerator<Hasher> m_enum_tmers;
};

template <typename Hasher>
struct miniception {
    static std::string name() { return "miniception"; }

    miniception(uint64_t w, uint64_t k, uint64_t t, uint64_t seed)
        : m_w(w)
        , m_k(k)
        , m_t(t)
        , m_seed(seed)
        , m_enum_tmers(k - t + 1, t, seed)
        , m_enum_kmers(w, k, seed) {}

    uint64_t sample(char const* window) const {
        const uint64_t w0 = m_k - m_t;
        enumerator<Hasher> enum_tmers(w0 + 1, m_t, m_seed);
        uint64_t p = -1;
        typename Hasher::hash_type min_hash(-1);
        for (uint64_t i = 0; i != m_w; ++i) {
            char const* kmer = window + i;
            bool clear = i == 0;  // first kmer
            enum_tmers.eat(kmer, clear);
            uint64_t tmer_p = enum_tmers.next();
            assert(tmer_p >= 0 and tmer_p <= w0);
            if (tmer_p == 0 or tmer_p == w0) {  // context is charged
                auto hash = Hasher::hash(kmer, m_w, m_k, m_seed);
                if (hash < min_hash) {
                    min_hash = hash;
                    p = i;
                }
            }
        }
        assert(p < m_w);
        return p;
    }

    uint64_t sample(char const* window, bool clear) {
        for (uint64_t i = clear ? 0 : m_w - 1; i != m_w; ++i) {
            char const* kmer = window + i;
            m_enum_tmers.eat(kmer, i == 0);
            uint64_t tmer_p = m_enum_tmers.next();
            assert(tmer_p >= 0 and tmer_p <= m_k - m_t);
            if (tmer_p == 0 or tmer_p == m_k - m_t) {  // context is charged
                m_enum_kmers.eat(kmer);
            } else {
                m_enum_kmers.skip();
            }
        }
        uint64_t p = m_enum_kmers.next();
        assert(p < m_w);
        return p;
    }

private:
    uint64_t m_w, m_k, m_t, m_seed;
    enumerator<Hasher> m_enum_tmers;
    enumerator<Hasher> m_enum_kmers;
};

template <typename Hasher>
using rotational_alt_hash = std::pair<int64_t, typename Hasher::hash_type>;

/// Return the negative of the sum of characters in positions 0 mod w, so that
/// the kmer with max sum compares smallest.
template <typename Hasher>
struct rotational_alt_hasher {
    using hash_type = rotational_alt_hash<Hasher>;

    // TODO: This can be implemented in O(1) time by storing prefix sums and using a rolling hash.
    static hash_type hash(char const* kmer, const uint64_t w, const uint64_t k,
                          const uint64_t seed) {
        int64_t sum = 0;
        for (uint64_t j = 0; j < k; j += w) sum += kmer[j];
        return {-sum, Hasher::hash(kmer, w, k, seed)};
    }
};

/// Our own simpler and much faster version.
/// Sample the leftmost kmer with the largest sum of characters in positions 0 mod w.
/// This is equivalent to a mod_sampling with the rotational_hasher function.
template <typename Hasher>
struct rotational_alt {
    static std::string name() { return "rotational_alt"; }

    rotational_alt(uint64_t w, uint64_t k, uint64_t /*t*/, uint64_t seed)
        : m_w(w), m_k(k), m_seed(seed), m_enum_kmers(w, k, seed) {}

    uint64_t sample(char const* window) {
        uint64_t p = -1;
        rotational_alt_hash<Hasher> min_hash{-1, -1};
        for (uint64_t i = 0; i != m_w; ++i) {
            char const* kmer = window + i;
            auto hash = rotational_alt_hasher<Hasher>::hash(kmer, m_w, m_k, m_seed);
            if (hash < min_hash) {
                min_hash = hash;
                p = i;
            }
        }
        assert(p < m_w);
        return p;
    }

    uint64_t sample(char const* window, bool clear) {
        m_enum_kmers.eat(window, clear);
        return m_enum_kmers.next();
    }

private:
    uint64_t m_w, m_k, m_seed;
    enumerator<rotational_alt_hasher<Hasher>> m_enum_kmers;
};

template <typename Hasher>
using uhs_hash = std::pair<uint8_t, typename Hasher::hash_type>;

uint8_t char_remap[256];

/// Return whether the kmer is in the UHS, and the random kmer order.
template <typename Hasher>
struct rotational_orig_hasher {
    using hash_type = uhs_hash<Hasher>;

    static hash_type hash(char const* kmer, const uint64_t w, const uint64_t k,
                          const uint64_t seed) {
        bool in_uhs = true;

        uint64_t sum0 = 0;
        for (int pos = 0; pos < k; pos += w) sum0 += char_remap[kmer[pos]];

        for (uint64_t j = 1; j != w; ++j) {
            uint64_t sumj = 0;
            for (int pos = j; pos < k; pos += w) sumj += char_remap[kmer[pos]];
            // Assume alphabet size 4.
            uint64_t sigma = 4;
            // Instead of <=+sigma, we do <=+sigma-1,
            // since the max difference between two characters is actually
            // sigma-1, not sigma.
            // And in fact, sigma-2 also seems to work.
            // TODO: Prove that sigma-2 (or maybe sigma/2) is sufficient.
            if (!(sumj <= sum0 + sigma - 1)) {
                in_uhs = false;
                break;
            }
        }

        return {in_uhs ? 0 : 1, Hasher::hash(kmer, w, k, seed)};
    }
};

/* Version faithful to the original description by Marcais et al. */
template <typename Hasher>
struct rotational_orig {
    static std::string name() { return "rotational_orig"; }

    rotational_orig(uint64_t w, uint64_t k, uint64_t /*t*/, uint64_t seed)
        : m_w(w), m_k(k), m_seed(seed), m_enum_kmers(w, k, seed) {
        assert(m_k % m_w == 0);

        char_remap['A'] = 0;
        char_remap['C'] = 1;
        char_remap['T'] = 2;
        char_remap['G'] = 3;
    }

    uint64_t sample(char const* window) {
        uint64_t p = -1;
        uhs_hash<Hasher> min_hash{-1, -1};
        for (uint64_t i = 0; i != m_w; ++i) {
            char const* kmer = window + i;
            auto hash = rotational_orig_hasher<Hasher>::hash(kmer, m_w, m_k, m_seed);
            if (hash < min_hash) {
                min_hash = hash;
                p = i;
            }
        }
        if (min_hash.first != 0) {
            std::cerr << "Not a single kmer is in UHS!" << std::endl;
            assert(false);
        }
        assert(p < m_w);
        return p;
    }

    uint64_t sample(char const* window, bool clear) {
        m_enum_kmers.eat(window, clear);
        return m_enum_kmers.next();
    }

private:
    uint64_t m_w, m_k, m_seed;
    enumerator<rotational_orig_hasher<Hasher>> m_enum_kmers;
};

// TODO: Global variables are ugly and ideally should be replaced by member variables on the Hasher
// objects.
vector<long double> sines;
vector<std::complex<long double>> roots;
long double pi = std::numbers::pi_v<long double>;

// The pseudocode from the original paper.
// We intentionally ignore the 0 case.
bool is_decycling_original(char const* kmer, const uint64_t k) {
    long double im = 0;
    for (uint64_t i = 0; i != k; ++i) { im += sines[i] * kmer[i]; }
    long double im_rot = 0;
    for (uint64_t i = 0; i != k; ++i) { im_rot += sines[i + 1] * kmer[i]; }
    // std::cerr << "Im: " << im << " im_rot: " << im_rot <<
    // std::endl;
    return im > 0 and im_rot <= 0;
}

// Same method, but using complex numbers.
// This is only different due to floating point rounding errors, e.g. when imag(x)=0.
// The original method has ever so slightly better density.
bool is_decycling_arg_pos(char const* kmer, const uint64_t k) {
    std::complex<long double> x = 0;
    for (uint64_t i = 0; i != k; ++i) { x += roots[i] * (long double)kmer[i]; }
    auto a = std::arg(x);
    // std::cerr << "arg:    " << a << "\nthresh: " << pi - 2 * pi / k << endl;
    return pi - 2 * pi / k < a;
}

// Use angle around 0 instead of around pi.
//
// This is the first negative instead of first positive rotation.
// That should be equivalent since it's basically using the D-tilde.
//
// FIXME: This is around 1% worse than the versions above. I do not understand why.
bool is_decycling_arg_neg(char const* kmer, const uint64_t k) {
    std::complex<long double> x = 0;
    for (uint64_t i = 0; i != k; ++i) { x += roots[i] * (long double)kmer[i]; }
    auto a = std::arg(x);
    return -2 * pi / k < a and a <= 0;
}

template <typename Hasher>
struct decycling_hasher {
    using hash_type = uhs_hash<Hasher>;

    // TODO: This can be implemented in O(1) using rolling embedding.
    static hash_type hash(char const* kmer, const uint64_t w, const uint64_t k,
                          const uint64_t seed) {
        // for testing
        // auto is_decycling_1 = is_decycling_original(kmer, k);
        auto is_decycling_2 = is_decycling_arg_pos(kmer, k);
        // auto is_decycling_3 = is_decycling_arg_neg(kmer, k);
        auto is_decycling = is_decycling_2;
        return {is_decycling ? 0 : 1, Hasher::hash(kmer, w, k, seed)};
    }
};

/// Decycling code from original paper.
/// TODO: double decycling
template <typename Hasher>
struct decycling {
    static std::string name() { return "decycling"; }

    decycling(uint64_t w, uint64_t k, uint64_t /*t*/, uint64_t seed)
        : m_w(w), m_k(k), m_seed(seed), m_enum_kmers(w, k, seed) {
        sines.reserve(m_k + 1);
        for (uint64_t i = 0; i != m_k; ++i) { sines.push_back(std::sin(2 * pi * i / m_k)); }
        roots.reserve(m_k + 1);
        for (uint64_t i = 0; i != m_k; ++i) {
            roots.push_back(std::exp(std::complex<long double>(0, 2 * pi * i / m_k)));
        }
    }

    uint64_t sample(char const* window) {
        using T = std::pair<uint8_t, typename Hasher::hash_type>;
        uint64_t p = -1;
        T min_hash{-1, -1};
        for (uint64_t i = 0; i != m_w; ++i) {
            char const* kmer = window + i;
            auto hash = decycling_hasher<Hasher>::hash(kmer, m_w, m_k, m_seed);
            if (hash < min_hash) {
                min_hash = hash;
                p = i;
            }
        }
        assert(p < m_w);
        return p;
    }

    uint64_t sample(char const* window, bool clear) {
        m_enum_kmers.eat(window, clear);
        return m_enum_kmers.next();
    }

private:
    uint64_t m_w, m_k, m_seed;
    enumerator<decycling_hasher<Hasher>> m_enum_kmers;
};

template <typename Hasher>
struct double_decycling_hasher {
    using hash_type = uhs_hash<Hasher>;

    // TODO: This can be implemented in O(1) using rolling embedding.
    static hash_type hash(char const* kmer, const uint64_t w, const uint64_t k,
                          const uint64_t seed) {
        // FIXME: Using _original instead of _pos gives slightly better density?
        bool is_decycling_pos = is_decycling_arg_pos(kmer, k);
        bool is_decycling_neg = is_decycling_arg_neg(kmer, k);
        if (is_decycling_pos) { return {0, Hasher::hash(kmer, w, k, seed)}; }
        if (is_decycling_neg) { return {1, Hasher::hash(kmer, w, k, seed)}; }
        return {2, Hasher::hash(kmer, w, k, seed)};
    }
};

/// Decycling code from original paper.
/// TODO: double decycling
template <typename Hasher>
struct double_decycling {
    static std::string name() { return "double_decycling"; }

    double_decycling(uint64_t w, uint64_t k, uint64_t /*t*/, uint64_t seed)
        : m_w(w), m_k(k), m_seed(seed), m_enum_kmers(w, k, seed) {
        sines.reserve(m_k + 1);
        for (uint64_t i = 0; i != m_k; ++i) { sines.push_back(std::sin(2 * pi * i / m_k)); }
        roots.reserve(m_k + 1);
        for (uint64_t i = 0; i != m_k; ++i) {
            roots.push_back(std::exp(std::complex<long double>(0, 2 * pi * i / m_k)));
        }
    }

    uint64_t sample(char const* window) {
        using T = std::pair<uint8_t, typename Hasher::hash_type>;
        uint64_t p = -1;
        T min_hash{-1, -1};
        for (uint64_t i = 0; i != m_w; ++i) {
            char const* kmer = window + i;
            auto hash = double_decycling_hasher<Hasher>::hash(kmer, m_w, m_k, m_seed);
            if (hash < min_hash) {
                min_hash = hash;
                p = i;
            }
        }
        assert(p < m_w);
        return p;
    }

    uint64_t sample(char const* window, bool clear) {
        m_enum_kmers.eat(window, clear);
        return m_enum_kmers.next();
    }

private:
    uint64_t m_w, m_k, m_seed;
    enumerator<double_decycling_hasher<Hasher>> m_enum_kmers;
};

}  // namespace minimizers
