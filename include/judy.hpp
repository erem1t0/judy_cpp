// Judy_lib C++20
#pragma once

#include <cstdint>
//#ifdef WINDOWS // or maybe include for everyone?
#include <cstddef>

// is it crossplatform?
#include <climits> // for CHAR_BIT

#include <algorithm> // for test lower_bound

#include <cstring> // for std::memcpy

#include <bit> // for std::popcount

#include <stdio.h> // for printf

#include <iostream>
#include <cassert>

#include <concepts>
#include <type_traits>
#include <cstdlib> // for malloc

#include <array> // for std::array

#include <memory>
#include <new>

// JUDY1
#include <stdexcept> 

/*
    TBD: alignment

    TBD: consteval?
        constinit?
        inline?

    TBD: likely unlikely

    TBD: think about memory corrupt?

    Check: all thresholds
    
    TBD: make max_compress for JudySL: RLL-logic like in original

    Warning: RLL is not the same as in original, it is just JLL on root_level
*/


// Need to move in config.h.in
constexpr std::size_t EXPANSE_COUNT = 256;
constexpr std::size_t CACHE_LINE_SIZE_WORDS = 8;   
constexpr std::size_t BRANCH_LINEAR_MAX = 7;      
constexpr std::size_t BRANCH_BITMAP_MAX = 185;
constexpr std::size_t LEAF_LINEAR_MAX = 128; 
constexpr std::size_t LEAF1_LINEAR_MAX = 32;
constexpr std::size_t JUDY1_LEAF_LINEAR_CL_MAX = 2;
constexpr std::size_t JUDYL_LEAF_LINEAR_CL_MAX = 4;      
constexpr std::size_t UNDER_BRANCH_LINEAR_MAX = 1000;
constexpr std::size_t UNDER_BRANCH_BITMAP_MAX = 135;
constexpr std::size_t MIN_ARRAY_SIZE_TO_UNCOMPRESSED = 750;

// do not like it, maybe turned it off
constexpr std::size_t GLOBAL_HYSTERESYS = 300;

// let assume so for now
constexpr std::size_t MALLOC_HEADER_WORDS = 2;
/*
#if defined(_ARM_) || (defined(_X86_) && !defined(__x86_64))
    #define _ALLOCA_S_MARKER_SIZE 8
#elif defined(__ia64__) || defined(__x86_64) || defined(__aarch64__)
    #define _ALLOCA_S_MARKER_SIZE 16
#endif
*/

constexpr std::size_t HYSTERESYS_BRANCH_TO_LEAF = 1;
constexpr std::size_t HYSTERESYS_LEAF_TO_IMMED = 0;
constexpr std::size_t HYSTERESYS_LEAF_BITMAP_TO_LINEAR = 1;
constexpr std::size_t HYSTERESYS_BRANCH_BITMAP_TO_LINEAR = 1;
constexpr std::size_t HYSTERESYS_BRANCH_UNCOMPRESSED_TO_BITMAP = 10;
constexpr std::size_t HYSTERESYS_BRANCH_UNCOMPRESSED_TO_LINEAR = 0; 

static_assert(BRANCH_LINEAR_MAX <= EXPANSE_COUNT && BRANCH_BITMAP_MAX <= EXPANSE_COUNT);


/*
    this code is little-endian
*/

namespace judy {

    using word_t = std::uintptr_t;

    using word8_t  = std::uint8_t;
    using word16_t = std::uint16_t;
    using word32_t = std::uint32_t;
    
    // #ifdef JUDY64_BIT
    using word64_t = std::uint64_t;

    constexpr std::size_t word_size = sizeof(word_t);
    constexpr std::size_t word_bits_size = CHAR_BIT * word_size;

    constexpr std::size_t root_level = word_size;

    constexpr std::size_t SUBEXPANSE_COUNT = (EXPANSE_COUNT  + word_bits_size - 1) / word_bits_size;


    // need to reconsider
    enum class ErrorType {
        NotError = 0,
        NoMem = 1,
        Unsorted = 2,
        Corrupt = 3,
    };

    struct JudyError {
        ErrorType type;
        std::size_t line;
    };


    // Judy Pointer
    union JP {

        struct JPBase {
            word_t addr;
            word8_t dcd_pop0[word_size - 1];
            word8_t type;
        } node;
        
        struct Raw {
            word_t w[2];
        } raw;
        
    };

    // not sure
    //constexpr JP null_jp = { .node = {.addr = 0, .dcd_pop = {0}, .type=0x01}};
    constexpr JP null_jp{};

    static_assert(sizeof(JP) == 2 * word_size, "Judy Pointer must be 2 words");

    struct JPM {
        word_t pop0;
        JP top_jp;
        word_t lastpop0; // want to turn off
        word_t total_memory; // still unused
        JudyError error; // only for debug
    };

    //static_assert((sizeof(JPM) / word_size) <= CACHE_LINE_SIZE_WORDS); // i think it's good

    // Judy branch linear
    struct JBL {
        word8_t pop0;
        word8_t keys[BRANCH_LINEAR_MAX];

        // alignas(word_t) JP pointers[BRANCH_LINEAR_MAX];
        JP pointers[BRANCH_LINEAR_MAX];
    };

    // 16 for 32-bit, 15 for 64-bit
    //static_assert(sizeof(JBL) / word_size <= CACHE_LINE_SIZE_WORDS);

    struct JBB {

        struct Subexpanse { 
            word_t bitmap;
            JP* pointers;
        } subexpanses[SUBEXPANSE_COUNT];

    };

    // 16 for 32-bit, 8 for 64-bit
    //static_assert(sizeof(JBB) / word_size <= 2 * CACHE_LINE_SIZE_WORDS);
    // NOTE: 8 words for 64-bit it's great!

    struct JBU {
        JP pointers[EXPANSE_COUNT];
    };

    static_assert(sizeof(JBU) / word_size == 2 * EXPANSE_COUNT);

    struct Judy1JLB {
        word_t subexpanses[SUBEXPANSE_COUNT];
    };

    struct JudyLJLB {
        struct {
            word_t bitmap;
            word_t* values;
        } subexpanses[SUBEXPANSE_COUNT];
    };

    namespace types {

        constexpr word8_t Null_JP = 0;

        constexpr word8_t JBL_Base = 1;
        constexpr word8_t JBL_Level(std::size_t level) { return JBL_Base + (level - 2); }

        constexpr word8_t JBB_Base = JBL_Base + (root_level - 1);
        constexpr word8_t JBB_Level(std::size_t level) { return JBB_Base + (level - 2); }

        constexpr word8_t JBU_Base = JBB_Base + (root_level - 1);
        constexpr word8_t JBU_Level(std::size_t level) { return JBU_Base + (level - 2); }

        constexpr word8_t JLL_Base = JBU_Base + (root_level - 1);
        constexpr word8_t JLL_Level(std::size_t level) { return JLL_Base + (level - 1); }
        constexpr word8_t RLL = JLL_Level(root_level);
        /* level = idx_size; */

        constexpr word8_t JLB_Base = JLL_Base + root_level;

        constexpr word8_t JLB_FULL = JLB_Base + 1;

        // should do FULL - 1, -2, -3 ...?

        constexpr word8_t JPIMMED_Base = JLB_FULL + 1;

        template<typename JudyPolicy>
        constexpr word8_t immed_level(std::size_t k) {
            word8_t offset = 0;
            for(std::size_t i = 1; i < k; ++i) {
                offset += JudyPolicy::immed_max_cnt(i);
            }
            return offset;
        }

        template<typename JudyPolicy>
        constexpr word8_t JPImmed_t(std::size_t k, std::size_t cnt) {
            return JPIMMED_Base + immed_level<JudyPolicy>(k) + (cnt - 1);
        }

        template<typename JudyPolicy>
        constexpr word8_t JP_max_type = JPImmed_t<JudyPolicy>(word_size - 1, JudyPolicy::immed_max_cnt(word_size - 1)) + 1;
    } // namespace types


    template<typename Policy>
    concept JudyCapacityPolicy = requires(std::size_t size) {
        { Policy::can_insert_inplace(size, size) }  -> std::same_as<bool>;
        { Policy::can_delete_inplace(size, size) }  -> std::same_as<bool>;
        { Policy::get_capacity(size) }              -> std::convertible_to<std::size_t>;
    };

    using DefaultAllocator = std::allocator<std::byte>;

    struct DefaultCapacityPolicy {

        static constexpr std::size_t LIMIT_1 = JUDY1_LEAF_LINEAR_CL_MAX;
        static constexpr std::size_t LIMIT_2 = word_bits_size * sizeof(JP) / word_size;
        static constexpr std::size_t LIMITS[] = {LIMIT_1, LIMIT_2};

        static constexpr std::size_t LIMIT_DIFF_WORDS = 4; 

        static constexpr std::size_t LIMIT_MAX = std::max({LIMIT_1, LIMIT_2});

        static constexpr std::size_t STEP_1 = 4;
        static constexpr std::size_t STEP_2 = 16;
        static constexpr std::size_t STEP_3 = 32;

        static constexpr std::size_t STEP_LIMIT_1 = 16;
        static constexpr std::size_t STEP_LIMIT_2 = 64;

        static constexpr std::size_t MAX_SIZES_COUNT = 32;

        template<std::size_t MaxSize>
        struct ArraySizes {
            std::size_t sizes[MaxSize];
            std::size_t count;
        };

        static constexpr ArraySizes<MAX_SIZES_COUNT> arr_sizes = []() consteval {

            ArraySizes<MAX_SIZES_COUNT> ans{};
            
            std::size_t curr_bytes = CACHE_LINE_SIZE_WORDS * word_size;
            while(ans.count < MAX_SIZES_COUNT && (curr_bytes / word_size) <= LIMIT_MAX) {
                std::size_t words_with_header = curr_bytes / word_size;
                std::size_t words_without_header = words_with_header - MALLOC_HEADER_WORDS;

                for(std::size_t limit : LIMITS) {
                    if(words_without_header < limit && (words_without_header + LIMIT_DIFF_WORDS) >= limit) {
                        words_without_header = limit;
                        break;
                    }
                }

                if(ans.count == 0 || words_without_header != ans.sizes[ans.count - 1]) {
                    ans.sizes[ans.count++] = words_without_header;
                }
                
                if      (curr_bytes < STEP_LIMIT_1 * word_size)     curr_bytes += STEP_1 * word_size;
                else if (curr_bytes < STEP_LIMIT_2 * word_size)     curr_bytes += STEP_2 * word_size;
                else                                                curr_bytes += STEP_3 * word_size;
            }

            return ans;
        }();

        static constexpr std::size_t get_capacity(std::size_t size) {
            std::size_t i = 0;
            while(i < arr_sizes.count - 1 && arr_sizes.sizes[i] < size) {
                ++i;
            }
            std::size_t ans = arr_sizes.sizes[i];
            while(size > ans) {
                ans += STEP_3 * word_size;
            }
            
            return ans;        
        }

        static bool can_insert_inplace(std::size_t pop1, std::size_t element_size) {
            const std::size_t old_words = (pop1 * element_size + word_size - 1) / word_size;
            const std::size_t new_words = ((pop1 + 1) * element_size + word_size - 1) / word_size;
            return get_capacity(old_words) == get_capacity(new_words);
        }

        static bool can_delete_inplace(std::size_t pop1, std::size_t element_size) {
            const std::size_t old_words = (pop1 * element_size + word_size - 1) / word_size;
            const std::size_t new_words = ((pop1 - 1) * element_size + word_size - 1) / word_size;
            return get_capacity(old_words) == get_capacity(new_words);
        }
    };

    namespace memory {

        inline std::size_t words_for_bytes(std::size_t bytes) {
            return (bytes + word_size - 1) / word_size;
        }

        template<typename CapacityPolicy>
        requires JudyCapacityPolicy<CapacityPolicy>
        std::size_t allocation_words(std::size_t count, std::size_t element_size) {
            return CapacityPolicy::get_capacity(words_for_bytes(count * element_size));
        }

        template<typename CapacityPolicy>
        requires JudyCapacityPolicy<CapacityPolicy> 
        std::size_t allocation_bytes(std::size_t count, std::size_t element_size) {
            return allocation_words<CapacityPolicy>(count, element_size) * word_size;
        }

        template<typename JudyPolicy, typename CapacityPolicy>
        requires JudyCapacityPolicy<CapacityPolicy>
        std::size_t jll_allocation_words(std::size_t pop1, std::size_t level) {
            return    CapacityPolicy::get_capacity(JudyPolicy::jll_words(level, pop1));
        }

        template<typename /*policy::JudyArrayPolicy*/ JudyPolicy, typename CapacityPolicy>
        requires JudyCapacityPolicy<CapacityPolicy> 
        std::size_t jll_allocation_bytes(std::size_t pop1, std::size_t level) {
            return jll_allocation_words<JudyPolicy, CapacityPolicy>(pop1, level) * word_size;
        }

        template<typename T>
        void zero_memory(T* ptr, std::size_t size) {
            std::memset(ptr, 0, size);
        }

        template<typename Allocator, typename T>
        using rebind_alloc_t = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

        // raw alloc
        template<typename Allocator>
        word_t* alloc_words(JPM* jpm, Allocator& alloc, std::size_t words) {
            using A = rebind_alloc_t<Allocator, word_t>;
            using Traits = std::allocator_traits<A>;

            A a(alloc);
            try {
                word_t* ptr = Traits::allocate(a, words);
                jpm->total_memory += words * word_size;
                return ptr;
            } catch(const std::bad_alloc& e) {
                return nullptr;
            }
        }

        // raw free
        template<typename Allocator>
        void free_words(JPM* jpm, Allocator& alloc, void* ptr, std::size_t words) {
            using A = rebind_alloc_t<Allocator, word_t>;
            using Traits = std::allocator_traits<A>;
            
            if(!ptr) {
                return;
            }

            A a(alloc);

            Traits::deallocate(a, static_cast<word_t*>(ptr), words);
            jpm->total_memory -= words * word_size;
        }

        template<typename T, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        T* alloc_array(JPM* jpm, Allocator& alloc, std::size_t count) {
            const std::size_t words = allocation_words<CapacityPolicy>(count, sizeof(T));
            return reinterpret_cast<T*>(alloc_words(jpm, alloc, words));
        }

        template<typename T, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        void free_array(JPM* jpm, Allocator& alloc, T* ptr, std::size_t count) {
            const std::size_t words = allocation_words<CapacityPolicy>(count, sizeof(T));
            free_words(jpm, alloc, ptr, words);
        }

        template<typename T, typename Allocator>
        T* alloc_object(JPM* jpm, Allocator& alloc) {
            using A = rebind_alloc_t<Allocator, T>;
            using Traits = std::allocator_traits<A>;

            A a(alloc);
            try {
                T* ptr = Traits::allocate(a, 1);
                std::construct_at(ptr);
                jpm->total_memory += sizeof(T);
                return ptr;
            } catch(const std::bad_alloc& e) {
                return nullptr;
            }
        }

        template<typename T, typename Allocator>
        void free_object(JPM* jpm, Allocator& alloc, T* ptr) {
            using A = rebind_alloc_t<Allocator, T>;
            using Traits = std::allocator_traits<A>;
            
            if(!ptr) {
                return;
            }

            A a(alloc);
            std::destroy_at(ptr);
            Traits::deallocate(a, ptr, 1);
            jpm->total_memory -= sizeof(T);
        }

        template<typename Allocator>
        JPM* alloc_jpm(Allocator& alloc) {
            using A = rebind_alloc_t<Allocator, JPM>;
            using Traits = std::allocator_traits<A>;

            A a(alloc);
            try {
                JPM* jpm = Traits::allocate(a, 1);
                std::construct_at(jpm);
                zero_memory(jpm, sizeof(JPM));
                jpm->total_memory = sizeof(JPM);
                return jpm;
            } catch(const std::bad_alloc& e) {
                return nullptr;
            }
        }

        template<typename Allocator>
        void free_jpm(JPM* jpm, Allocator& alloc) {
            using A = rebind_alloc_t<Allocator, JPM>;
            using Traits = std::allocator_traits<A>;
            
            if(!jpm) {
                return;
            }

            A a(alloc);
            std::destroy_at(jpm);
            Traits::deallocate(a, jpm, 1);
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        //requires policy::JudyArrayPolicy<JudyPolicy> &&
        requires JudyCapacityPolicy<CapacityPolicy>
        void* alloc_jll(JPM* jpm, Allocator& alloc, std::size_t pop1, std::size_t level) {
            using A = rebind_alloc_t<Allocator, word_t>;
            using Traits = std::allocator_traits<A>;

            const std::size_t words = jll_allocation_words<JudyPolicy, CapacityPolicy>(pop1, level);

            A a(alloc);
            try {
                word_t* jll = Traits::allocate(a, words);
                //std::construct_at(jll);
                jpm->total_memory += words * word_size;
                return static_cast<void*>(jll);
            } catch(const std::bad_alloc& e) {
                return nullptr;
            }
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        //requires policy::JudyArrayPolicy<JudyPolicy> &&
        requires JudyCapacityPolicy<CapacityPolicy>
        void free_jll(JPM* jpm, Allocator& alloc, void* jll, std::size_t pop1, std::size_t level) {
            using A = rebind_alloc_t<Allocator, word_t>;
            using Traits = std::allocator_traits<A>;
            
            if(!jll) {
                return;
            }

            const std::size_t words = jll_allocation_words<JudyPolicy, CapacityPolicy>(pop1, level);

            A a(alloc);
            //std::destroy_at(jll);
            Traits::deallocate(a, reinterpret_cast<word_t*>(jll), words);
            jpm->total_memory -= words * word_size;
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        void free_jlb_subexpanse(JPM* jpm, Allocator& alloc, typename JudyPolicy::JLB* jlb, std::size_t subexpanse) {
            if constexpr(JudyPolicy::has_values) {
                word_t* values = JudyPolicy::jlb_values(jlb, subexpanse);
                if(values) {
                    std::size_t pop1 = std::popcount(JudyPolicy::jlb_bitmap(jlb, subexpanse));
                    memory::free_array<word_t, CapacityPolicy>(jpm, alloc, values, pop1);
                    JudyPolicy::jlb_set_values(jlb, subexpanse, nullptr);
                }
            }
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        void free_jlb(JPM* jpm, Allocator& alloc, typename JudyPolicy::JLB* jlb) {
            for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                free_jlb_subexpanse<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb, subexpanse);
            }
            memory::free_object<typename JudyPolicy::JLB>(jpm, alloc, jlb);
        }

        template<typename JudyPolicy>
        struct JllLimits {
            static constexpr word_t leaf_max_words = JudyPolicy::jll_max_cache_lines * CACHE_LINE_SIZE_WORDS;
        
            static constexpr std::array<word_t, root_level + 1> max_pop1 = [] {
                std::array<word_t, root_level + 1> result{};

                for(std::size_t level = 1; level <= root_level; ++level) {
                    const word_t absolute_limit = (level == 1) ? LEAF1_LINEAR_MAX : LEAF_LINEAR_MAX;

                    while(result[level] < absolute_limit && JudyPolicy::jll_words(level, result[level] + 1) <= leaf_max_words) {
                        ++result[level];
                    }
                }
                return result;
            }();
        };
        
        template<typename JudyPolicy>
        inline constexpr word_t leaf_max_pop1(std::size_t level) {
            return JllLimits<JudyPolicy>::max_pop1[level];
        }

        template<typename JudyPolicy>
        inline constexpr word_t branch_to_leaf_threshold(std::size_t level) {
            const word_t max_pop1 = leaf_max_pop1<JudyPolicy>(level);
            return (HYSTERESYS_BRANCH_TO_LEAF >= max_pop1 ? 0 : (max_pop1 - HYSTERESYS_BRANCH_TO_LEAF));
        }

        
        inline constexpr word_t branch_bitmap_to_linear_threshold() {
            return ((HYSTERESYS_BRANCH_BITMAP_TO_LINEAR >= BRANCH_LINEAR_MAX) ? 0 
                    : (BRANCH_LINEAR_MAX - HYSTERESYS_BRANCH_BITMAP_TO_LINEAR));
        }

        inline constexpr word_t branch_uncompressed_to_bitmap_threshold() {
            return ((HYSTERESYS_BRANCH_UNCOMPRESSED_TO_BITMAP >= BRANCH_BITMAP_MAX) ? 0 
                    : (BRANCH_BITMAP_MAX - HYSTERESYS_BRANCH_UNCOMPRESSED_TO_BITMAP));
        }

        inline constexpr word_t branch_uncompressed_to_linear_threshold() {
            return ((HYSTERESYS_BRANCH_UNCOMPRESSED_TO_LINEAR >= BRANCH_LINEAR_MAX) ? 0 
                    : (BRANCH_LINEAR_MAX - HYSTERESYS_BRANCH_UNCOMPRESSED_TO_LINEAR));
        }

        template<typename JudyPolicy>
        inline constexpr word_t leaf_bitmap_to_linear_threshold() {
            const word_t max_pop1 = leaf_max_pop1<JudyPolicy>(1);
            return ((HYSTERESYS_LEAF_BITMAP_TO_LINEAR >= max_pop1) ? 0 
                    : (max_pop1 - HYSTERESYS_LEAF_BITMAP_TO_LINEAR));
        }

    } // namespace memory

    enum class OpStatus {
        found,
        not_found,
        inserted,
        existed,
        erased,
        error,
    };

    struct CoreResult {
        OpStatus status;
        word_t* value; // nullptr for Judy1
    };

    inline constexpr word_t error_word = ~word_t{0};

    inline word_t* error_value() {
        return reinterpret_cast<word_t*>(error_word);
    }

    namespace policy {

        struct Judy1;
        struct JudyL;

        /*
            main idea of Policies: replace uncomfortable macroses from original
        */

        template<typename Policy>
        concept JudyArrayPolicy = requires(JP* jp, const JP* cjp, void* leaf, 
                            typename Policy::JLB* jlb,
                            const typename Policy::JLB* cjlb,
                            std::size_t subexpanse, word_t bitmask, word_t* pointer, std::size_t level, 
                            std::size_t pop1, std::size_t pos, word_t index, word8_t type) {
            
            { Policy::has_values }      -> std::convertible_to<bool>;
            { Policy::jll_max_cache_lines } -> std::convertible_to<std::size_t>;

            // JPImmed
            { Policy::immed_max_cnt(level) }    -> std::same_as<word8_t>;
            { Policy::immed_keys(   jp, pop1) } -> std::same_as<word8_t*>;
            { Policy::immed_keys(  cjp, pop1) } -> std::same_as<const word8_t*>;
            { Policy::immed_values( jp, pop1) } -> std::same_as<word_t*>;
            { Policy::immed_values(cjp, pop1) } -> std::same_as<const word_t*>;

            // JLL
            { Policy::jll_key_words(level, pop1) }      -> std::same_as<std::size_t>;
            { Policy::jll_words(level, pop1) }          -> std::same_as<std::size_t>;
            { Policy::jll_keys(leaf, level, pop1) }     -> std::same_as<word8_t*>;
            { Policy::template jll_capacity<DefaultCapacityPolicy>(level, pop1) }             -> std::same_as<std::size_t>;
            { Policy::template jll_values<DefaultCapacityPolicy>(leaf, level, pop1) }         -> std::same_as<word_t*>;
            { Policy::template jll_can_insert_inplace<DefaultCapacityPolicy>(level, pop1) }   -> std::convertible_to<bool>;
            { Policy::template jll_can_delete_inplace<DefaultCapacityPolicy>(level, pop1) }   -> std::convertible_to<bool>;
            
            // JLB
            { Policy::jlb_bitmap   (cjlb,   subexpanse) }          -> std::same_as<word_t>;
            { Policy::jlb_values   (cjlb,   subexpanse) }          -> std::same_as<word_t*>;
            { Policy::jlb_set_bit   (jlb,   subexpanse, bitmask) } -> std::same_as<void>;
            { Policy::jlb_clear_bit (jlb,   subexpanse, bitmask) } -> std::same_as<void>;
            { Policy::jlb_set_bitmap(jlb,   subexpanse, bitmask) } -> std::same_as<void>;
            { Policy::jlb_set_values(jlb,   subexpanse, pointer) } -> std::same_as<void>;

        };  
        
        struct Judy1Policy {
            
            using JLB = Judy1JLB;

            static constexpr bool has_values = false;
            static constexpr std::size_t jll_max_cache_lines = JUDY1_LEAF_LINEAR_CL_MAX;

            static constexpr word8_t immed_max_cnt(std::size_t k) {
                return static_cast<word8_t>((2 * word_size - 1) / k);
            }

            static const word8_t* immed_keys(const JP* jp, std::size_t pop1) {
                return reinterpret_cast<const word8_t*>(pop1 == 1 ? &jp->raw.w[1] : &jp->raw.w[0]);
            }

            static word8_t* immed_keys(JP* jp, std::size_t pop1) {
                return const_cast<word8_t*>(immed_keys(static_cast<const JP*>(jp), pop1));
            }

            static const word_t* immed_values(const JP* jp, std::size_t pop1) {
                return nullptr;
            }

            static word_t* immed_values(JP* jp, std::size_t pop1) {
                return nullptr;
            }

            static word_t jlb_bitmap(const JLB* jlb, std::size_t subexpanse) {
                return jlb->subexpanses[subexpanse];
            }

            static word_t* jlb_values(const JLB* jlb, std::size_t subexpanse) {
                return nullptr;
            }

            static void jlb_set_bit(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse] |= bitmask;
            }

            static void jlb_clear_bit(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse] &= bitmask;
            }

            static void jlb_set_bitmap(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse] = bitmask;
            }

            static void jlb_set_values(JLB* jlb, std::size_t subexpanse, word_t* pointer) {
                ;
            }

            static constexpr std::size_t jll_key_words(std::size_t level, std::size_t pop1) {
                return ((level * pop1 + word_size - 1) / word_size);
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static std::size_t jll_capacity(std::size_t level, std::size_t pop1) {
                return CapacityPolicy::get_capacity(jll_words(level, pop1));
            }

            static constexpr std::size_t jll_words(std::size_t level, std::size_t pop1) {
                return jll_key_words(level, pop1);
            }

            static word8_t* jll_keys(void* jll, std::size_t level, std::size_t pop1) {
                return reinterpret_cast<word8_t*>(jll);
            }
            
            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static word_t* jll_values(void* jll, std::size_t level, std::size_t pop1) {
                return nullptr;
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static bool jll_can_insert_inplace(std::size_t level, std::size_t pop1) {
                return CapacityPolicy::get_capacity(jll_words(level, pop1)) == 
                       CapacityPolicy::get_capacity(jll_words(level, pop1 + 1));
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static bool jll_can_delete_inplace(std::size_t level, std::size_t pop1) {
                return CapacityPolicy::get_capacity(jll_words(level, pop1)) == 
                       CapacityPolicy::get_capacity(jll_words(level, pop1 - 1));
            }

        };

        struct JudyLPolicy {

            using JLB = JudyLJLB;

            static constexpr bool has_values = true;

            static constexpr std::size_t jll_max_cache_lines = JUDYL_LEAF_LINEAR_CL_MAX;

            static constexpr word8_t immed_max_cnt(std::size_t k) {
                return static_cast<word8_t>((word_size - 1) / k);
            }

            static word_t jlb_bitmap(const JLB* jlb, std::size_t subexpanse) {
                return jlb->subexpanses[subexpanse].bitmap;
            }

            static word_t* jlb_values(const JLB* jlb, std::size_t subexpanse) {
                return jlb->subexpanses[subexpanse].values;
            }

            static void jlb_set_bit(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse].bitmap |= bitmask;
            }

            static void jlb_clear_bit(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse].bitmap &= bitmask;
            }
            
            static void jlb_set_bitmap(JLB* jlb, std::size_t subexpanse, word_t bitmask) {
                jlb->subexpanses[subexpanse].bitmap = bitmask;
            }

            static void jlb_set_values(JLB* jlb, std::size_t subexpanse, word_t* pointer) {
                jlb->subexpanses[subexpanse].values = pointer;
            }

            static const word8_t* immed_keys(const JP* jp, std::size_t pop1) {
                return reinterpret_cast<const word8_t*>(&jp->raw.w[1]);
            }

            static word8_t* immed_keys(JP* jp, std::size_t pop1) {
                return const_cast<word8_t*>(immed_keys(static_cast<const JP*>(jp), pop1));
            }

            static const word_t* immed_values(const JP* jp, std::size_t pop1) {
                if(pop1 == 1) {
                    return &jp->raw.w[0];
                }
                return reinterpret_cast<const word_t*>(jp->raw.w[0]);
            }

            static word_t* immed_values(JP* jp, std::size_t pop1) {
                return const_cast<word_t*>(immed_values(static_cast<const JP*>(jp), pop1));
            }

            static constexpr std::size_t jll_key_words(std::size_t level, std::size_t pop1) {
                return ((level * pop1 + word_size - 1) / word_size);
            }

            static constexpr std::size_t jll_words(std::size_t level, std::size_t pop1) {
                return jll_key_words(level, pop1) + pop1;
            }

            static word8_t* jll_keys(void* jll, std::size_t level, std::size_t pop1) {
                return reinterpret_cast<word8_t*>(jll);
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static std::size_t jll_capacity(std::size_t level, std::size_t pop1) {
                return  CapacityPolicy::get_capacity(jll_words(level, pop1));
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static word_t* jll_values(void* jll, std::size_t level, std::size_t pop1) {
                std::size_t capacity = jll_capacity<CapacityPolicy>(level, pop1) * word_size;
                std::size_t max_keys = capacity / (word_size + level);

                // values starts after max_keys * level + alignment to words
                word8_t* base = reinterpret_cast<word8_t*>(jll);
                return reinterpret_cast<word_t*>(base + jll_key_words(level, max_keys) * word_size);
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static bool jll_can_insert_inplace(std::size_t level, std::size_t pop1) {
                return CapacityPolicy::get_capacity(jll_words(level, pop1)) == 
                       CapacityPolicy::get_capacity(jll_words(level, pop1 + 1));
            }

            template<typename CapacityPolicy>
            requires JudyCapacityPolicy<CapacityPolicy>
            static bool jll_can_delete_inplace(std::size_t level, std::size_t pop1) {
                return CapacityPolicy::get_capacity(jll_words(level, pop1)) == 
                       CapacityPolicy::get_capacity(jll_words(level, pop1 - 1));
            }

        };

        template<JudyArrayPolicy JudyPolicy>
        struct JudyReturn;

        template<>
        struct JudyReturn<Judy1Policy> {

            using get_type = int;
            using insert_type = int;
            using erase_type = int;

            static get_type get(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return -1;
                }
                return (result.status == OpStatus::found ? 1 : 0);
            }

            static insert_type insert(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return -1;
                }
                return (result.status == OpStatus::inserted ? 1 : 0);
            }

            static erase_type erase(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return -1;
                }
                return (result.status == OpStatus::erased ? 1 : 0);
            }
        };

        template<>
        struct JudyReturn<JudyLPolicy> {

            using get_type = word_t*;
            using insert_type = word_t*;
            using erase_type = int;

            static get_type get(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return error_value();
                }

                return (result.status == OpStatus::found ? result.value : nullptr);
            }

            static insert_type insert(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return error_value();
                }
                return result.value;
            }

            static erase_type erase(CoreResult result) {
                if(result.status == OpStatus::error) {
                    return -1;
                }
                return (result.status == OpStatus::erased ? 1 : 0);
            }
        };

    };



    namespace detail {

        constexpr word_t all_ones_mask = ~static_cast<word_t>(0);
        constexpr word_t first_byte_mask = 0xFF;
        constexpr word_t n_byte_mask(std::size_t n) { return first_byte_mask << (CHAR_BIT * n); }
        constexpr word_t last_byte_mask = n_byte_mask(word_size - 1);
        constexpr word_t decode_pop0_mask = all_ones_mask >> CHAR_BIT;
        
        constexpr word_t first_n_bytes_mask(std::size_t n) { 
            return (n == word_size ? all_ones_mask : (static_cast<word_t>(1) << (CHAR_BIT * n)) - 1); }

        constexpr word_t bit_pos_mask(std::size_t bit) { 
            return static_cast<word_t>(1) << (bit % word_bits_size); }
        
        constexpr word_t pop0_mask(std::size_t level) {
            return decode_pop0_mask & first_n_bytes_mask(level); }
        
        constexpr word_t decode_mask(std::size_t level) { 
            return decode_pop0_mask & (~pop0_mask(level)); }
        
        inline word_t get_decode(JP* jp, std::size_t level) { return jp->raw.w[1] & decode_mask(level); }
        inline word_t get_pop0(JP* jp, std::size_t level)   { return jp->raw.w[1] & pop0_mask(level); }        
        
        inline void set_decode(JP* jp, word_t decode, std::size_t level) {
            jp->raw.w[1] = (jp->raw.w[1] & ~decode_mask(level)) | (decode & decode_mask(level)); }
        
        inline void set_pop0(JP* jp, word_t pop0, std::size_t level) {
            jp->raw.w[1] = (jp->raw.w[1] & ~pop0_mask(level)) | (pop0 & pop0_mask(level)); }
        
        template<policy::JudyArrayPolicy JudyPolicy>
        inline void set_immed1(JP* jp, const word_t index, std::size_t level, word_t value) {
            jp->raw.w[0] = value;
            jp->raw.w[1] = (index & first_n_bytes_mask(level)) | 
            ((static_cast<word_t>(types::JPImmed_t<JudyPolicy>(level, 1))) << (CHAR_BIT * (word_size - 1))); 
        }
        
        // Compare if index in JpImmed1
        inline bool immed1_matches(const JP* jp, std::size_t level, word_t index) {
            const word_t mask = first_n_bytes_mask(level);
            return (jp->raw.w[1] & mask) == (index & mask);
        }
        
        inline void set_jp(JP* jp, word_t addr, word_t index, word_t pop0, word_t type, std::size_t level) {
            jp->raw.w[0] = addr;
            jp->raw.w[1] = (index & decode_mask(level)) | (pop0 & pop0_mask(level)) 
            | (type << (CHAR_BIT * (word_size - 1)));
        }

        inline void set_null_jp(JP* jp) { jp->raw.w[0] = jp->raw.w[1] = 0; }

        inline bool decode_not_matched(const word_t index, const JP* jp, std::size_t level) {
            return (index ^ jp->raw.w[1]) & decode_mask(level); }
        
        inline word_t get_byte(const word_t index, std::size_t n) { 
            return (index >> (n * CHAR_BIT)) & first_byte_mask; }
        
        template<typename T>
        inline T* get_ptr(const word_t addr) {
            return reinterpret_cast<T*>(addr);
        }

        // read key from leaf
        inline word_t read_key(const word8_t* arr, std::size_t pos, std::size_t level) {
            word_t ans = 0;
            std::memcpy(&ans, arr + (pos * level), level);
            return ans;
        }

        // DEBUG FUNCTION (maybe  be useful for count?)
        template<policy::JudyArrayPolicy JudyPolicy>
        word_t calc_pop1(JP* jp) {
            using JLB = typename JudyPolicy::JLB;

            word_t ans = 0;

            switch(jp->node.type) {
                case types::Null_JP: return 0;
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    JBL* jbl = get_ptr<JBL>(jp->node.addr);
                    word_t pop1 = jbl->pop0 + 1;
                    for(std::size_t offset = 0; offset < pop1; ++offset) {
                        ans += calc_pop1<JudyPolicy>(&jbl->pointers[offset]);
                    }
                    break;
                }
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    JBB* jbb = get_ptr<JBB>(jp->node.addr);
                
                    for(word_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                        word_t subexpanse = digit / word_bits_size;
                        word_t bitmap = jbb->subexpanses[subexpanse].bitmap;
                        if(bitmap & bit_pos_mask(digit)) {
                            JP* pointers = jbb->subexpanses[subexpanse].pointers;
                            std::size_t offset = std::popcount(bitmap & (bit_pos_mask(digit) - 1));
                            ans += calc_pop1<JudyPolicy>(&pointers[offset]);
                        }
                    }
                    break;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    JBU* jbu = get_ptr<JBU>(jp->node.addr);

                    for(word_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                        ans += calc_pop1<JudyPolicy>(&jbu->pointers[digit]);
                    }
                    break;
                }
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    std::size_t level = jp->node.type - types::JLL_Base + 1;

                    ans += get_pop0(jp, level) + 1;
                    break;
                }
                case types::RLL: {
                    ans += get_pop0(jp, root_level) + 1;
                    break;
                }
                case types::JLB_Base: {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    for(word_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        ans += std::popcount(JudyPolicy::jlb_bitmap(jlb, subexpanse));
                    }
                    
                    break;
                }
                case types::JLB_FULL: {
                    ans += EXPANSE_COUNT;
                    break;
                }
                case types::JPIMMED_Base...types::JP_max_type<JudyPolicy>: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type<JudyPolicy> && 
                        jp->node.type >= (types::JPIMMED_Base + types::immed_level<JudyPolicy>(k + 1))) {
                        ++k;
                    }
                    word_t cnt = jp->node.type - types::JPImmed_t<JudyPolicy>(k, 1) + 1;

                    ans += cnt;
                    break;
                }
                default: {
                    assert(false && "INVALID LEVEL OF JP");
                }
            }

            return ans;
        }

        struct SearchResult {
            bool found;
            word_t pos;
        };

        /*
            template<typename T>
            constexpr bool IsPowerOfTwo = (sizeof(T) & (sizeof(T) - 1)) == 0;
        */

        // this is NONATIVE, should make if constexpr or native?
        template<typename T>
        SearchResult leaf_binary_search(T* arr, word_t pop1, std::size_t level, word_t index) {
            const word8_t* base = static_cast<const word8_t*>(arr);
            const word_t search_key = index & first_n_bytes_mask(level);

            word_t left = all_ones_mask;
            word_t right = pop1;
            word_t mid;
            word_t mid_val;

            while (right - left > 1) {
                mid = (right + left) >> 1;
                
                mid_val = 0;
                // is compiler do optimization for word_t?
                std::memcpy(&mid_val, base + (mid * level), level);

                if(mid_val > search_key) {
                    right = mid;
                } else {
                    left = mid;
                }
            }

            if(left == all_ones_mask) {
                return SearchResult{false, right};
            }

            mid_val  = 0;
            std::memcpy(&mid_val, base + (left * level), level);
            
            return mid_val == search_key    ? SearchResult{true,    left} 
                                            : SearchResult{false,   right};
        }

        // same nonative
        template<typename T>
        SearchResult leaf_linear_search(T* arr, word_t pop1, std::size_t level, word_t index) {
            const word8_t* base = static_cast<const word8_t*>(arr);
            const word_t search_key = index & first_n_bytes_mask(level);

            word_t curr;
            for(word_t i = 0; i < pop1; ++i) {
                curr = 0;
                std::memcpy(&curr, base + (i * level), level);
                
                if(curr == search_key) {
                    return SearchResult{true, i};
                } else if(curr > search_key) {
                    return SearchResult{false, i};
                }              
            }

            return SearchResult{false, pop1};
        }

        template<typename T>
        inline void insert_inplace(T* arr, word_t pop1, std::size_t element_size, const T* element, std::size_t pos) {
            word8_t* base = reinterpret_cast<word8_t*>(arr);
            const word8_t* element_base = reinterpret_cast<const word8_t*>(element);

            std::memmove(base + (pos + 1) * element_size, base + pos * element_size, 
                            (pop1 - pos) * element_size);
            std::memcpy(base + pos * element_size, element_base, element_size);
        }

        template<typename T>
        inline void insert_copy(T* destination, const T* source, word_t pop1, std::size_t element_size, 
                                                            const T* element, std::size_t pos) {
            word8_t* destination_base = reinterpret_cast<word8_t*>(destination);
            const word8_t* source_base = reinterpret_cast<const word8_t*>(source);
            const word8_t* element_base = reinterpret_cast<const word8_t*>(element);
            if(pos > 0) {
                std::memcpy(destination_base, source_base, pos * element_size);
            }
            std::memcpy(destination_base + (pos * element_size), element_base, element_size);
            if(pos < pop1) {
                std::memcpy(destination_base + (pos + 1) * element_size, source_base + pos * element_size, 
                                                (pop1 - pos) * element_size);
            }
        }
        
        template<typename T, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        T* insert_with_realloc(JPM* jpm, Allocator& alloc, T* arr, word_t old_pop1, std::size_t element_size, 
                               const T* element, std::size_t pos) {
            std::size_t need_capacty = memory::allocation_bytes<CapacityPolicy>(old_pop1 + 1, element_size);
            T* new_arr = memory::alloc_array<T, CapacityPolicy>(jpm, alloc, old_pop1 + 1);
            if(!new_arr) {
                return nullptr;
            }

            insert_copy<T>(new_arr, arr, old_pop1, element_size, element, pos);

            return new_arr;
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        void* jll_insert_with_realloc(JPM* jpm, Allocator& alloc, void* jll, std::size_t old_pop1, std::size_t level, 
                                      const word8_t* key, word_t
                                       value, std::size_t pos) {
            std::size_t need_capacity = memory::jll_allocation_bytes<JudyPolicy, CapacityPolicy>(old_pop1 + 1, level);

            void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, old_pop1 + 1, level);
            if(!new_jll) {
                return nullptr;
            }

            insert_copy<word8_t>(JudyPolicy::jll_keys(new_jll, level, old_pop1 + 1), 
                                 JudyPolicy::jll_keys(jll, level, old_pop1), 
                                 old_pop1, level, key, pos);

            if constexpr(JudyPolicy::has_values) {
                word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(new_jll, level, old_pop1 + 1);
                word_t* old_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, old_pop1);
                insert_copy<word_t>(new_values, old_values, old_pop1, word_size, &value, pos);
            }
            return new_jll;
        }

        
        template<typename T>
        inline void delete_inplace(T* arr, word_t pop1, std::size_t element_size, std::size_t pos)  {
            if(pos < pop1 - 1) {
                word8_t* base = reinterpret_cast<word8_t*>(arr);
                std::memmove(base + pos * element_size, base + (pos + 1) * element_size, 
                            (pop1 - pos - 1) * element_size);
            }
        }

        template<typename T>
        void delete_copy(T* destination, const T* source, word_t pop1, std::size_t element_size,
                         std::size_t pos) {
            word8_t* destination_base = reinterpret_cast<word8_t*>(destination);
            const word8_t* source_base = reinterpret_cast<const word8_t*>(source);

            if(pos > 0) {
                std::memcpy(destination_base, source_base, pos * element_size);
            }
            if(pos + 1 < pop1) {
                std::memcpy(destination_base + pos * element_size, source_base + (pos + 1) * element_size, 
                                                (pop1 - pos - 1) * element_size);
            }
        }
        
        template<typename T, typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        T* delete_with_realloc(JPM* jpm, Allocator& alloc, T* arr, word_t old_pop1, std::size_t element_size, std::size_t pos) {
            T* new_arr = memory::alloc_array<T, CapacityPolicy>(jpm, alloc, old_pop1 - 1);
            if(!new_arr) {
                return nullptr;
            }

            delete_copy<T>(new_arr, arr, old_pop1, element_size, pos);

            return new_arr;
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        void* jll_delete_with_realloc(JPM* jpm, Allocator& alloc, void* jll, std::size_t old_pop1, std::size_t level, std::size_t pos) {
            void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, old_pop1 - 1, level);
            if(!new_jll) {
                return nullptr;
            }

            delete_copy<word8_t>(JudyPolicy::jll_keys(new_jll, level, old_pop1 - 1), 
                                 JudyPolicy::jll_keys(jll, level, old_pop1), 
                                 old_pop1, level,pos);

            if constexpr(JudyPolicy::has_values) {
                word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(new_jll, level, old_pop1 - 1);
                word_t* old_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, old_pop1);
                delete_copy<word_t>(new_values, old_values, old_pop1, word_size, pos);
            }
            return new_jll;
        }

        
        template<typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        inline bool allocate_jbb_pointers(JPM* jpm, Allocator& alloc, JBB* jbb, word8_t* subexpanse_pops) {


            for(std::size_t i = 0; i < SUBEXPANSE_COUNT; ++i) {
                if(subexpanse_pops[i]) {
                    JP* new_arr = memory::alloc_array<JP, CapacityPolicy>(jpm, alloc, subexpanse_pops[i]);

                    jbb->subexpanses[i].pointers = new_arr;
                    
                    if(!jbb->subexpanses[i].pointers) {
                        for(word_t j = 0; j < i; ++j) {
                            if(jbb->subexpanses[j].pointers) {
                                memory::free_array<JP, CapacityPolicy>(jpm, alloc, jbb->subexpanses[j].pointers, subexpanse_pops[j]);
                            }
                        }

                        return false;
                    }
                }
            }

            return true;
        }
        
        template<typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        JBB* build_jbb_from_linear(JPM* jpm, Allocator& alloc, word8_t* digits, JP* pointers, std::size_t pop1) {
            word_t offset, digit, subexpanse;
            
            // alloc JBB
            JBB* jbb = memory::alloc_object<JBB>(jpm, alloc);
            if(!jbb) {
                return nullptr;
            }
            memory::zero_memory(jbb, sizeof(JBB));

            // 1 pass: calculate how many needs each subexpanse
            word8_t subexpanse_pops[SUBEXPANSE_COUNT] = {0};
            
            for(offset = 0; offset < pop1; ++offset) {
                digit = digits[offset];
                subexpanse = digit / word_bits_size;
                ++subexpanse_pops[subexpanse];
            }

            bool res = allocate_jbb_pointers<CapacityPolicy>(jpm, alloc, jbb, subexpanse_pops);
            if(!res) {
                memory::free_object<JBB>(jpm, alloc, jbb);
                return nullptr;
            }
            
            // 2 pass: copy
            word8_t current_idx[SUBEXPANSE_COUNT] = {0};
            for(offset = 0; offset < pop1; ++offset) {
                digit = digits[offset];
                subexpanse = digit / word_bits_size;

                word8_t idx = current_idx[subexpanse]++;
                jbb->subexpanses[subexpanse].pointers[idx] = pointers[offset];
                jbb->subexpanses[subexpanse].bitmap |= bit_pos_mask(digit);
            }

            return jbb;
        }


        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> && 
        JudyCapacityPolicy<CapacityPolicy>
        JP* insert_branch_outlier(JPM* jpm, Allocator& alloc, JP* jp, word_t index, std::size_t level) {
            
            JP* inserted_jp = nullptr;
            word_t xor_diff = ((index ^ jp->raw.w[1]) & decode_mask(level)) >> (level * CHAR_BIT);
        
            std::size_t highest_nonzero_bit = (word_bits_size - 1) - std::countl_zero(xor_diff);
            std::size_t branch_level = level + (highest_nonzero_bit / CHAR_BIT) + 1;
            
            word8_t old_byte = get_byte(jp->raw.w[1], branch_level - 1);
            word8_t new_byte = get_byte(index, branch_level - 1);
            
            JBL* jbl = memory::alloc_object<JBL>(jpm, alloc);
            if(!jbl) {
                return nullptr;
            }

            JP new_jp;
            // 0 - Judy1, value - JudyL.
            set_immed1<JudyPolicy>(&new_jp, index, branch_level - 1, 0);

            JP old_jp = *jp;
            
            if(old_byte < new_byte) {
                jbl->keys[0] = old_byte;
                jbl->keys[1] = new_byte;
                jbl->pointers[0] = old_jp;
                jbl->pointers[1] = new_jp;
                inserted_jp = &jbl->pointers[1];
            } else {
                jbl->keys[0] = new_byte;
                jbl->keys[1] = old_byte;
                jbl->pointers[0] = new_jp;
                jbl->pointers[1] = old_jp;
                inserted_jp = &jbl->pointers[0];
            }

            jbl->pop0 = 1;

            // common_prefix = index & decode_mask(branch_level);
            word_t new_pop0 = get_pop0(jp, level) + 1;
            set_jp(jp, reinterpret_cast<word_t>(jbl), index, new_pop0, types::JBL_Level(branch_level), branch_level);

            return inserted_jp;
        }

        // JLL1 -> JLB1
        // JLL2 -> JLB1
        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        typename JudyPolicy::JLB* build_jlb_from_jll(JPM* jpm, Allocator& alloc, void* jll, word_t pop1, std::size_t level, 
                                                     std::size_t start, std::size_t count) {
            using JLB = typename JudyPolicy::JLB;

            JLB* jlb = memory::alloc_object<JLB>(jpm, alloc);
            if(!jlb) {
                return nullptr;
            }
            memory::zero_memory(jlb, sizeof(JLB));
            
            word8_t* jll_keys = JudyPolicy::jll_keys(jll, level, pop1);
        
            std::size_t subexpanse_pops[SUBEXPANSE_COUNT] = {0};
            for(std::size_t offset = 0; offset < count; ++offset) {
                word_t digit = jll_keys[(start + offset) * level]; // only minor byte
                std::size_t subexpanse = digit / word_bits_size;

                JudyPolicy::jlb_set_bit(jlb, subexpanse, bit_pos_mask(digit));
                ++subexpanse_pops[subexpanse];
            }

            if constexpr(JudyPolicy::has_values) {
                word_t* jll_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);

                for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                    if(subexpanse_pops[subexpanse] == 0) {
                        continue;
                    }
                    word_t* jlb_values = memory::alloc_array<word_t, CapacityPolicy>(jpm, alloc, subexpanse_pops[subexpanse]);

                    if(!jlb_values) {
                        memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
                        return nullptr;
                    }
                    JudyPolicy::jlb_set_values(jlb, subexpanse, jlb_values);
                }

                std::size_t write_offset[SUBEXPANSE_COUNT] = {0};

                for(std::size_t offset = 0; offset < count; ++offset) {
                    word_t digit = jll_keys[(start + offset) * level]; 
                    word_t subexpanse = digit / word_bits_size;

                    word_t* jlb_values = JudyPolicy::jlb_values(jlb, subexpanse);
                    jlb_values[write_offset[subexpanse]++] = jll_values[start + offset];
                }
            }

            return jlb;
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        inline void free_jll_cascade(JPM* jpm, Allocator& alloc, JP* pointers, std::size_t count) {
            using JLB = typename JudyPolicy::JLB;

            for(std::size_t i = 0; i < count; ++i) {
                JP* jp = &pointers[i];
                
                if(jp->node.type >= types::JLL_Level(1) && jp->node.type <= types::JLL_Level(root_level - 1)) {
                    void* jll = get_ptr<void>(jp->node.addr);
                    std::size_t level = jp->node.type - types::JLL_Base + 1;
                    std::size_t pop1 = get_pop0(jp, level) + 1;
                    memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                } else if(jp->node.type == types::JLB_Base) {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
                }
            }
        }

        /*
            JLL1 -> JLB1
            JLLX -> JLL(X-1) + narrow
            JLLX -> JLB/JBB(X) + JLL(X-1)/JLB1
        */
        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        bool cascade(JPM* jpm, Allocator& alloc, JP* jp, std::size_t level, word_t index_to_insert) {
            using JLB = typename JudyPolicy::JLB;
      
            void* jll = get_ptr<void>(jp->node.addr);
            word_t pop1 = get_pop0(jp, level) + 1;
            word8_t* old_keys = JudyPolicy::jll_keys(jll, level, pop1);
            word_t* old_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);
            word_t old_decode = get_decode(jp, level);

            // JLL1 -> JLB1
            if(level == 1) {
                JLB* jlb = build_jlb_from_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level, 0, pop1);
                if(!jlb) {
                    return false;
                }

                set_jp(jp, reinterpret_cast<word_t>(jlb), old_decode, pop1 - 1, types::JLB_Base, 1);  
                memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                return true;
            }   

            word_t first_key = read_key(old_keys, 0, level);
            word_t last_key = read_key(old_keys, pop1 - 1, level);
            const std::size_t child_level = level - 1;

            // Everyone has the same major byte => take it outside and reduce leaf level by 1
            // (Except RLL-case)
            if(get_byte(first_key, level - 1) == get_byte(last_key, level - 1) && (level != root_level)) {

                // JLL2 -> JLB1
                if(child_level == 1) {
                    JLB* jlb = build_jlb_from_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level, 0, pop1);
                    if(!jlb) {
                        return false;
                    }
                    
                    // need to check again
                    word_t new_decode = old_decode | (get_byte(first_key, level - 1) << (CHAR_BIT * child_level));
                    set_jp(jp, reinterpret_cast<word_t>(jlb), new_decode, pop1 - 1, types::JLB_Base, 1);
                    
                    memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                    return true;
                }

                void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, pop1, child_level);
                if(!new_jll) {
                    return false;
                }
                word8_t* new_keys = JudyPolicy::jll_keys(new_jll, child_level, pop1);
                word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(new_jll, child_level, pop1);

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(new_keys + (offset * child_level), old_keys + (offset * level), child_level);
                    if constexpr(JudyPolicy::has_values) {
                        std::memcpy(new_values + offset, old_values + offset, word_size);
                    }
                }

                memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                
                word_t new_decode = old_decode | (get_byte(first_key, level - 1) << (CHAR_BIT * child_level));
                set_jp(jp, reinterpret_cast<word_t>(new_jll), new_decode, pop1 - 1, types::JLL_Level(child_level), child_level);
                
                return true;
            }

            // See final convertation
            word_t index_byte = get_byte(index_to_insert, child_level);
            bool index_in_branch = false;

            // Collect common prefixes
            JP pointers[LEAF_LINEAR_MAX]; // make zero?
            word8_t digits[LEAF_LINEAR_MAX];

            std::size_t start = 0, end = 0, subexpanse = 0;
            while(start < pop1) {
                
                word_t start_key = read_key(old_keys, start, level);
                word_t top_byte = get_byte(start_key, child_level);
                index_in_branch |= (top_byte == index_byte);
                
                end = start + 1;
                while(end < pop1 && get_byte(read_key(old_keys, end, level), child_level) == top_byte) {
                    ++end;
                }

                std::size_t subexpanse_pop1 = end - start;
                digits[subexpanse] = top_byte;
                JP* new_jp = &pointers[subexpanse];

                // Immed1
                if(subexpanse_pop1 == 1) {
                    word_t value = 0;
                    if constexpr(JudyPolicy::has_values) {
                        value = old_values[start];
                    }
                    set_immed1<JudyPolicy>(new_jp, start_key, child_level, value);                    
                } 
                // Immed2+
                else if(subexpanse_pop1 <= JudyPolicy::immed_max_cnt(child_level)) {
                    word_t* new_values = nullptr;
                    if constexpr(JudyPolicy::has_values) {
                        new_values = memory::alloc_words(jpm, alloc, subexpanse_pop1);
                        if(!new_values) {
                            return false;
                        }
                    }
                    set_null_jp(new_jp);
                    
                    word8_t* new_keys = JudyPolicy::immed_keys(new_jp, subexpanse_pop1);
                    for(std::size_t offset = 0; offset < subexpanse_pop1; ++offset) {
                        std::memcpy(new_keys + (offset * child_level), old_keys + ((start + offset) * level), child_level);
                        if constexpr(JudyPolicy::has_values) {
                           new_values[offset] = old_values[start + offset];
                        }
                    }
                    if constexpr(JudyPolicy::has_values) {
                        new_jp->raw.w[0] = reinterpret_cast<word_t>(new_values);
                    }
                    new_jp->node.type = types::JPImmed_t<JudyPolicy>(child_level, subexpanse_pop1);
                } 
                // JLB
                else if(child_level == 1 && subexpanse_pop1 > LEAF1_LINEAR_MAX) {
                    JLB* jlb = build_jlb_from_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level, start, subexpanse_pop1);
                    if(!jlb) {
                        free_jll_cascade<JudyPolicy, CapacityPolicy>(jpm, alloc, pointers, subexpanse);
                        return false;
                    }

                    word_t new_decode = old_decode | (top_byte << (CHAR_BIT * child_level));
                    set_jp(new_jp, reinterpret_cast<word_t>(jlb), new_decode, subexpanse_pop1 - 1, types::JLB_Base, child_level);
                }
                // JLL
                else {
                    void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, subexpanse_pop1, child_level);
                    if(!new_jll) {
                        free_jll_cascade<JudyPolicy, CapacityPolicy>(jpm, alloc, pointers, subexpanse);
                        return false;
                    }
                    word8_t* new_keys = JudyPolicy::jll_keys(new_jll, child_level, subexpanse_pop1);
                    word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(new_jll, child_level, subexpanse_pop1);
                    
                    for(std::size_t offset = 0; offset < subexpanse_pop1; ++offset) {
                        std::memcpy(new_keys + (offset * child_level), old_keys + ((start + offset) * level), child_level);    
                        if constexpr(JudyPolicy::has_values) {
                            new_values[offset] = old_values[start + offset];
                        }
                    }                    

                    word_t new_decode = old_decode | (top_byte << (CHAR_BIT * child_level));
                    set_jp(new_jp, reinterpret_cast<word_t>(new_jll), new_decode, subexpanse_pop1 - 1, types::JLL_Level(child_level), child_level);
                }
                ++subexpanse;
                start = end;
            }

            /**
             * Original doesnt take into account the moment when the branch size is equal 
             * to the maximum for JBL. As a result, after cascade, we will have to do the
             * JBL->JBB conversion again. Here, i calculated in advance while building the
             * branch, whether the new index would fall into the branch or create a new branch
             */
            bool is_jbl = subexpanse < BRANCH_LINEAR_MAX || (subexpanse == BRANCH_LINEAR_MAX && index_in_branch);

            if(is_jbl) {
                JBL* jbl = memory::alloc_object<JBL>(jpm, alloc);
                if(!jbl) {
                    free_jll_cascade<JudyPolicy, CapacityPolicy>(jpm, alloc, pointers, subexpanse);
                    return false;
                }

                std::memcpy(jbl->keys, digits, subexpanse);
                std::memcpy(jbl->pointers, pointers, subexpanse * sizeof(JP));
                jbl->pop0 = subexpanse - 1;

                jp->node.addr = reinterpret_cast<word_t>(jbl);
                jp->node.type = types::JBL_Level(level);                
            } else {
                JBB* jbb = build_jbb_from_linear<CapacityPolicy>(jpm, alloc, digits, pointers, subexpanse);
                if(!jbb) {
                    free_jll_cascade<JudyPolicy, CapacityPolicy>(jpm, alloc, pointers, subexpanse);
                    return false;
                }

                jp->node.addr = reinterpret_cast<word_t>(jbb);
                jp->node.type = types::JBB_Level(level);
            }
            
            memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
            
            return 1;
        }

        
        // Universal function that copies from any JPImmed/JLL/JLB to JLL
        /*
            jll -> where to copy
            pos -> jll position where starts copying
            top_byte -> major byte for paste
            jp -> from where copy
            level -> jll level
        */
        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        inline void copy_immedleaf_to_jll(JPM* jpm, Allocator& alloc, void* jll, std::size_t& pos, word8_t top_byte,
                                         JP* jp, std::size_t level, std::size_t node_pop1) {
            using JLB = typename JudyPolicy::JLB;

            const std::size_t child_level = level - 1;
            const word8_t type = jp->node.type;
           
            std::size_t new_offset = pos / level;
            word8_t* new_keys = JudyPolicy::jll_keys(jll, level, node_pop1);
            word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, node_pop1);


            if(type == types::JLL_Level(child_level)) {
                void* leaf = get_ptr<void>(jp->node.addr);
                const word_t pop1 = get_pop0(jp, child_level) + 1;
                word8_t* old_keys = JudyPolicy::jll_keys(leaf, child_level, pop1);
                word_t* old_values = JudyPolicy::template jll_values<CapacityPolicy>(leaf, child_level, pop1);

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(new_keys + pos, old_keys + offset * child_level, child_level);
                    if constexpr(JudyPolicy::has_values) {
                        new_values[new_offset++] = old_values[offset];
                    }

                    new_keys[pos + child_level] = top_byte;
                    pos += level;
                }
                memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, leaf, pop1, child_level);
            } 
            else if(type >= types::JPImmed_t<JudyPolicy>(child_level, 1) &&
                    type <= types::JPImmed_t<JudyPolicy>(child_level, JudyPolicy::immed_max_cnt(child_level))) {
                word_t pop1 = type - types::JPImmed_t<JudyPolicy>(child_level, 1) + 1;
                word8_t* old_keys = JudyPolicy::immed_keys(jp, pop1);
                word_t* old_values = JudyPolicy::immed_values(jp, pop1);

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(new_keys + pos, old_keys + offset * child_level, child_level);
                    if constexpr(JudyPolicy::has_values) {
                        new_values[new_offset++] = old_values[offset]; 
                    }
                    
                    new_keys[pos + child_level] = top_byte;
                    pos += level;
                }
                if constexpr(JudyPolicy::has_values) {
                    if(pop1 > 1) {
                        memory::free_words(jpm, alloc, old_values, pop1);
                    }
                }
            }
            else if(type == types::JLB_Base && child_level == 1) {
                JLB* jlb = get_ptr<JLB>(jp->node.addr);

                std::size_t pop1 = 0;
                for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                    pop1 += std::popcount(JudyPolicy::jlb_bitmap(jlb, subexpanse));
                }
                
                for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                    word_t bitmap = JudyPolicy::jlb_bitmap(jlb, subexpanse);
                    word_t* old_values = JudyPolicy::jlb_values(jlb, subexpanse);
                    
                    std::size_t old_offset = 0;
                    while(bitmap) {
                        const std::size_t first_bit = std::countr_zero(bitmap);
                        const word8_t digit = static_cast<word8_t>(subexpanse * word_bits_size + first_bit);
                        
                        if constexpr(JudyPolicy::has_values) {
                            new_values[new_offset++] = old_values[old_offset++];
                        }
                    
                        new_keys[pos++] = digit;
                        new_keys[pos++] = top_byte;

                        bitmap &= (bitmap - 1);
                    }
                }

                memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
            } 
            else /* should never happen */ { 
                assert(false);
            }
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        void jbl_copy_traverse(JPM* jpm, Allocator& alloc, JP* jp, void* jll, std::size_t level, std::size_t node_pop1) {
            JBL* jbl = get_ptr<JBL>(jp->node.addr);
            std::size_t jll_pos = 0;
            word_t jbl_pop1 = jbl->pop0 + 1;

            for(std::size_t offset = 0; offset < jbl_pop1; ++offset) {
                
                word8_t top_byte = jbl->keys[offset];
                JP* curr_jp = &jbl->pointers[offset];
                
                copy_immedleaf_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, jll_pos, top_byte, curr_jp, level, node_pop1);
            }
        }

        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        void jbb_copy_traverse(JPM* jpm, Allocator& alloc, JP* jp, void* jll, std::size_t level, std::size_t node_pop1) {
            JBB* jbb = get_ptr<JBB>(jp->node.addr);
            std::size_t jll_pos = 0;

            for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                word_t bitmap = jbb->subexpanses[subexpanse].bitmap;
                if(!bitmap) {
                    continue;
                }

                word_t pop1 = std::popcount(bitmap);
                JP* pointers = jbb->subexpanses[subexpanse].pointers;
                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    JP* curr_jp = pointers + offset;
                    word8_t digit = static_cast<word8_t>(subexpanse * word_bits_size + std::countr_zero(bitmap));
                    copy_immedleaf_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, jll_pos, digit, curr_jp, level, node_pop1);
                    
                    bitmap &= (bitmap - 1);
                }
                memory::free_array<JP, CapacityPolicy>(jpm, alloc, pointers, pop1);
            }
        }
         
        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        void jbu_copy_traverse(JPM* jpm, Allocator& alloc, JP* jp, void* jll, std::size_t level, std::size_t node_pop1) {
            JBU* jbu = get_ptr<JBU>(jp->node.addr);
            std::size_t jll_pos = 0;

            for(std::size_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                
                JP* curr_jp = &jbu->pointers[digit];
                if(curr_jp->node.type == types::Null_JP) {
                    continue;
                }

                copy_immedleaf_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, jll_pos, digit, curr_jp, level, node_pop1);
            }
        }

        // True, if index in current immed/leaf
        template<typename JudyPolicy>
        requires policy::JudyArrayPolicy<JudyPolicy>
        inline bool check_immedleaf_index(JP* jp, word_t index, std::size_t level) {
            using JLB = typename JudyPolicy::JLB;

            const word8_t type = jp->node.type;

            if(type == types::JLL_Level(level)) {
                void* jll = get_ptr<void>(jp->node.addr);
                const word_t pop1 = get_pop0(jp, level) + 1;

                return leaf_binary_search<word8_t>(JudyPolicy::jll_keys(jll, level, pop1), pop1, level, index).found;
            } 
            else if(type >= types::JPImmed_t<JudyPolicy>(level, 1) &&
                    type <= types::JPImmed_t<JudyPolicy>(level, JudyPolicy::immed_max_cnt(level))) {
                word_t pop1 = type - types::JPImmed_t<JudyPolicy>(level, 1) + 1;
                word8_t* keys = JudyPolicy::immed_keys(jp, pop1);

                return (pop1 == 1 ? immed1_matches(jp, level, index) : leaf_linear_search<word8_t>(keys, pop1, level, index).found);
            }
            else if(type == types::JLB_Base) {
                JLB* jlb = get_ptr<JLB>(jp->node.addr);
                word_t digit = get_byte(index, level - 1);
                word_t subexpanse = digit / word_bits_size;
                
                return JudyPolicy::jlb_bitmap(jlb, subexpanse) & bit_pos_mask(digit);
            } 
            else /* should never happen */ { 
                assert(false);
                return false;
            }
        }

        template<policy::JudyArrayPolicy JudyPolicy>
        bool jbl_check_traverse(const JP* jp, word_t index, std::size_t level) {
            JBL* jbl = get_ptr<JBL>(jp->node.addr);
            word_t digit = get_byte(index, level - 1);
            word_t jbl_pop1 = jbl->pop0 + 1;

            std::size_t offset = 0;
            while(offset < jbl_pop1 && jbl->keys[offset] < digit) {
                ++offset;
            }
            
            return ((offset == jbl_pop1 || jbl->keys[offset] != digit) 
                ? false    
                : check_immedleaf_index<JudyPolicy>(&jbl->pointers[offset], index, level - 1));
        }

        template<policy::JudyArrayPolicy JudyPolicy>
        bool jbb_check_traverse(const JP* jp, word_t index, std::size_t level) {
            JBB* jbb = get_ptr<JBB>(jp->node.addr);
            word_t digit = get_byte(index, level - 1);
            word_t subexpanse = digit / word_bits_size;
            word_t bitmap = jbb->subexpanses[subexpanse].bitmap;
            word_t bitmask = bit_pos_mask(digit);
            if(!bitmap || !(bitmap & bitmask)) {
                return false;
            }

            std::size_t offset = std::popcount(bitmap & (bitmask - 1));
            JP* curr_jp = &jbb->subexpanses[subexpanse].pointers[offset];
            return check_immedleaf_index<JudyPolicy>(curr_jp, index, level - 1);
        }
         
        template<policy::JudyArrayPolicy JudyPolicy>
        bool jbu_check_traverse(const JP* jp, word_t index, std::size_t level) {
            JBU* jbu = get_ptr<JBU>(jp->node.addr);
            word_t digit = get_byte(index, level - 1);
            JP* curr_jp = &jbu->pointers[digit];

            return (curr_jp->node.type == types::Null_JP 
                ? false 
                : check_immedleaf_index<JudyPolicy>(curr_jp, index, level - 1));
        }

        // Make common traverse?
        template<policy::JudyArrayPolicy JudyPolicy>
        bool index_under_branch(const JP* jp, const word_t index, std::size_t level) {
           if(jp->node.type == types::JBL_Level(level)) {
                return jbl_check_traverse<JudyPolicy>(jp, index, level);
            } else if(jp->node.type == types::JBB_Level(level)) {
                return jbb_check_traverse<JudyPolicy>(jp, index, level);
            } else if(jp->node.type == types::JBU_Level(level)) {
                return jbu_check_traverse<JudyPolicy>(jp, index, level);
            }
            // for crying compilers
            return false; 
        }

        // JB* -> JLL at same level
        template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        bool branch_to_jll(JPM* jpm, Allocator& alloc, JP* jp, word_t node_pop1, std::size_t level) {
            std::size_t need_capacity = memory::jll_allocation_bytes<JudyPolicy, CapacityPolicy>(node_pop1, level);
            void* jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, node_pop1, level);
            if(!jll) {
                return false;
            }

            // logic
            if(jp->node.type == types::JBL_Level(level)) {
                jbl_copy_traverse<JudyPolicy, CapacityPolicy>(jpm, alloc, jp, jll, level, node_pop1);
                memory::free_object<JBL>(jpm, alloc, get_ptr<JBL>(jp->node.addr));
            } else if(jp->node.type == types::JBB_Level(level)) {
                jbb_copy_traverse<JudyPolicy, CapacityPolicy>(jpm, alloc, jp, jll, level, node_pop1);
                memory::free_object<JBB>(jpm, alloc, get_ptr<JBB>(jp->node.addr));
            } else if(jp->node.type == types::JBU_Level(level)) {
                jbu_copy_traverse<JudyPolicy, CapacityPolicy>(jpm, alloc, jp, jll, level, node_pop1);
                memory::free_object<JBU>(jpm, alloc, get_ptr<JBU>(jp->node.addr));
            } 
            
            set_jp(jp, reinterpret_cast<word_t>(jll), get_decode(jp, level), 
                   node_pop1 - 1, types::JLL_Level(level), level);
            
            return true;
        }

        // convert jbb to jbl
        // 0 - no memory
        // 1 - success
        template<typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        bool jbb_to_jbl(JPM* jpm, Allocator& alloc, JP* jp, std::size_t level) {
            JBB* jbb = get_ptr<JBB>(jp->node.addr);

            JBL* jbl = memory::alloc_object<JBL>(jpm, alloc);
            if(!jbl) {
                return false;
            }

            std::size_t pos = 0;
            for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {

                word_t bitmap = jbb->subexpanses[subexpanse].bitmap;
                if(!bitmap) {
                    continue;
                }
                
                word_t pop1 = std::popcount(bitmap);
                JP* pointers = jbb->subexpanses[subexpanse].pointers;

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    word8_t digit = static_cast<word8_t>(subexpanse * word_bits_size + std::countr_zero(bitmap));
                    jbl->keys[pos] = digit;
                    jbl->pointers[pos++] = pointers[offset];
                    bitmap &= (bitmap - 1);
                }
                
                memory::free_array<JP, CapacityPolicy>(jpm, alloc, pointers, pop1);
            }
            jbl->pop0 = pos - 1;

            memory::free_object<JBB>(jpm, alloc, jbb);
            
            jp->node.addr = reinterpret_cast<word_t>(jbl);
            jp->node.type = types::JBL_Level(level);

            return true;
        }

        // JBU->JBB (level can be removed and calculates from jp?)
        template<typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        bool jbu_to_jbb(JPM* jpm, Allocator& alloc, JP* jp, std::size_t level) {

            word_t digit, subexpanse;

            JBB* jbb = memory::alloc_object<JBB>(jpm, alloc);
            if(!jbb) {
                return false;
            }
            memory::zero_memory(jbb, sizeof(JBB));

            JBU* jbu = get_ptr<JBU>(jp->node.addr);

            word8_t subexpanse_pops[SUBEXPANSE_COUNT] = {0};
            for(digit = 0; digit < EXPANSE_COUNT; ++digit) {
                if(jbu->pointers[digit].node.type == types::Null_JP) {
                    continue;
                }
                subexpanse = digit / word_bits_size;
                ++subexpanse_pops[subexpanse];
            }           

            bool res = allocate_jbb_pointers<CapacityPolicy>(jpm, alloc, jbb, subexpanse_pops);
            if(!res) {
                memory::free_object<JBB>(jpm, alloc, jbb);
                return false;
            }

            std::size_t subexpanse_pos[SUBEXPANSE_COUNT] = {0};
            for(digit = 0; digit < EXPANSE_COUNT; ++digit) {
                if(jbu->pointers[digit].node.type == types::Null_JP) {
                    continue;
                }

                subexpanse = digit / word_bits_size;
                jbb->subexpanses[subexpanse].pointers[subexpanse_pos[subexpanse]++] = jbu->pointers[digit];
                jbb->subexpanses[subexpanse].bitmap |= bit_pos_mask(digit);
            }

            memory::free_object<JBU>(jpm, alloc, jbu);
            
            jp->node.addr = reinterpret_cast<word_t>(jbb);
            jp->node.type = types::JBB_Level(level);

            return true;
        }

        template<typename CapacityPolicy, typename Allocator>
        requires JudyCapacityPolicy<CapacityPolicy>
        bool jbu_to_jbl(JPM* jpm, Allocator& alloc, JP* jp, std::size_t level) {

            JBL* jbl = memory::alloc_object<JBL>(jpm, alloc);
            if(!jbl) {
                return false;
            }

            JBU* jbu = get_ptr<JBU>(jp->node.addr);
            std::size_t pos = 0;

            for(std::size_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                if(jbu->pointers[digit].node.type == types::Null_JP) {
                    continue;
                }
                jbl->keys[pos] = digit;
                jbl->pointers[pos++] = jbu->pointers[digit];
            }

            jbl->pop0 = pos - 1;

            memory::free_object<JBU>(jpm, alloc, jbu);
            
            jp->node.addr = reinterpret_cast<word_t>(jbl);
            jp->node.type = types::JBL_Level(level);

            return true;
        }

        // only free, not making 0
        template<policy::JudyArrayPolicy JudyPolicy, typename CapacityPolicy, typename Allocator>
        requires policy::JudyArrayPolicy<JudyPolicy> &&
        JudyCapacityPolicy<CapacityPolicy>
        word_t free_jp(JPM* jpm, Allocator& alloc, JP* jp) {
            using JLB = typename JudyPolicy::JLB;

            word_t pop1, total_free = 0;

            switch(jp->node.type) {
                case types::Null_JP: {
                    break;
                }
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    JBL* jbl = get_ptr<JBL>(jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    for(std::size_t offset = 0; offset < pop1; ++offset) {
                        total_free += free_jp<JudyPolicy, CapacityPolicy>(jpm, alloc, &jbl->pointers[offset]);
                    }
                    memory::free_object<JBL>(jpm, alloc, jbl);
                    break;
                }
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    JBB* jbb = get_ptr<JBB>(jp->node.addr);

                    for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        if(!jbb->subexpanses[subexpanse].bitmap) {
                            continue;
                        }
                        pop1 = std::popcount(jbb->subexpanses[subexpanse].bitmap);
                        JP* pointers = jbb->subexpanses[subexpanse].pointers;
                        for(std::size_t offset = 0; offset < pop1; ++offset) {
                            total_free += free_jp<JudyPolicy, CapacityPolicy>(jpm, alloc, &pointers[offset]);
                        }
                        memory::free_array<JP, CapacityPolicy>(jpm, alloc, jbb->subexpanses[subexpanse].pointers, pop1);
                    }
                    memory::free_object<JBB>(jpm, alloc, jbb);
                    break;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    
                    JBU* jbu = get_ptr<JBU>(jp->node.addr);
                    
                    for(std::size_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                        if(jbu->pointers[digit].node.type == types::Null_JP) {
                            continue;
                        }
                        total_free += free_jp<JudyPolicy, CapacityPolicy>(jpm, alloc, &jbu->pointers[digit]);
                    }

                    memory::free_object<JBU>(jpm, alloc, jbu);
                    break;
                }
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    std::size_t level = jp->node.type - types::JLL_Base + 1;
                    pop1 = get_pop0(jp, level) + 1;

                    void* jll = get_ptr<void>(jp->node.addr);

                    memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                    total_free = pop1;
                    break;
                }
                case types::RLL: {
                    std::size_t level = root_level;
                    pop1 = get_pop0(jp, level) + 1;
                    void* rll = get_ptr<void>(jp->node.addr);
                    memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, rll, pop1, level);
                    total_free = pop1;
                    break;
                }
                case types::JLB_Base: {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    pop1 = 0;
                    for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        pop1 += std::popcount(JudyPolicy::jlb_bitmap(jlb, subexpanse));
                    }
                    memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
                    total_free = pop1;
                    break;
                }
                case types::JLB_FULL: {
                    total_free = EXPANSE_COUNT;
                    break;
                }
                case types::JPIMMED_Base...types::JP_max_type<JudyPolicy>: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type<JudyPolicy> && 
                        jp->node.type >= (types::JPIMMED_Base + types::immed_level<JudyPolicy>(k + 1))) {
                        ++k;
                    }
                    word_t cnt = jp->node.type - types::JPImmed_t<JudyPolicy>(k, 1) + 1;

                    if constexpr(JudyPolicy::has_values) {
                        if(cnt > 1) {
                            memory::free_words(jpm, alloc, JudyPolicy::immed_values(jp, cnt), cnt);
                        }
                    }

                    total_free = cnt;
                    break;
                }
                default: {
                    assert(false);
                    break;
                }

            }

            return total_free;
        }

    } // namespace detail

    namespace conversions {

        // jbu_to_jbl
        // jbu_to_jbb
        // jbb_to_jbl
        // jll_to_jlb?

    } // namespace conversions

    template<typename Allocator>
    JPM* judy_create(Allocator& alloc) {
        return memory::alloc_jpm(alloc);
    }

    template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    CoreResult judy_insert_core(JPM* jpm, Allocator& alloc, const word_t index) {
        word_t level = root_level, digit, offset, subexpanse, pop1, node_pop1, bitmap, bitmask;
        word_t* result_value = nullptr;
        using namespace detail;
        using JLB = typename JudyPolicy::JLB;

        JP* stack_jps               [word_size + 1];
        std::size_t stack_levels    [word_size + 1];

        std::size_t stack_pos = 0;

        if(!jpm) {
            return CoreResult{ OpStatus::error, nullptr };
        }

        JP* curr_jp = &jpm->top_jp;
     
        if(curr_jp->node.type == types::Null_JP) {
            /**
             * Note that JBL allocates with trash (no initialization).
             * Since we control the size using pop0 fields and use 
             * linear search, this will allow us not to go beyond the 
             * boundaries or into unwanted garbage.
             */
            void* rll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, 1, root_level);          
            
            if(!rll) {
                jpm->error.type = ErrorType::NoMem;
                return CoreResult{ OpStatus::error, nullptr };
            }

            word8_t* rll_keys = JudyPolicy::jll_keys(rll, root_level, 1);
            word_t* rll_values = JudyPolicy::template jll_values<CapacityPolicy>(rll, root_level, 1);
            
            std::memcpy(rll_keys, &index, word_size);
            if constexpr(JudyPolicy::has_values) {
                rll_values[0] = 0;
                result_value = &rll_values[0];
            }

            jpm->pop0 = 0;
            set_jp(&jpm->top_jp, reinterpret_cast<word_t>(rll), 0, 0, types::RLL, root_level);

            return CoreResult{ OpStatus::inserted, result_value };
        }

        while(true) {

            switch(curr_jp->node.type) {
                // only JBU
                case types::Null_JP: {
                    set_immed1<JudyPolicy>(curr_jp, index, level - 1, 0);
                    if constexpr(JudyPolicy::has_values) {
                        result_value = JudyPolicy::immed_values(curr_jp, 1);
                    }

                    goto WalkExit;
                }
                // JBL
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    level = curr_jp->node.type - types::JBL_Base + 2;
                    digit = get_byte(index, level - 1);   
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }

                        goto WalkExit;
                    }


                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    node_pop1 = get_pop0(curr_jp, level) + 1; 

                    // JBL -> JBU
                    if(node_pop1 > UNDER_BRANCH_LINEAR_MAX) {
                        JBU* jbu = memory::alloc_object<JBU>(jpm, alloc);
                        if(!jbu) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        memory::zero_memory(jbu, sizeof(JBU));

                        for(offset = 0; offset < pop1; ++offset) {
                            digit = jbl->keys[offset];
                            JP* digit_jp = &jbl->pointers[offset];

                            jbu->pointers[digit] = *digit_jp;
                        }
                        
                        memory::free_object<JBL>(jpm, alloc, jbl);
                        
                        curr_jp->node.addr = reinterpret_cast<word_t>(jbu);
                        curr_jp->node.type = types::JBU_Level(level);

                        // hysteresis
                        jpm->lastpop0 = jpm->pop0; 

                        continue;
                    }

                    SearchResult search = leaf_linear_search<word8_t>(jbl->keys, pop1, 1, digit);
                    
                    if(search.found) {
                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;
                        
                        curr_jp = jbl->pointers + search.pos;
                        continue;
                    }

                    if(pop1 < BRANCH_LINEAR_MAX) {

                        JP new_jp;
                        set_immed1<JudyPolicy>(&new_jp, index, level - 1, 0);

                        insert_inplace(jbl->keys, jbl->pop0 + 1UL, 1UL, 
                            reinterpret_cast<word8_t*>(&digit), search.pos);

                        insert_inplace<JP>(jbl->pointers, jbl->pop0 + 1UL, sizeof(JP), &new_jp, search.pos);                            

                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(jbl->pointers + search.pos, 1);
                        }
                        ++jbl->pop0;

                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;
                        
                        goto WalkExit;
                    } else {
                        // JBL -> JBB
                        
                        JBB* jbb = build_jbb_from_linear<CapacityPolicy>(jpm, alloc, jbl->keys, jbl->pointers, jbl->pop0 + 1);
                        if(!jbb) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        memory::free_object<JBL>(jpm, alloc, jbl);
                        
                        curr_jp->node.addr = reinterpret_cast<word_t>(jbb);
                        curr_jp->node.type = types::JBB_Level(level);

                        continue;
                    }
                    
                }
                // JBB
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }

                        goto WalkExit;
                    }
                    
                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);

                    
                    // add max jbb count? 180?
                    if(((jpm->pop0 - jpm->lastpop0) >= GLOBAL_HYSTERESYS)        &&
                        (jpm->pop0 >= MIN_ARRAY_SIZE_TO_UNCOMPRESSED)            &&
                        (get_pop0(curr_jp, level) >= UNDER_BRANCH_BITMAP_MAX)   )
                    {
                        
                        JBU* jbu = memory::alloc_object<JBU>(jpm, alloc);
                        if(!jbu) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        memory::zero_memory(jbu, sizeof(JBU));

                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            digit = subexpanse * word_bits_size;
                            
                            bitmap = jbb->subexpanses[subexpanse].bitmap;
                            pop1 = std::popcount(bitmap);
                            JP* jps = jbb->subexpanses[subexpanse].pointers;

                            if(!bitmap) {
                                continue;
                            }
                            
                            /*
                                whats faster: copy one by one or by ranges
                                is there any sense to make check on full bitmap like in original?
                            */
                            
                            for(word_t bit = 0, offset = 0; bit < word_bits_size; ++bit, bitmap >>= 1, ++digit) {
                                jbu->pointers[digit] = ((bitmap & 1) ? jps[offset++] : null_jp);
                            }
                            memory::free_array<JP, CapacityPolicy>(jpm, alloc, jps, pop1);
                        }
                        memory::free_object<JBB>(jpm, alloc, jbb);
                        
                        jpm->lastpop0 = jpm->pop0;

                        curr_jp->node.addr = reinterpret_cast<word_t>(jbu);
                        curr_jp->node.type = types::JBU_Level(level);
    
                        continue;
                    }
                    
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    bitmap = jbb->subexpanses[subexpanse].bitmap;
                    bitmask = bit_pos_mask(digit);
                    
                    if(bitmap & bitmask) {
                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;

                        offset = std::popcount(bitmap & (bitmask - 1));
                        curr_jp = jbb->subexpanses[subexpanse].pointers + offset;

                        continue;
                    }  
                    
                    JP* pointers = jbb->subexpanses[subexpanse].pointers;
                    pop1 = std::popcount(bitmap);
                    offset = std::popcount(bitmap & (bitmask - 1));

                    JP new_jp;
                    set_immed1<JudyPolicy>(&new_jp, index, level - 1, 0);
                    JP* inserted_jp = nullptr;

                    if(!bitmap) {
                        std::size_t need_capacity = memory::allocation_bytes<CapacityPolicy>(pop1 + 1, sizeof(JP));
                        JP* new_pointers = memory::alloc_array<JP, CapacityPolicy>(jpm, alloc, pop1 + 1);
                        if(!new_pointers) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        new_pointers[0] = new_jp;
                        inserted_jp = new_pointers;
                        jbb->subexpanses[subexpanse].pointers = new_pointers;
                    } else if(CapacityPolicy::can_insert_inplace(pop1, sizeof(JP))) { 
                        insert_inplace<JP>(pointers, pop1, sizeof(JP), &new_jp, offset);
                        inserted_jp = pointers + offset;
                    } else {
                        JP* new_pointers = insert_with_realloc<JP, CapacityPolicy>(jpm, alloc, pointers, pop1, sizeof(JP), &new_jp, offset);
                        if(!new_pointers) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        memory::free_array<JP, CapacityPolicy>(jpm, alloc, pointers, pop1);
                        inserted_jp = new_pointers + offset;
                        jbb->subexpanses[subexpanse].pointers = new_pointers;
                    }

                    jbb->subexpanses[subexpanse].bitmap |= bitmask;

                    if constexpr(JudyPolicy::has_values) {
                        result_value = JudyPolicy::immed_values(inserted_jp, 1);
                    }

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                // JBU
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;
                    digit = get_byte(index, level - 1);

                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }

                        goto WalkExit;
                    }

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    curr_jp = jbu->pointers + digit;

                    continue;
                }
                // JLL/RLL
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1):
                case types::RLL: {
                    level = curr_jp->node.type - types::JLL_Base + 1;
                    pop1 = get_pop0(curr_jp, level) + 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        
                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }

                        goto WalkExit;
                    }

                    void* jll = get_ptr<word8_t>(curr_jp->node.addr);
                    word8_t* jll_keys = JudyPolicy::jll_keys(jll, level, pop1);
                    word_t* jll_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);
                    SearchResult search = leaf_binary_search<word8_t>(jll_keys, pop1, level, index);

                    if(search.found) {
                        return CoreResult{ OpStatus::existed, nullptr };
                    }

                    if((pop1) + 1 > memory::leaf_max_pop1<JudyPolicy>(level))
                        {
                        // JLL -> JBL/JBB + JLL/JLB
                        bool res = cascade<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, level, index);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        continue;
                    }

                    if(JudyPolicy::template jll_can_insert_inplace<CapacityPolicy>(level, pop1)) {
                        insert_inplace<word8_t>(jll_keys, pop1, level, 
                                            reinterpret_cast<const word8_t*>(&index), search.pos);
                        if constexpr(JudyPolicy::has_values) {
                            word_t value = 0;
                            insert_inplace<word_t>(jll_values, pop1, word_size, &value, search.pos);
                            result_value = jll_values + search.pos;
                        }
                    } else { 
                        
                        void* new_jll = jll_insert_with_realloc<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level, 
                                                     reinterpret_cast<const word8_t*>(&index), 0, search.pos);
                        if(!new_jll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        
                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::template jll_values<CapacityPolicy>(new_jll, level, pop1 + 1) + search.pos;
                        }
                        
                        memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                        curr_jp->node.addr = reinterpret_cast<word_t>(new_jll);
                    }

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                // JLB1
                case types::JLB_Base: {
                    level = 1;
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        
                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }
                        
                        goto WalkExit;
                    }
                    
                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;
                    bitmap = JudyPolicy::jlb_bitmap(jlb, subexpanse);

                    if(JudyPolicy::jlb_bitmap(jlb, subexpanse) & bit_pos_mask(digit)) {
                        return CoreResult{ OpStatus::existed, nullptr };
                    }

                    if constexpr(JudyPolicy::has_values) {
                        word_t value = 0;
                        word_t* values = JudyPolicy::jlb_values(jlb, subexpanse);

                        pop1 = std::popcount(bitmap);
                        offset = std::popcount(bitmap & (bit_pos_mask(digit) - 1));

                        if(pop1 == 0) {
                            values = memory::alloc_array<word_t, CapacityPolicy>(jpm, alloc, 1);

                            if(!values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }

                            values[0] = value;
                            JudyPolicy::jlb_set_values(jlb, subexpanse, values);
                            result_value = values;
                        } else if(CapacityPolicy::can_insert_inplace(pop1, word_size)) {
                            insert_inplace<word_t>(values, pop1, word_size, &value, offset);
                            result_value = values + offset;
                        } else {
                            word_t* new_values = insert_with_realloc<word_t, CapacityPolicy>(jpm, alloc, values, pop1, word_size, &value, offset);
                            if(!new_values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }

                            memory::free_array<word_t, CapacityPolicy>(jpm, alloc, values, pop1);
                            values = new_values;
                            JudyPolicy::jlb_set_values(jlb, subexpanse, values);
                            result_value = values + offset;
                        }
                    }

                    JudyPolicy::jlb_set_bit(jlb, subexpanse, bit_pos_mask(digit));

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    
                    pop1 = get_pop0(curr_jp, level) + 1;

                    // Judy1 only
                    if constexpr(!JudyPolicy::has_values) {
                        if(pop1 + 1 == EXPANSE_COUNT) {
                            memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
                            
                            set_pop0(curr_jp, EXPANSE_COUNT - 1, level);
                            curr_jp->node.type = types::JLB_FULL;
                            
                            --stack_pos; // jlb no more exists
                        }
                    } 

                    goto WalkExit;
                }
                // (Judy1 only)
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        JP* res = insert_branch_outlier<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, index, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        
                        if constexpr(JudyPolicy::has_values) {
                            result_value = JudyPolicy::immed_values(res, 1);
                        }

                        goto WalkExit;
                    }

                    return CoreResult{ OpStatus::existed, nullptr };
                }
                //JPIMMED
                case types::JPIMMED_Base...types::JP_max_type<JudyPolicy>: {

                    std::size_t k = 1;
                    while(k < types::JP_max_type<JudyPolicy> && 
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level<JudyPolicy>(k + 1))) {
                        ++k;
                    }
                    word_t cnt = curr_jp->node.type - types::JPImmed_t<JudyPolicy>(k, 1) + 1;

                    if((cnt == 1) && !((cnt + 1) > JudyPolicy::immed_max_cnt(k))) {
                        word_t old_index = read_key(JudyPolicy::immed_keys(curr_jp, 1), 0, k);
                        word_t new_index = index & first_n_bytes_mask(k);

                        if(old_index == new_index) {
                            return CoreResult{ OpStatus::existed, nullptr };
                        }
                        word_t old_value = 0;
                        word_t* new_values = nullptr;
                        if constexpr(JudyPolicy::has_values) {
                            old_value = *JudyPolicy::immed_values(curr_jp, 1);
                            new_values = memory::alloc_array<word_t, CapacityPolicy>(jpm, alloc, 2);
                            if(!new_values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }
                        }

                        word8_t* keys = JudyPolicy::immed_keys(curr_jp, 2);
                        if(old_index < new_index) {
                            std::memcpy(keys, &old_index, k);
                            std::memcpy(keys + k, &new_index, k);        
                            
                            if constexpr(JudyPolicy::has_values) {
                                new_values[0] = old_value;
                                new_values[1] = 0;
                                result_value = &new_values[1];
                            }

                        } else {
                            std::memcpy(keys, &new_index, k);
                            std::memcpy(keys + k, &old_index, k);
                        
                            if constexpr(JudyPolicy::has_values) {
                                new_values[0] = 0;
                                new_values[1] = old_value;
                                result_value = &new_values[0];
                            }
                        }

                        if constexpr(JudyPolicy::has_values) {
                            curr_jp->raw.w[0] = reinterpret_cast<word_t>(new_values);
                        }
                        curr_jp->node.type = types::JPImmed_t<JudyPolicy>(k, 2);

                        goto WalkExit;
                    }

                    
                    word8_t* keys = JudyPolicy::immed_keys(curr_jp, cnt);
                    SearchResult search = leaf_linear_search<word8_t>(keys, cnt, k, index);
                    
                    if(search.found) {
                        return CoreResult{ OpStatus::existed, nullptr };
                    }
                        
                    // jpimmed -> JLL
                    if((cnt + 1) > JudyPolicy::immed_max_cnt(k)) {
                        std::size_t need_capacity = memory::jll_allocation_bytes<JudyPolicy, CapacityPolicy>(cnt + 1, k);

                        void* jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, cnt + 1, k);
                        if(!jll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        insert_copy<word8_t>(JudyPolicy::jll_keys(jll, k, cnt + 1), keys, cnt, k, 
                                                reinterpret_cast<const word8_t*>(&index), search.pos);
                        
                        if constexpr(JudyPolicy::has_values) {
                            word_t value = 0;
                            word_t* old_values = JudyPolicy::immed_values(curr_jp, cnt);
                            word_t* new_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, k, cnt + 1);
                             
                            insert_copy<word_t>(new_values, old_values, cnt, word_size, &value, search.pos);
                            
                            result_value = new_values + search.pos;

                            if(cnt > 1) {
                                memory::free_words(jpm, alloc, old_values, cnt);
                            }
                        }

                        set_jp(curr_jp, reinterpret_cast<word_t>(jll), index, cnt, types::JLL_Level(k), k);
                    } else {
                        if constexpr(JudyPolicy::has_values) {
                            word_t value = 0;
                            word_t* old_values = JudyPolicy::immed_values(curr_jp, cnt);
                            word_t* new_values = memory::alloc_array<word_t, CapacityPolicy>(jpm, alloc, cnt + 1);

                            if(!new_values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }

                            insert_copy<word_t>(new_values, old_values, cnt, word_size, &value, search.pos);

                            memory::free_words(jpm, alloc, old_values, cnt);
                            curr_jp->raw.w[0] = reinterpret_cast<word_t>(new_values);
                            result_value = new_values + search.pos;
                        }
                        
                        insert_inplace<word8_t>(keys, cnt, k,
                                                reinterpret_cast<const word8_t*>(&index), search.pos);
                        curr_jp->node.type = types::JPImmed_t<JudyPolicy>(k, cnt + 1);
                    }

                    goto WalkExit;
                }
                default: {
                    jpm->error.type = ErrorType::Corrupt;
                    return CoreResult{ OpStatus::error, nullptr };
                }
            }
        }
        WalkExit:

        // unwinding the stack and +1 to pop0
        while(stack_pos-- > 0) {
            curr_jp = stack_jps[stack_pos];
            level = stack_levels[stack_pos];

            word_t new_pop0 = get_pop0(curr_jp, level) + 1;
            set_pop0(curr_jp, new_pop0, level);
        }

        ++jpm->pop0;
        
        return CoreResult{ OpStatus::inserted, result_value };
    }

    template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    typename policy::JudyReturn<JudyPolicy>::insert_type judy_insert(JPM* jpm, Allocator& alloc, const word_t index) {
        CoreResult result = judy_insert_core<JudyPolicy, CapacityPolicy>(jpm, alloc, index);
        return policy::JudyReturn<JudyPolicy>::insert(result);
    }

    // 0 - already deleted
    // 1 - succesfully
    // -1 - error (check jpm)
    // Original first do JudyTest and only then go down. In my case we do not do extra judy_test
    // and check element existing "on the way"
    template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    CoreResult judy_erase_core(JPM* jpm, Allocator& alloc, const word_t index) {
        word_t level = root_level, digit, offset, subexpanse, pop1, node_pop1, bitmap, bitmask;
        using namespace detail;
        using JLB = typename JudyPolicy::JLB;

        JP* stack_jps               [word_size + 1];
        std::size_t stack_levels    [word_size + 1];

        std::size_t stack_pos = 0;

        if(!jpm) {
            return CoreResult{ OpStatus::error, nullptr };
        }

        JP* curr_jp = &jpm->top_jp;

        while(true) {
            switch(curr_jp->node.type) {
                case types::Null_JP: {
                    return CoreResult{ OpStatus::not_found, nullptr };
                }
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    level = curr_jp->node.type - types::JBL_Base + 2;
                    digit = get_byte(index, level - 1);   
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    node_pop1 = get_pop0(curr_jp, level) + 1; 

                    // JBL -> JLL
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold<JudyPolicy>(level)) {
                        /* JBL -> JLL */
                        if(!index_under_branch<JudyPolicy>(curr_jp, index, level)) {
                            return CoreResult{ OpStatus::not_found, nullptr };
                        }
                        
                        bool res = branch_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        jpm->error.type = ErrorType::NoMem;
                        return CoreResult{ OpStatus::error, nullptr };
                    }
                    
                    SearchResult search = leaf_linear_search<word8_t>(jbl->keys, pop1, 1, digit);

                    if(!search.found) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    
                    JP* next_jp = jbl->pointers + search.pos;
                    // Check JPIMMED1 or no
                    // if no - go deeper
                    if(next_jp->node.type != types::JPImmed_t<JudyPolicy>(level - 1, 1)) {
                        curr_jp = next_jp;
                        continue;
                    }
                    
                    if(!immed1_matches(next_jp, level - 1, index)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    // if yes - delete and check what remains of JBL
                    delete_inplace<word8_t>(jbl->keys, jbl->pop0 + 1, 1, search.pos);
                    delete_inplace<JP>(jbl->pointers, jbl->pop0 + 1, sizeof(JP), search.pos);
                    if(jbl->pop0 == 0) {
                        memory::free_object<JBL>(jpm, alloc, jbl);
                        set_null_jp(curr_jp);
                        if(level == root_level) {
                            jpm->pop0 = 0;
                            return CoreResult{ OpStatus::erased, nullptr };
                        }
                        --stack_pos; // jbl deleted
                        goto WalkExit;
                    }
                    --jbl->pop0;

                    // level != root_level
                    if(level != root_level && jbl->pop0 == 0) {
                        // raising JP to parent
                        *curr_jp = jbl->pointers[0];

                        memory::free_object<JBL>(jpm, alloc, jbl);
                        --stack_pos; // jbl no more exists
                    }
                    goto WalkExit;
                }
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);
                    node_pop1 = get_pop0(curr_jp, level) + 1; 
                    
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold<JudyPolicy>(level)) {
                        /* JBB -> JLL */
                        if(!index_under_branch<JudyPolicy>(curr_jp, index, level)) {
                            return CoreResult{ OpStatus::not_found, nullptr };
                        }

                        bool res = branch_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        jpm->error.type = ErrorType::NoMem;
                        return CoreResult{ OpStatus::error, nullptr };
                    }

                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    bitmap = jbb->subexpanses[subexpanse].bitmap;
                    bitmask = bit_pos_mask(digit);
                    
                    if(!(bitmap & bitmask)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }  
                    
                    pop1 = std::popcount(bitmap);
                    offset = std::popcount(bitmap & (bitmask - 1));
                    JP* pointers = jbb->subexpanses[subexpanse].pointers;

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    // Big child - go to him
                    if(pointers[offset].node.type != types::JPImmed_t<JudyPolicy>(level - 1, 1)) {

                        curr_jp = pointers + offset;
                        continue;
                    }
                    
                    if(!immed1_matches(&pointers[offset], level - 1, index)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    if(pop1 == 1) {
                        memory::free_array<JP, CapacityPolicy>(jpm, alloc, pointers, pop1);
                        jbb->subexpanses[subexpanse].pointers = nullptr;
                    } else if(CapacityPolicy::can_delete_inplace(pop1, sizeof(JP))) {
                        delete_inplace<JP>(pointers, pop1, sizeof(JP), offset);
                    } else {
                        JP* new_arr = delete_with_realloc<JP, CapacityPolicy>(jpm, alloc, pointers, pop1, sizeof(JP), offset);
                        if(!new_arr) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        memory::free_array<JP, CapacityPolicy>(jpm, alloc, pointers, pop1);
                        jbb->subexpanses[subexpanse].pointers = new_arr;
                    }

                    jbb->subexpanses[subexpanse].bitmap &= ~bitmask;

                    if(pop1 > BRANCH_LINEAR_MAX) {
                        goto WalkExit;
                    } 

                    // what if JBL is turned off?
                    pop1 = 0;
                    for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        pop1 += std::popcount(jbb->subexpanses[subexpanse].bitmap);
                    }

                    if(pop1 == 0) {
                        memory::free_object<JBB>(jpm, alloc, jbb);
                        set_null_jp(curr_jp);
                        if(level == root_level) {
                            jpm->pop0 = 0;
                            return CoreResult{ OpStatus::erased, nullptr };
                        }
                        --stack_pos;
                    }
                    else if(pop1 <= memory::branch_bitmap_to_linear_threshold()) {
                        // jbb_to_jbl
                        bool res = jbb_to_jbl<CapacityPolicy>(jpm, alloc, curr_jp, level);
                        if(!res) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                    }

                    goto WalkExit;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;
                    digit = get_byte(index, level - 1);

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);
                    node_pop1 = get_pop0(curr_jp, level) + 1; 
                    
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold<JudyPolicy>(level)) {
                        /* JBU -> JLL */
                        if(!index_under_branch<JudyPolicy>(curr_jp, index, level)) {
                            return CoreResult{ OpStatus::not_found, nullptr };
                        }
                        
                        bool res = branch_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        jpm->error.type = ErrorType::NoMem;
                        return CoreResult{ OpStatus::error, nullptr };
                    }

                    if(jbu->pointers[digit].node.type == types::JPImmed_t<JudyPolicy>(level - 1, 1)) {
                        
                        if(!immed1_matches(&jbu->pointers[digit], level - 1, index)) {
                            return CoreResult{ OpStatus::not_found, nullptr };
                        }

                        jbu->pointers[digit].node.type = types::Null_JP;

                        if(get_pop0(curr_jp, level) == 0) {
                            memory::free_object<JBU>(jpm, alloc, jbu);
                            set_null_jp(curr_jp);
                            if(level == root_level) {
                                jpm->pop0 = 0;
                                return CoreResult{ OpStatus::erased, nullptr };
                            }
                            
                            goto WalkExit;
                        }
                        
                        // JBU->JBB
                        // maybe somewhere store pop1 of jbu?
                        if constexpr(memory::branch_uncompressed_to_bitmap_threshold()) {
                            word_t pop1 = 0;
                            for(offset = 0; offset < EXPANSE_COUNT; ++offset) {
                                pop1 += (jbu->pointers[offset].node.type != types::Null_JP);
                            }

                            if(pop1 <= memory::branch_uncompressed_to_bitmap_threshold()) {
                                bool res = jbu_to_jbb<CapacityPolicy>(jpm, alloc, curr_jp, level);
                                if(!res) {
                                    jpm->error.type = ErrorType::NoMem;
                                    return CoreResult{ OpStatus::error, nullptr };
                                }
                            }
                        }
                        // JBU -> JBL
                        else if constexpr(memory::branch_uncompressed_to_linear_threshold()) {
                            word_t pop1 = 0;
                            for(offset = 0; offset < EXPANSE_COUNT; ++offset) {
                                pop1 += (jbu->pointers[offset].node.type != types::Null_JP);
                            }

                            if(pop1 <= memory::branch_uncompressed_to_linear_threshold()) {
                                bool res = jbu_to_jbl<CapacityPolicy>(jpm, alloc, curr_jp, level);
                                if(!res) {
                                    jpm->error.type = ErrorType::NoMem;
                                    return CoreResult{ OpStatus::error, nullptr };
                                }
                            }
                        }

                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;

                        goto WalkExit;
                    }

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    curr_jp = jbu->pointers + digit;

                    continue;
                }
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    word_t parent_level = level;
                    level = curr_jp->node.type - types::JLL_Base + 1;
                    pop1 = get_pop0(curr_jp, level) + 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    void* jll = get_ptr<word8_t>(curr_jp->node.addr);
                    word8_t* jll_keys = JudyPolicy::jll_keys(jll, level, pop1);
                    word_t* jll_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);
                    SearchResult search = leaf_binary_search<word8_t>(jll_keys, pop1, level, index);

                    if(!search.found) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    // jll->jll_up_level
                    if((parent_level - 1) > level && (pop1 == memory::leaf_max_pop1<JudyPolicy>(level + 1))) {
                        void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, pop1, level + 1);
                        if(!new_jll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        std::size_t pos = 0;
                        // jll free inside
                        copy_immedleaf_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, new_jll, pos, get_byte(index, level), curr_jp, level + 1, pop1);
                    
                        set_jp(curr_jp, reinterpret_cast<word_t>(new_jll), get_decode(curr_jp, level + 1),
                            pop1 - 1, types::JLL_Level(level + 1), level + 1);

                        continue;
                    } 

                    // jll IMMED
                    if((pop1 - 1) == JudyPolicy::immed_max_cnt(level)) {
                        if(pop1 == 2)  {
                            word_t value = 0;
                            if constexpr(JudyPolicy::has_values) {
                                word_t* old_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);
                                value = old_values[(search.pos ? 0 : 1)];
                            }
                            word_t immed_key = 0;
                            std::memcpy(&immed_key, jll_keys + (search.pos ? 0 : level), level);
                            set_null_jp(curr_jp);
                            set_immed1<JudyPolicy>(curr_jp, immed_key, level, value);
                        } else {
                            if constexpr(JudyPolicy::has_values) {
                                word_t* immed_values = memory::alloc_words(jpm, alloc, pop1 - 1);
                                if(!immed_values) {
                                    jpm->error.type = ErrorType::NoMem;
                                    return CoreResult{ OpStatus::error, nullptr };
                                }
                                curr_jp->raw.w[0] = reinterpret_cast<word_t>(immed_values);
                                delete_copy<word_t>(JudyPolicy::immed_values(curr_jp, pop1 - 1),
                                                        jll_values, pop1, word_size, search.pos);
                            }

                            delete_copy<word8_t>(JudyPolicy::immed_keys(curr_jp, pop1), 
                                                    jll_keys, pop1, level, search.pos);
                        }

                        curr_jp->node.type = types::JPImmed_t<JudyPolicy>(level, pop1 - 1);
                        memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);
                        
                        goto WalkExit;
                    }

                    // JLL inplace/realloc
                    if(JudyPolicy::template jll_can_delete_inplace<CapacityPolicy>(level, pop1)) {
                        delete_inplace<word8_t>(JudyPolicy::jll_keys(jll, level, pop1), pop1, level, search.pos);
                        if constexpr(JudyPolicy::has_values) {
                            delete_inplace<word_t>(JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1), pop1, word_size, search.pos);
                        }
                    } else {
                        void* new_arr = jll_delete_with_realloc<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level, search.pos);
                        if(!new_arr) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        curr_jp->node.addr = reinterpret_cast<word_t>(new_arr);
                        
                        memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, jll, pop1, level);    
                    }
                    
                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                    
                }
                // RLL
                case types::RLL: {

                    level = root_level;
                    pop1 = get_pop0(curr_jp, level) + 1;

                    void* rll = get_ptr<void>(curr_jp->node.addr);
                    word8_t* rll_keys = JudyPolicy::jll_keys(rll, level, pop1);
                    word_t* rll_values = JudyPolicy::template jll_values<CapacityPolicy>(rll, level, pop1);

                    SearchResult search = leaf_binary_search<word8_t>(rll_keys, pop1, level, index);
                    if(!search.found) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    if(pop1 == 1) {
                        memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, rll, pop1, level);
                        set_null_jp(&jpm->top_jp);
                        return CoreResult{ OpStatus::erased, nullptr };
                    }

                    if(JudyPolicy::template jll_can_delete_inplace<CapacityPolicy>(level, pop1)) {
                        delete_inplace<word8_t>(rll_keys, pop1, level, search.pos);

                        if constexpr(JudyPolicy::has_values) {
                            delete_inplace<word_t>(rll_values, pop1, word_size, search.pos);
                        } 
                    } else {
                        void* new_rll = jll_delete_with_realloc<JudyPolicy, CapacityPolicy>(jpm, alloc, rll, pop1, level, search.pos);
                        if(!new_rll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }
                        memory::free_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, rll, pop1, level);
                        curr_jp->node.addr = reinterpret_cast<word_t>(new_rll);
                    }

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                case types::JLB_Base: {
                    word_t parent_level = level;
                    level = 1;
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;
                    bitmask = bit_pos_mask(digit);

                    if(!(JudyPolicy::jlb_bitmap(jlb, subexpanse) & bitmask)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    pop1 = get_pop0(curr_jp, level) + 1;
                                        
                    // make zero right now, to avoid extra copying
                    JudyPolicy::jlb_clear_bit(jlb, subexpanse, ~bitmask);
                    
                    // can be optimized, but for now ok
                    if constexpr(JudyPolicy::has_values) {
                        word_t old_bitmap = JudyPolicy::jlb_bitmap(jlb, subexpanse) | bitmask;
                        word_t* old_values = JudyPolicy::jlb_values(jlb, subexpanse);
                        std::size_t old_subexpanse_pop1 = std::popcount(old_bitmap);
                        std::size_t offset = std::popcount(old_bitmap & (bitmask - 1));
                        
                        if(old_subexpanse_pop1 == 1) {
                            memory::free_array<word_t, CapacityPolicy>(jpm, alloc, old_values, old_subexpanse_pop1);
                            JudyPolicy::jlb_set_values(jlb, subexpanse, nullptr);
                        } else if(CapacityPolicy::can_delete_inplace(old_subexpanse_pop1, word_size)) { 
                            delete_inplace<word_t>(old_values, old_subexpanse_pop1, word_size, offset);
                        } else {
                            word_t* new_values = delete_with_realloc<word_t, CapacityPolicy>(jpm, alloc, old_values, old_subexpanse_pop1, word_size, offset);
                            if(!new_values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }

                            memory::free_array<word_t, CapacityPolicy>(jpm, alloc, old_values, old_subexpanse_pop1);
                            JudyPolicy::jlb_set_values(jlb, subexpanse, new_values);
                        }

                    } else {
                        if(JudyPolicy::jlb_bitmap(jlb, subexpanse) == 0) {
                            memory::free_jlb_subexpanse<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb, subexpanse);
                            JudyPolicy::jlb_set_values(jlb, subexpanse, nullptr);
                        }
                    }
                    
                    // jlb->jll_up_level
                    if((parent_level - 1) > level && (pop1 == memory::leaf_max_pop1<JudyPolicy>(level + 1))) {
                        void* new_jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, pop1 - 1, level + 1);
                        if(!new_jll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        }

                        std::size_t pos = 0;
                        // jlb free inside
                        copy_immedleaf_to_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, new_jll, pos, get_byte(index, level), curr_jp, level + 1, pop1 - 1);

                        set_jp(curr_jp, reinterpret_cast<word_t>(new_jll), get_decode(curr_jp, level + 1),
                            pop1 - 2, types::JLL_Level(level + 1), level + 1);

                        goto WalkExit;
                    } 

                    // jlb -> IMMED (when happen?)
                    if((pop1 - 1) == JudyPolicy::immed_max_cnt(level)) {
                        word8_t* immed_keys = JudyPolicy::immed_keys(curr_jp, pop1 - 1);

                        if constexpr(JudyPolicy::has_values) {
                            if(pop1 > 2) {
                                word_t* values = memory::alloc_words(jpm, alloc, pop1 - 1);
                                if(!values) {
                                    jpm->error.type = ErrorType::NoMem;
                                    return CoreResult{ OpStatus::error, nullptr };
                                }
                                curr_jp->raw.w[0] = reinterpret_cast<word_t>(values);
                            }
                        }
                        word_t* immed_values = JudyPolicy::immed_values(curr_jp, pop1 - 1);

                        std::size_t pos = 0;

                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            word_t bitmap = JudyPolicy::jlb_bitmap(jlb, subexpanse);
                            word_t* jlb_values = JudyPolicy::jlb_values(jlb, subexpanse);
                            std::size_t offset = 0;

                            while(bitmap) {
                                const std::size_t first_bit = std::countr_zero(bitmap);
                                const word8_t digit = static_cast<word_t>(subexpanse * word_bits_size + first_bit);

                                immed_keys[pos] = digit;
                                if constexpr(JudyPolicy::has_values) {
                                    immed_values[pos] = jlb_values[offset++];
                                }
                                ++pos;

                                bitmap &= (bitmap - 1);
                            }
                        }
                        memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);

                        curr_jp->node.type = types::JPImmed_t<JudyPolicy>(level, pop1 - 1);

                        goto WalkExit;
                    }

                    // jlb -> jll
                    if((pop1 - 1) <= memory::leaf_bitmap_to_linear_threshold<JudyPolicy>()) {
                        void* jll = memory::alloc_jll<JudyPolicy, CapacityPolicy>(jpm, alloc, pop1 - 1, level);
                        if(!jll) {
                            jpm->error.type = ErrorType::NoMem;
                            return CoreResult{ OpStatus::error, nullptr };
                        } 
                        word8_t* jll_keys = JudyPolicy::jll_keys(jll, level, pop1 - 1);
                        word_t* jll_values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1 - 1);
                        
                        std::size_t pos = 0;
                        std::size_t value_pos = 0;
                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            word_t bitmap   = JudyPolicy::jlb_bitmap(jlb, subexpanse);
                            word_t* values  = JudyPolicy::jlb_values(jlb, subexpanse);

                            std::size_t offset = 0;
                            while(bitmap) {
                                std::size_t first_bit = std::countr_zero(bitmap);
                                word8_t digit = subexpanse * word_bits_size + first_bit;
                                jll_keys[pos++] = digit;
                                if constexpr(JudyPolicy::has_values) {
                                    jll_values[value_pos++] = values[offset++];
                                }

                                bitmap &= (bitmap - 1);
                            }
                        }

                        memory::free_jlb<JudyPolicy, CapacityPolicy>(jpm, alloc, jlb);
                        set_jp(curr_jp, reinterpret_cast<word_t>(jll), get_decode(curr_jp, level), pop1 - 2,
                            types::JLL_Level(level), level);

                        goto WalkExit;
                    }

                    // no changes

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                // Judy1 only
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    // jlb_full -> jlb
                    
                    JLB* jlb = memory::alloc_object<JLB>(jpm, alloc);
                    if(!jlb) {
                        jpm->error.type = ErrorType::NoMem;
                        return CoreResult{ OpStatus::error, nullptr };
                    }

                    for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        JudyPolicy::jlb_set_bitmap(jlb, subexpanse, all_ones_mask);
                    }
                    
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;
                    
                    JudyPolicy::jlb_clear_bit(jlb, subexpanse, ~bit_pos_mask(digit));
                    
                    set_jp(curr_jp, reinterpret_cast<word_t>(jlb), get_decode(curr_jp, level), EXPANSE_COUNT - 2,
                    types::JLB_Base, level);
                    
                    goto WalkExit;
                }
                case types::JPIMMED_Base...types::JP_max_type<JudyPolicy>: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type<JudyPolicy> && 
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level<JudyPolicy>(k + 1))) {
                        ++k;
                    }
                    word_t cnt = curr_jp->node.type - types::JPImmed_t<JudyPolicy>(k, 1) + 1;

                    if(cnt == 1) /* should never happened here */{
                        word_t mask = first_n_bytes_mask(k);
                        if((curr_jp->raw.w[1] & mask) != (index & mask)) {
                            return CoreResult{ OpStatus::not_found, nullptr };
                        }
                        set_null_jp(curr_jp);
                        
                        goto WalkExit;
                    }

                    word8_t* keys = JudyPolicy::immed_keys(curr_jp, cnt);
                    word_t* values = JudyPolicy::immed_values(curr_jp, cnt);
                    SearchResult search = leaf_linear_search<word8_t>(keys, cnt, k, index);

                    if(!search.found) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    if(cnt == 2) {
                        word_t first_key = 0, immed_key = 0, value = 0;
                        std::memcpy(&first_key, keys, k);
                        offset = (first_key == (index & first_n_bytes_mask(k)));
                        std::memcpy(&immed_key, keys + (offset ? k : 0), k);

                        if constexpr(JudyPolicy::has_values) {
                            value = values[(search.pos ? 0 : 1)];
                            memory::free_words(jpm, alloc, values, cnt);
                        }
                        set_immed1<JudyPolicy>(curr_jp, immed_key, k, value);
                    } else {
                        // delete from JPIMMED
                        if constexpr(JudyPolicy::has_values) {
                            word_t* new_values = memory::alloc_words(jpm, alloc, cnt - 1);
                            if(!new_values) {
                                jpm->error.type = ErrorType::NoMem;
                                return CoreResult{ OpStatus::error, nullptr };
                            }
                            delete_copy<word_t>(new_values, values, cnt, word_size, search.pos);
                            memory::free_words(jpm, alloc, values, cnt);
                            curr_jp->raw.w[0] = reinterpret_cast<word_t>(new_values);
                        }
                        delete_inplace<word8_t>(keys, cnt, k, search.pos);
                        curr_jp->node.type = types::JPImmed_t<JudyPolicy>(k, cnt - 1);
                    }
                    goto WalkExit;
                }
                default: {
                    jpm->error.type = ErrorType::Corrupt;
                    return CoreResult{ OpStatus::error, nullptr };
                }
            }
        }
        WalkExit:

        // unwinding the stack and -1 to pop0
        while(stack_pos-- > 0) {
            curr_jp = stack_jps[stack_pos];
            level = stack_levels[stack_pos];

            word_t new_pop0 = get_pop0(curr_jp, level) - 1;
            set_pop0(curr_jp, new_pop0, level);
        }

        --jpm->pop0;

        return CoreResult{ OpStatus::erased, nullptr };
    }

    template<typename JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    typename policy::JudyReturn<JudyPolicy>::erase_type judy_erase(JPM* jpm, Allocator& alloc, const word_t index) {
        CoreResult result = judy_erase_core<JudyPolicy, CapacityPolicy>(jpm, alloc, index);
        return policy::JudyReturn<JudyPolicy>::erase(result);    
    }

    // 0 - no index
    // 1 - has index
    // -1 - error (check jpm)
    template<typename JudyPolicy, typename CapacityPolicy>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    CoreResult judy_get_core(JPM* jpm, word_t index) {
        word_t level, digit, offset, subexpanse, pop1, bitmap, bitmask, cnt;
        using namespace detail;
        using JLB = typename JudyPolicy::JLB;

        if(!jpm) {
            return CoreResult{ OpStatus::error, nullptr };
        }
        
        JP* curr_jp = &jpm->top_jp;

        while(true) {
            switch(curr_jp->node.type) {
                case types::Null_JP: {
                    return CoreResult{ OpStatus::not_found, nullptr };
                }
                /* Loose cross-platform bcs gcc only feature. need to rewrite on if-else if */
                // JBL
                case types::JBL_Level(2)...types::JBL_Level(root_level): {

                    level = curr_jp->node.type - types::JBL_Base + 2;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    digit = get_byte(index, level - 1);     
                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    offset = 0;
                    while(offset < pop1 && jbl->keys[offset] < digit) {
                        ++offset;
                    }

                    if(offset == pop1 || jbl->keys[offset] != digit) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    curr_jp = jbl->pointers + offset;
                    continue;
                }
                // JBB
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);

                    bitmap          = jbb->subexpanses[subexpanse].bitmap;
                    JP* pointers    = jbb->subexpanses[subexpanse].pointers;
                    
                    bitmask = bit_pos_mask(digit);
                    if(!(bitmap & bitmask)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    offset = std::popcount(bitmap & (bitmask - 1));
                    curr_jp = pointers + offset;
                    continue;
                }
                // JBU
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    digit = get_byte(index, level - 1);

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);
                    curr_jp = jbu->pointers + digit;
                    continue;
                }
                // JLL/RLL
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1):
                case types::RLL: {
                    // In that case level - size of keys in leaf
                    level = curr_jp->node.type - types::JLL_Base + 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    // Leaf-pop no more than 1 byte
                    pop1 = get_pop0(curr_jp, level) + 1;
                    void* jll = get_ptr<void>(curr_jp->node.addr);

                    SearchResult search = leaf_binary_search<word8_t>(JudyPolicy::jll_keys(jll, level, pop1), pop1, level, index);
                    word_t* value = nullptr;
                    if constexpr(JudyPolicy::has_values) { 
                        word_t* values = JudyPolicy::template jll_values<CapacityPolicy>(jll, level, pop1);
                        value = (search.found ? (values + search.pos) : nullptr);
                    }

                    return CoreResult{ (search.found ? OpStatus::found : OpStatus::not_found), value };
                }
                // JLB
                case types::JLB_Base: {
                    level = 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }

                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);

                    bitmap = JudyPolicy::jlb_bitmap(jlb, subexpanse);
                    bitmask = bit_pos_mask(digit);
                    offset = std::popcount(bitmap & (bitmask - 1));
                    
                    bool found = (bitmap & bitmask) != 0;
                    word_t* value = nullptr;
                    if constexpr(JudyPolicy::has_values) {
                        word_t* values = JudyPolicy::jlb_values(jlb, subexpanse);
                        value = (found ? (values + offset) : nullptr);
                    }

                    return CoreResult{ (found ? OpStatus::found : OpStatus::not_found), value };
                }
                // FULLPOP (only Judy1)
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return CoreResult{ OpStatus::not_found, nullptr };
                    }
                    return CoreResult{ OpStatus::found, nullptr };
                }
                //JPIMMED
                case types::JPIMMED_Base...types::JP_max_type<JudyPolicy>: {

                    std::size_t k = 1;
                    while(k < types::JP_max_type<JudyPolicy> && /* to avoid memory dumps */
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level<JudyPolicy>(k + 1))) {
                        ++k;
                    }
                    cnt = curr_jp->node.type - types::JPImmed_t<JudyPolicy>(k, 1) + 1;

                    if(cnt == 1) {
                        // if smth changes - put it inside JudyPolicy
                        bool found = immed1_matches(curr_jp, k, index);
                        word_t* value = nullptr;
                        if constexpr(JudyPolicy::has_values) {
                            value = (found ? JudyPolicy::immed_values(curr_jp, cnt) : nullptr);
                        }
                        return CoreResult{ (found ? OpStatus::found : OpStatus::not_found), value };
                    }
                    
                    word8_t* keys = JudyPolicy::immed_keys(curr_jp, cnt);
                    SearchResult search = leaf_linear_search<word8_t>(keys, cnt, k, index);
                    
                    word_t* value = nullptr;
                    if constexpr(JudyPolicy::has_values) {
                        value = (search.found ? JudyPolicy::immed_values(curr_jp, cnt) + search.pos : nullptr);
                    }

                    return CoreResult{ (search.found ? OpStatus::found : OpStatus::not_found), value };
                }
                default: {
                    jpm->error.type = ErrorType::Corrupt;
                    return CoreResult{ OpStatus::error, nullptr };
                }
            }
        }
    }

    template<typename JudyPolicy, typename CapacityPolicy>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    typename policy::JudyReturn<JudyPolicy>::get_type judy_get(JPM* jpm, const word_t index) {
        CoreResult result = judy_get_core<JudyPolicy, CapacityPolicy>(jpm, index);
        return policy::JudyReturn<JudyPolicy>::get(result);
    }

    // return popultaion [start_index, end_index]
    // or -1 (if has error - check jpm)
    word_t judy_count(JPM* jpm, const word_t start_index, const word_t end_index) {
        return static_cast<word_t>(0);
    }

    // 1 - ans contain first 1 at [index, -1]
    // 0 - don't have 1 at [index, -1]
    // -1 - error (check jpm)
    int judy_first(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_next(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_last(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_prev(JPM* jpm, const word_t index) {
        return 0;
    }
    
    int judy_first_empty(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_next_empty(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_last_empty(JPM* jpm, const word_t index) {
        return 0;
    }

    int judy_prev_empty(JPM* jpm, const word_t index) {
        return 0;
    }

    template<policy::JudyArrayPolicy JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    void judy_clear_array(JPM* jpm, Allocator& alloc) {
        if(!jpm) {
            return;
        }
        detail::free_jp<JudyPolicy, CapacityPolicy, Allocator>(jpm, alloc, &jpm->top_jp);
        jpm->top_jp.raw.w[0] = jpm->top_jp.raw.w[1] = 0;
        jpm->pop0 = 0;
    }

    word_t by_count(JPM* jpm, std::size_t pos) {
        return 0;
    }

    template<policy::JudyArrayPolicy JudyPolicy, typename CapacityPolicy, typename Allocator>
    requires policy::JudyArrayPolicy<JudyPolicy> &&
    JudyCapacityPolicy<CapacityPolicy>
    void judy_free_array(JPM* jpm, Allocator& alloc) {
        if(!jpm) {
            return;
        }

        judy_clear_array<JudyPolicy, CapacityPolicy, Allocator>(jpm, alloc);
        memory::free_jpm(jpm, alloc);
    }
    
    // deep copy of judy array, return pointer to new judy array
    JPM* judy_copy_array() {
        return nullptr;
    }

    template<typename CapacityPolicy, typename Allocator>
    requires JudyCapacityPolicy<CapacityPolicy>
    class Judy1 {
    public:

        using JudyPolicy = policy::Judy1Policy;

        Judy1()  {
            jpm_ = judy::judy_create(alloc_);
            if(!jpm_) {
                ;// throw? or outside should check
            }
        }

        int get(const judy::word_t index) const {
            return judy::judy_get<JudyPolicy, CapacityPolicy>(jpm_, index);
        }

        int insert(const judy::word_t index) {
            return judy::judy_insert<JudyPolicy, CapacityPolicy>(jpm_, alloc_, index);
        }

        int erase(const judy::word_t index) {
            return judy::judy_erase<JudyPolicy, CapacityPolicy>(jpm_, alloc_, index);
        }

        judy::word_t count(const judy::word_t left, const judy::word_t right) const {
            return judy::judy_count(jpm_, left, right);
        }

        judy::word_t next(const judy::word_t index) const {
            return judy::judy_next(jpm_, index);
        }

        judy::word_t prev(const judy::word_t index) const {
            return judy::judy_prev(jpm_, index);
        }

        judy::word_t first(const judy::word_t index) const  {
            return judy::judy_first(jpm_, index);
        }

        judy::word_t last(const judy::word_t index) const {
            return judy::judy_last(jpm_, index);
        }

        judy::word_t next_empty(const judy::word_t index) const {
            return judy::judy_next_empty(jpm_, index);
        }

        judy::word_t prev_empty(const judy::word_t index) const {
            return judy::judy_prev_empty(jpm_, index);
        }

        judy::word_t first_empty(const judy::word_t index) const {
            return judy::judy_first_empty(jpm_, index);
        }

        judy::word_t last_empty(const judy::word_t index) const {
            return judy::judy_last_empty(jpm_, index);
        }

        judy::word_t by_count(std::size_t pos) const {
            return judy::by_count(jpm_, pos);
        }

        void clear() {
            judy::judy_clear_array<JudyPolicy, CapacityPolicy>(jpm_, alloc_);
        }

        ~Judy1() {
            if(jpm_) {
                judy::judy_free_array<JudyPolicy, CapacityPolicy>(jpm_, alloc_);
            }
        }

        bool is_ok()    const {
            return (jpm_->error.type == ErrorType::NotError);
        }

    private:

        Allocator alloc_;
        JPM* jpm_;

    };

    template<typename CapacityPolicy, typename Allocator>
    requires JudyCapacityPolicy<CapacityPolicy>
    class JudyL {
    public:

        using get_type = word_t*;
        using insert_type = word_t*;
        using erase_type = int;     

        using JudyPolicy = policy::JudyLPolicy;

        JudyL()  {
            jpm_ = judy::judy_create(alloc_);
            if(!jpm_) {
                ;// throw? or outside should check
            }
        }

        get_type get(const judy::word_t index) const {
            return judy::judy_get<JudyPolicy, CapacityPolicy>(jpm_, index);
        }

        insert_type insert(const judy::word_t index) {
            return judy::judy_insert<JudyPolicy, CapacityPolicy>(jpm_, alloc_, index);
        }

        erase_type erase(const judy::word_t index) {
            return judy::judy_erase<JudyPolicy, CapacityPolicy>(jpm_, alloc_, index);
        }

        void clear() {
            judy::judy_clear_array<JudyPolicy, CapacityPolicy>(jpm_, alloc_);
        }

        ~JudyL() {
            if(jpm_) {
                judy::judy_free_array<JudyPolicy, CapacityPolicy>(jpm_, alloc_);
            }
        }

        bool is_ok()    const {
            return (jpm_->error.type == ErrorType::NotError);
        }

    private:

        Allocator alloc_{};
        JPM* jpm_;

    };


};
