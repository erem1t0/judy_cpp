// judy_cpp C++20 v0.4.0

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <climits> // for CHAR_BIT
#include <cstring> // for std::memcpy
#include <bit> // for std::popcount
#include <stdexcept> 
#include <cassert>
#include <algorithm> // for std::max

/*
    TBD: pmr allocators + std::vector?

    TBD: alignment

    TBD: consteval
         constinit
         consexpr
         inline?

    TBD: likely unlikely - check if need flags on different OS

    TBD: do cleanup in case corrupt branch_to_jll    
*/


// Need to move in config.h.in ?
constexpr std::size_t EXPANSE_COUNT = 256;
constexpr std::size_t CACHE_LINE_SIZE_WORDS = 8; 
constexpr std::size_t BRANCH_LINEAR_MAX = 6;  // set to 0, if you want to turn it off      
constexpr std::size_t BRANCH_BITMAP_MAX = 185;
constexpr std::size_t LEAF_LINEAR_MAX = 128;  // max number of elements in linear leaf 
constexpr std::size_t LEAF1_LINEAR_MAX = 32; // max number of elements in linear leaf for level 1
constexpr std::size_t LEAF_LINEAR_CL_MAX = 2; // max number of cache-lines for linear leaf
constexpr std::size_t UNDER_BRANCH_LINEAR_MAX = 1000;
constexpr std::size_t UNDER_BRANCH_BITMAP_MAX = 135;
constexpr std::size_t MIN_ARRAY_SIZE_TO_UNCOMPRESSED = 750;

// dont like it
constexpr std::size_t GLOBAL_HYSTERESYS = 300;

// let use this now
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
 
/*
    Let says that leaf size for judy1 is 2 cache-lines (or 128 elements).
    (i think 128 is useless, should remove)
    */

// static_assert(BRANCH_LINEAR_MAX <= EXPANSE_COUNT && BRANCH_BITMAP_MAX <= EXPANSE_COUNT);

namespace judy {

    // need to take it outside?
    using word_t = std::uintptr_t;
    using word8_t  = std::uint8_t;

    constexpr std::size_t word_size = sizeof(word_t);
    constexpr std::size_t word_bits_size = CHAR_BIT * word_size;

    constexpr std::size_t root_level = word_size;

    constexpr std::size_t SUBEXPANSE_COUNT = (EXPANSE_COUNT  + word_bits_size - 1) / word_bits_size;


    // TBD
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
        
        struct JPImmed {
            word8_t keys[2 * word_size - 1];
            word8_t type;
        } immed;
        
        struct Raw {
            word_t w[2];
        } raw;
        
    };

    // not sure
    //constexpr JP null_jp = { .node = {.addr = 0, .dcd_pop = {0}, .type=0x01}};
    constexpr JP null_jp{};

    //static_assert(sizeof(JP) == 2 * word_size, "Judy Pointer must be 2 words");

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

    //static_assert(sizeof(JBU) / word_size == 2 * EXPANSE_COUNT);

    struct JLB {
        word_t subexpanses[SUBEXPANSE_COUNT];
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
        /* level = idx_size; */

        constexpr word8_t JLB_1 = JLL_Base + root_level;

        constexpr word8_t JLB_FULL = JLB_1 + 1;

        // should do FULL - 1, -2, -3 ...?

        constexpr word8_t JPIMMED_Base = JLB_FULL + 1;
        
        // max number of keys with len k to store in JPIMMED
        constexpr word8_t immed_max_cnt(std::size_t k) {
            return static_cast<word8_t>((2 * word_size - 1) / k);
        }

        // calc type pos for immed keys with len k
        constexpr word8_t immed_level(std::size_t k) {
            word8_t offset = 0;
            for(std::size_t i = 1; i < k; ++i) {
                offset += immed_max_cnt(i);
            }
            return offset;
        }

        // get type immed
        constexpr word8_t JPImmed_t(std::size_t k, std::size_t cnt) {
            return JPIMMED_Base + immed_level(k) + (cnt - 1);
        }

        constexpr word8_t JP_max_type = JPImmed_t(word_size - 1, 2) + 1;
    } // namespace types

    namespace memory {

        // remove extra?
        template<typename T>
        inline T* allocate(std::size_t count = 1, std::size_t extra_bytes = 0) {
            return static_cast<T*>(std::malloc(count * sizeof(T) + extra_bytes));
        }

        template<typename T>
        inline T* allocate_zeros(std::size_t count = 1, std::size_t extra_bytes = 0) {
            return static_cast<T*>(std::calloc(1, sizeof(T) * count + extra_bytes));
        }

        // ok or no?
        template<typename T>
        inline void deallocate(T* ptr) {
            std::free(ptr);
        }

        // max number of elements in linear_leaf level?
        inline constexpr word_t leaf_max_pop1(std::size_t level) {
            constexpr word_t leaf_max_words = LEAF_LINEAR_CL_MAX * CACHE_LINE_SIZE_WORDS;
            const word_t max_leaf_keys = (leaf_max_words * word_size) / level;
            return ((max_leaf_keys < LEAF_LINEAR_MAX) ? max_leaf_keys : LEAF_LINEAR_MAX);
        }

        inline constexpr word_t branch_to_leaf_threshold(std::size_t level) {
            const word_t leaf = leaf_max_pop1(level);
            return ((HYSTERESYS_BRANCH_TO_LEAF >= leaf) ? 0 : (leaf - HYSTERESYS_BRANCH_TO_LEAF));
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

        inline constexpr word_t leaf_bitmap_to_linear_threshold() {
            return ((HYSTERESYS_LEAF_BITMAP_TO_LINEAR >= LEAF1_LINEAR_MAX) ? 0 
                    : (LEAF1_LINEAR_MAX - HYSTERESYS_LEAF_BITMAP_TO_LINEAR));
        }

        // limits for memory
        constexpr std::size_t LIMIT_1 = CACHE_LINE_SIZE_WORDS * 2;
        constexpr std::size_t LIMIT_2 = word_bits_size * sizeof(JP) / word_size;
        constexpr std::size_t LIMITS[] = {LIMIT_1, LIMIT_2};

        constexpr std::size_t LIMIT_DIFF_WORDS = 4; 

        constexpr std::size_t LIMIT_MAX = std::max({LIMIT_1, LIMIT_2});

        constexpr std::size_t STEP_1 = 4;
        constexpr std::size_t STEP_2 = 16;
        constexpr std::size_t STEP_3 = 32;

        constexpr std::size_t STEP_LIMIT_1 = 16;
        constexpr std::size_t STEP_LIMIT_2 = 64;

        constexpr std::size_t MAX_SIZES_COUNT = 32;

        template<std::size_t MaxSize>
        struct ArraySizes {
            std::size_t sizes[MaxSize] = {};
            std::size_t count = 0;
        };

        constexpr ArraySizes<MAX_SIZES_COUNT> generate_array_sizes() {

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
        }

        static constexpr ArraySizes<MAX_SIZES_COUNT> arr_sizes = generate_array_sizes();

        constexpr std::size_t get_capacity(std::size_t words) {
            std::size_t i = 0;
            while(i < arr_sizes.count && arr_sizes.sizes[i] < words) {
                ++i;
            }
            
            return arr_sizes.sizes[i];        
        }

        inline bool can_insert_inplace(std::size_t pop1, std::size_t element_size) {
            const std::size_t old_words = (pop1 * element_size + word_size - 1) / word_size;
            const std::size_t new_words = ((pop1 + 1) * element_size + word_size - 1) / word_size;
            return get_capacity(old_words) == get_capacity(new_words);
        }

        inline bool can_delete_inplace(std::size_t pop1, std::size_t element_size) {
            const std::size_t old_words = (pop1 * element_size + word_size - 1) / word_size;
            const std::size_t new_words = ((pop1 - 1) * element_size + word_size - 1) / word_size;
            return get_capacity(old_words) == get_capacity(new_words);
        }

    } // namespace memory


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
        inline word_t get_pop0(JP* jp, std::size_t level) { return jp->raw.w[1] & pop0_mask(level); }        
        
        inline void set_decode(JP* jp, word_t decode, std::size_t level) {
            jp->raw.w[1] = (jp->raw.w[1] & ~decode_mask(level)) | (decode & decode_mask(level)); }
        
        inline void set_pop0(JP* jp, word_t pop0, std::size_t level) {
            jp->raw.w[1] = (jp->raw.w[1] & ~pop0_mask(level)) | (pop0 & pop0_mask(level)); }
        
        inline void set_immed1(JP* jp, const word_t index, std::size_t level) {
            jp->raw.w[1] = (index & first_n_bytes_mask(level)) | 
            ((static_cast<word_t>(types::JPImmed_t(level, 1))) << (CHAR_BIT * (word_size - 1))); 
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

        inline word_t read_key(const word8_t* arr, std::size_t pos, std::size_t level) {
            word_t ans = 0;
            std::memcpy(&ans, arr + (pos * level), level);
            return ans;
        }

        inline bool immed1_matched(const JP* jp, const word_t index, std::size_t level) {
            return (jp->raw.w[1] & first_n_bytes_mask(level)) == (index & first_n_bytes_mask(level));
        }   

        // DEBUG FUNCTION (maybe  be useful for count?)
        word_t calc_pop1(JP* jp) {

            word_t ans = 0;

            switch(jp->node.type) {
                case types::Null_JP: return 0;
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    JBL* jbl = get_ptr<JBL>(jp->node.addr);
                    word_t pop1 = jbl->pop0 + 1;
                    for(std::size_t offset = 0; offset < pop1; ++offset) {
                        ans += calc_pop1(&jbl->pointers[offset]);
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
                            ans += calc_pop1(&pointers[offset]);
                        }
                    }
                    break;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    JBU* jbu = get_ptr<JBU>(jp->node.addr);

                    for(word_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                        ans += calc_pop1(&jbu->pointers[digit]);
                    }
                    break;
                }
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    std::size_t level = jp->node.type - types::JLL_Base + 1;

                    ans += get_pop0(jp, level) + 1;
                    break;
                }
                case types::JLB_1: {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    for(word_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        ans += std::popcount(jlb->subexpanses[subexpanse]);
                    }
                    
                    break;
                }
                case types::JLB_FULL: {
                    ans += EXPANSE_COUNT;
                    break;
                }
                case types::JPIMMED_Base...types::JP_max_type: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type && 
                        jp->node.type >= (types::JPIMMED_Base + types::immed_level(k + 1))) {
                        ++k;
                    }
                    word_t cnt = jp->node.type - types::JPImmed_t(k, 1) + 1;

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

        // need native/nonnative?
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

        // native/nonnative?
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
        
        template<typename T>
        T* insert_with_realloc(T* arr, word_t old_pop1, std::size_t element_size, 
                               const T* element, std::size_t pos) {
                
            std::size_t words_need = ((old_pop1 + 1) * element_size + word_size - 1) / word_size;
            std::size_t new_capacity = memory::get_capacity(words_need);

            word_t* new_arr = memory::allocate<word_t>(new_capacity);
            
            if(!new_arr) {
                return nullptr;
            }

            insert_copy<T>(reinterpret_cast<T*>(new_arr), arr, old_pop1, element_size, element, pos);

            return reinterpret_cast<T*>(new_arr);
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

        template<typename T>
        T* delete_with_realloc(T* arr, word_t old_pop1, std::size_t element_size, std::size_t pos) {
            std::size_t words_need = ((old_pop1 - 1) * element_size + word_size - 1) / word_size;
            std::size_t new_capacity = memory::get_capacity(words_need);

            word_t* new_arr = memory::allocate<word_t>(new_capacity);
            if(!new_arr) {
                return nullptr;
            }

            delete_copy<T>(reinterpret_cast<T*>(new_arr), arr, old_pop1, element_size, pos);

            return reinterpret_cast<T*>(new_arr);
        }

        inline bool allocate_jbb_pointers(JBB* jbb, word8_t* subexpanse_pops) {

            for(std::size_t i = 0; i < SUBEXPANSE_COUNT; ++i) {
                if(subexpanse_pops[i]) {
                    std::size_t need_words = (subexpanse_pops[i] * sizeof(JP) + word_size - 1) / word_size;
                    std::size_t need_capacity = memory::get_capacity(need_words);
                    word_t* new_arr = memory::allocate<word_t>(need_capacity);

                    jbb->subexpanses[i].pointers = reinterpret_cast<JP*>(new_arr);
                    
                    if(!jbb->subexpanses[i].pointers) {
                        for(word_t j = 0; j < i; ++j) {
                            if(jbb->subexpanses[j].pointers) {
                                memory::deallocate(jbb->subexpanses[j].pointers);
                            }
                        }

                        return false;
                    }
                }
            }

            return true;
        }
        
        JBB* build_jbb_from_linear(word8_t* digits, JP* pointers, std::size_t pop1) {
            word_t offset, digit, subexpanse;
            
            // alloc JBB
            JBB* jbb = memory::allocate_zeros<JBB>();
            if(!jbb) {
                return nullptr;
            }
            

            // 1 pass: calc how many keys in subexpanses
            word8_t subexpanse_pops[SUBEXPANSE_COUNT] = {0};
            
            for(offset = 0; offset < pop1; ++offset) {
                digit = digits[offset];
                subexpanse = digit / word_bits_size;
                ++subexpanse_pops[subexpanse];
            }

            bool res = allocate_jbb_pointers(jbb, subexpanse_pops);
            if(!res) {
                memory::deallocate(jbb);
                return nullptr;
            }
            
            // 2 pass: go and copy
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

        // TBD: turn off jbl?
        bool insert_branch_outlier(JP* jp, word_t index, std::size_t level) {
            
            word_t xor_diff = ((index ^ jp->raw.w[1]) & decode_mask(level)) >> (level * CHAR_BIT);
        
            std::size_t highest_nonzero_bit = (word_bits_size - 1) - std::countl_zero(xor_diff);
            std::size_t branch_level = level + (highest_nonzero_bit / CHAR_BIT) + 1;
            
            word8_t old_byte = get_byte(jp->raw.w[1], branch_level - 1);
            word8_t new_byte = get_byte(index, branch_level - 1);
            
            JBL* jbl = memory::allocate<JBL>();
            if(!jbl) {
                return false;
            }

            JP new_jp{};
            set_immed1(&new_jp, index, branch_level - 1);

            JP old_jp = *jp;
            
            if(old_byte < new_byte) {
                jbl->keys[0] = old_byte;
                jbl->keys[1] = new_byte;
                jbl->pointers[0] = old_jp;
                jbl->pointers[1] = new_jp;
            } else {
                jbl->keys[0] = new_byte;
                jbl->keys[1] = old_byte;
                jbl->pointers[0] = new_jp;
                jbl->pointers[1] = old_jp;
            }

            jbl->pop0 = 1;

            // common_prefix = index & decode_mask(branch_level);
            word_t new_pop0 = get_pop0(jp, level) + 1;
            set_jp(jp, reinterpret_cast<word_t>(jbl), index, new_pop0, types::JBL_Level(branch_level), branch_level);

            return true;
        }

        // JLL1 -> JLB1
        // JLL2 -> JLB1
        bool jll_to_jlb(JP* jp, word8_t* jll, word_t pop1, std::size_t level) {
            JLB* jlb = memory::allocate_zeros<JLB>();
            if(!jlb) {
                return false;
            }
            
            for(std::size_t offset = 0; offset < pop1; ++offset) {
                word_t digit = jll[offset * level]; 
                word_t subexpanse = digit / word_bits_size;

                jlb->subexpanses[subexpanse] |= bit_pos_mask(digit);
            }

            word_t new_decode = get_decode(jp, level) | (level == 1 ? 0 : (static_cast<word_t>(jll[1]) << CHAR_BIT));
            set_jp(jp, reinterpret_cast<word_t>(jlb), new_decode, pop1 - 1, types::JLB_1, 1);
            
            memory::deallocate(jll);
            
            return true;
        }

        inline void free_jll_cascade(JP* pointers, std::size_t count) {
            for(std::size_t i = 0; i < count; ++i) {
                JP* jp = &pointers[i];
                
                if(jp->node.type >= types::JLL_Level(1) && jp->node.type <= types::JLL_Level(root_level - 1)) {
                    word8_t* jll = get_ptr<word8_t>(jp->node.addr);
                    memory::deallocate(jll);
                } else if(jp->node.type == types::JLB_1) {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    memory::deallocate(jlb);
                }
            }
        }

        /*
            JLL1 -> JLB1
            JLLX -> JLL(X-1) + narrow
            JLLX -> JLB/JBB(X) + JLL(X-1)/JLB1
        */
        bool cascade(JP* jp, std::size_t level, word_t index_to_insert) {
      
            word8_t* jll = get_ptr<word8_t>(jp->node.addr);
            word_t pop1 = get_pop0(jp, 1) + 1;
            word_t old_decode = get_decode(jp, level);

            // JLL1 -> JLB1
            if(level == 1) {
                return jll_to_jlb(jp, jll, pop1, level);
            }   

            word_t first_key = read_key(jll, 0, level);
            word_t last_key = read_key(jll, pop1 - 1, level);
            const std::size_t child_level = level - 1;

            if(get_byte(first_key, level - 1) == get_byte(last_key, level - 1)) {

                // JLL2 -> JLB1
                if(child_level == 1) {
                    return jll_to_jlb(jp, jll, pop1, level);
                }

                std::size_t need_words = (pop1 * child_level + word_size - 1) / word_size;
                
                word_t* new_arr = memory::allocate<word_t>(memory::get_capacity(need_words));
                if(!new_arr) {
                    return false;
                }
                word8_t* new_jll = reinterpret_cast<word8_t*>(new_arr);

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(new_jll + (offset * child_level), jll + (offset * level), child_level);
                }

                memory::deallocate(jll);

                word_t new_decode = old_decode | (get_byte(first_key, level - 1) << (CHAR_BIT * child_level));
                set_jp(jp, reinterpret_cast<word_t>(new_jll), new_decode, pop1 - 1, types::JLL_Level(child_level), child_level);
                
                return true;
            }

            word_t index_byte = get_byte(index_to_insert, child_level);
            bool index_in_branch = false;

            // make zero?
            JP pointers[LEAF_LINEAR_MAX]; 
            word8_t digits[LEAF_LINEAR_MAX];

            std::size_t start = 0, end = 0, subexpanse = 0;
            while(start < pop1) {
                
                word_t start_key = read_key(jll, start, level);
                word_t top_byte = get_byte(start_key, child_level);
                index_in_branch |= (top_byte == index_byte);
                
                end = start + 1;
                while(end < pop1 && get_byte(read_key(jll, end, level), child_level) == top_byte) {
                    ++end;
                }

                std::size_t subexpanse_pop1 = end - start;
                digits[subexpanse] = top_byte;
                JP* new_jp = &pointers[subexpanse];

                // Immed1
                if(subexpanse_pop1 == 1) {
                    new_jp->raw.w[1] = 0;
                    set_immed1(new_jp, start_key, child_level);                    
                } 
                // Immed2+
                else if(subexpanse_pop1 <= types::immed_max_cnt(child_level)) {
                    set_null_jp(new_jp);
                    for(std::size_t offset = 0; offset < subexpanse_pop1; ++offset) {
                        std::memcpy(new_jp->immed.keys + (offset * child_level), jll + ((start + offset) * level), child_level);
                    }
                    new_jp->immed.type = types::JPImmed_t(child_level, subexpanse_pop1);
                } 
                // JLB
                else if(child_level == 1 && subexpanse_pop1 > LEAF1_LINEAR_MAX) {

                    JLB* jlb = memory::allocate_zeros<JLB>();
                    if(!jlb) {
                        free_jll_cascade(pointers, subexpanse);
                        return false;
                    }

                    for(std::size_t offset = 0; offset < subexpanse_pop1; ++offset) {
                        word_t digit = jll[start + offset];
                        word_t digit_subexpanse = digit / word_bits_size; 
                        jlb->subexpanses[digit_subexpanse] |= bit_pos_mask(digit);
                    }

                    word_t new_decode = old_decode | (top_byte << (CHAR_BIT * child_level));
                    set_jp(new_jp, reinterpret_cast<word_t>(jlb), new_decode, subexpanse_pop1 - 1, types::JLB_1, child_level);
                }
                // JLL
                else {
                    std::size_t need_words = ((subexpanse_pop1 * child_level) + word_size - 1) / word_size;
                    word_t* new_arr = memory::allocate<word_t>(memory::get_capacity(need_words));
                    if(!new_arr) {
                        free_jll_cascade(pointers, subexpanse);
                        return false;
                    }
                    
                    word8_t* new_jll = reinterpret_cast<word8_t*>(new_arr);
                    for(std::size_t offset = 0; offset < subexpanse_pop1; ++offset) {
                        std::memcpy(new_jll + (offset * child_level), jll + ((start + offset) * level), child_level);    
                    }                    

                    word_t new_decode = old_decode | (top_byte << (CHAR_BIT * child_level));
                    set_jp(new_jp, reinterpret_cast<word_t>(new_jll), new_decode, subexpanse_pop1 - 1, types::JLL_Level(child_level), child_level);
                }
                ++subexpanse;
                start = end;
            }

            bool is_jbl = subexpanse < BRANCH_LINEAR_MAX || (subexpanse == BRANCH_LINEAR_MAX && index_in_branch);

            if(is_jbl) {
                JBL* jbl = memory::allocate<JBL>();
                if(!jbl) {
                    free_jll_cascade(pointers, subexpanse);
                    return false;
                }

                std::memcpy(jbl->keys, digits, subexpanse);
                std::memcpy(jbl->pointers, pointers, subexpanse * sizeof(JP));
                jbl->pop0 = subexpanse - 1;

                jp->node.addr = reinterpret_cast<word_t>(jbl);
                jp->node.type = types::JBL_Level(level);                
            } else {
                JBB* jbb = build_jbb_from_linear(digits, pointers, subexpanse);
                if(!jbb) {
                    free_jll_cascade(pointers, subexpanse);
                    return false;
                }

                jp->node.addr = reinterpret_cast<word_t>(jbb);
                jp->node.type = types::JBB_Level(level);
            }
            
            memory::deallocate(jll);

            return 1;
        }


        // -1 - corrupt
        // 1 - success
        inline void copy_immedleaf_to_jll(word8_t* jll, std::size_t& pos, word8_t top_byte,
                                         JP* jp, std::size_t level) {
            
            const std::size_t child_level = level - 1;
            const word8_t type = jp->immed.type;

            if(type == types::JLL_Level(child_level)) {
                word8_t* leaf = get_ptr<word8_t>(jp->node.addr);
                const word_t pop1 = get_pop0(jp, child_level) + 1;

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(jll + pos, leaf + offset * child_level, child_level);
                    jll[pos + child_level] = top_byte;
                    pos += level;
                }
                memory::deallocate(leaf);
            } 
            else if(type >= types::JPImmed_t(child_level, 1) &&
                    type <= types::JPImmed_t(child_level, types::immed_max_cnt(child_level))) {
                word8_t* keys = jp->immed.keys;
                word_t pop1 = type - types::JPImmed_t(child_level, 1) + 1;

                if(pop1 == 1) {
                    keys += word_size;
                } 

                for(std::size_t offset = 0; offset < pop1; ++offset) {
                    std::memcpy(jll + pos, keys + offset * child_level, child_level);
                    jll[pos + child_level] = top_byte;
                    pos += level;
                }
            
            }
            else if(type == types::JLB_1 && child_level == 1) {
                JLB* jlb = get_ptr<JLB>(jp->node.addr);

                for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                    word_t bitmap = jlb->subexpanses[subexpanse];

                    while(bitmap) {
                        const std::size_t first_bit = std::countr_zero(bitmap);
                        const word8_t digit = static_cast<word8_t>(subexpanse * word_bits_size + first_bit);

                        jll[pos++] = digit;
                        jll[pos++] = top_byte;

                        bitmap &= (bitmap - 1);
                    }
                }

                memory::deallocate(jlb);
            } 
            else [[unlikely]] /* should never happen */ { 
                assert(false);
            }
        }


        void jbl_copy_traverse(JP* jp, word8_t* jll, std::size_t level) {
            JBL* jbl = get_ptr<JBL>(jp->node.addr);
            std::size_t jll_pos = 0;
            word_t jbl_pop1 = jbl->pop0 + 1;
            for(std::size_t offset = 0; offset < jbl_pop1; ++offset) {
                
                word8_t top_byte = jbl->keys[offset];
                JP* curr_jp = &jbl->pointers[offset];
                
                copy_immedleaf_to_jll(jll, jll_pos, top_byte, curr_jp, level);
            }
        }

        void jbb_copy_traverse(JP* jp, word8_t* jll, std::size_t level) {
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
                    copy_immedleaf_to_jll(jll, jll_pos, digit, curr_jp, level);
                    
                    bitmap &= (bitmap - 1);
                }
                memory::deallocate(pointers);
            }
        }
         
        void jbu_copy_traverse(JP* jp, word8_t* jll, std::size_t level) {
            JBU* jbu = get_ptr<JBU>(jp->node.addr);
            std::size_t jll_pos = 0;

            for(std::size_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                
                JP* curr_jp = &jbu->pointers[digit];
                if(curr_jp->node.type == types::Null_JP) {
                    continue;
                }

                copy_immedleaf_to_jll(jll, jll_pos, digit, curr_jp, level);
            }
        }

        inline bool check_immedleaf_index(JP* jp, word_t index, std::size_t level) {

            const word8_t type = jp->immed.type;

            if(type == types::JLL_Level(level)) {
                word8_t* jll = get_ptr<word8_t>(jp->node.addr);
                const word_t pop1 = get_pop0(jp, level) + 1;

                return leaf_binary_search<word8_t>(jll, pop1, level, index).found;
            } 
            else if(type >= types::JPImmed_t(level, 1) &&
                    type <= types::JPImmed_t(level, types::immed_max_cnt(level))) {
                word8_t* keys = jp->immed.keys;
                word_t pop1 = type - types::JPImmed_t(level, 1) + 1;
                
                return (pop1 == 1 ? immed1_matched(jp, index, level) : leaf_linear_search<word8_t>(keys, pop1, level, index).found);
            }
            else if(type == types::JLB_1 && level == 1) {
                JLB* jlb = get_ptr<JLB>(jp->node.addr);
                word_t digit = get_byte(index, level - 1);
                word_t subexpanse = digit / word_bits_size;
                
                return jlb->subexpanses[subexpanse] & bit_pos_mask(digit);
            } 
            else [[unlikely]] /* should never happen */ { 
                assert(false);
            }
            
        }

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
                : check_immedleaf_index(&jbl->pointers[offset], index, level - 1));
        }

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
            return check_immedleaf_index(curr_jp, index, level - 1);
        }
         
        bool jbu_check_traverse(const JP* jp, word_t index, std::size_t level) {
            JBU* jbu = get_ptr<JBU>(jp->node.addr);
            word_t digit = get_byte(index, level - 1);
            JP* curr_jp = &jbu->pointers[digit];

            return (curr_jp->node.type == types::Null_JP 
                ? false 
                : check_immedleaf_index(curr_jp, index, level - 1));
        }
        
        // TBD: common traverse?
        bool index_under_branch(const JP* jp, const word_t index, std::size_t level) {
           if(jp->node.type == types::JBL_Level(level)) {
                return jbl_check_traverse(jp, index, level);
            } else if(jp->node.type == types::JBB_Level(level)) {
                return jbb_check_traverse(jp, index, level);
            } else if(jp->node.type == types::JBU_Level(level)) {
                return jbu_check_traverse(jp, index, level);
            } 
        }

        // JB* -> JLL at same level
        bool branch_to_jll(JP* jp, word_t node_pop1, std::size_t level) {
            word_t need_words = (node_pop1 * level + word_size - 1) / word_size;
            word_t need_capacity = memory::get_capacity(need_words);
            word_t* leaf = memory::allocate<word_t>(need_capacity);
            if(!leaf) {
                return false;
            }

            word8_t* jll = reinterpret_cast<word8_t*>(leaf);

            if(jp->node.type == types::JBL_Level(level)) {
                jbl_copy_traverse(jp, jll, level);
            } else if(jp->node.type == types::JBB_Level(level)) {
                jbb_copy_traverse(jp, jll, level);
            } else if(jp->node.type == types::JBU_Level(level)) {
                jbu_copy_traverse(jp, jll, level);
            } 
            
            memory::deallocate(get_ptr<void>(jp->node.addr));
            
            set_jp(jp, reinterpret_cast<word_t>(jll), get_decode(jp, level), 
                   node_pop1 - 1, types::JLL_Level(level), level);
            
            return true;
        }

        // convert jbb to jbl
        // 0 - no memory
        // 1 - success
        bool jbb_to_jbl(JP* jp, std::size_t level) {
            JBB* jbb = get_ptr<JBB>(jp->node.addr);

            JBL* jbl = memory::allocate<JBL>();
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
                
                memory::deallocate(pointers);
            }
            jbl->pop0 = pos - 1;

            memory::deallocate(jbb);

            jp->node.addr = reinterpret_cast<word_t>(jbl);
            jp->node.type = types::JBL_Level(level);

            return true;
        }

        // level useless? can get from jp
        bool jbu_to_jbb(JP* jp, std::size_t level) {

            word_t digit, subexpanse;

            JBB* jbb = memory::allocate_zeros<JBB>();
            if(!jbb) {
                return false;
            }

            JBU* jbu = get_ptr<JBU>(jp->node.addr);

            word8_t subexpanse_pops[SUBEXPANSE_COUNT] = {0};
            for(digit = 0; digit < EXPANSE_COUNT; ++digit) {
                if(jbu->pointers[digit].node.type == types::Null_JP) {
                    continue;
                }
                subexpanse = digit / word_bits_size;
                ++subexpanse_pops[subexpanse];
            }           

            bool res = allocate_jbb_pointers(jbb, subexpanse_pops);
            if(!res) {
                memory::deallocate(jbb);
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

            memory::deallocate(jbu);

            jp->node.addr = reinterpret_cast<word_t>(jbb);
            jp->node.type = types::JBB_Level(level);

            return true;
        }

        bool jbu_to_jbl(JP* jp, std::size_t level) {

            JBL* jbl = memory::allocate<JBL>();
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

            memory::deallocate(jbu);
        
            jp->node.addr = reinterpret_cast<word_t>(jbl);
            jp->node.type = types::JBL_Level(level);

            return true;
        }

        // only free, not making 0
        void free_jp(JP* jp, JPM* arr) {
            word_t pop1;

            switch(jp->node.type) {
                case types::Null_JP: [[unlikely]] {
                    break;
                }
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    JBL* jbl = get_ptr<JBL>(jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    for(std::size_t offset = 0; offset < pop1; ++offset) {
                        free_jp(&jbl->pointers[offset], arr);
                    }
                    memory::deallocate(jbl);
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
                            free_jp(&pointers[offset], arr);
                        }
                        memory::deallocate(jbb->subexpanses[subexpanse].pointers);
                    }

                    memory::deallocate(jbb);
                    break;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    
                    JBU* jbu = get_ptr<JBU>(jp->node.addr);
                    
                    for(std::size_t digit = 0; digit < EXPANSE_COUNT; ++digit) {
                        if(jbu->pointers[digit].node.type == types::Null_JP) {
                            continue;
                        }
                        free_jp(&jbu->pointers[digit], arr);
                    }

                    memory::deallocate(jbu);
                    break;
                }
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    std::size_t level = jp->node.type - types::JLL_Base + 1;
                    pop1 = get_pop0(jp, level) + 1;

                    word8_t* jll = get_ptr<word8_t>(jp->node.addr);

                    memory::deallocate(jll);
                    arr->pop0 -= pop1;
                    break;
                }
                case types::JLB_1: {
                    JLB* jlb = get_ptr<JLB>(jp->node.addr);
                    pop1 = 0;
                    for(std::size_t subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        pop1 += std::popcount(jlb->subexpanses[subexpanse]);
                    }
                    memory::deallocate(jlb);
                    arr->pop0 -= pop1;
                    break;
                }
                case types::JLB_FULL: {
                    arr->pop0 -= EXPANSE_COUNT;
                    break;
                }
                case types::JPIMMED_Base...types::JP_max_type: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type && 
                        jp->node.type >= (types::JPIMMED_Base + types::immed_level(k + 1))) {
                        ++k;
                    }
                    word_t cnt = jp->node.type - types::JPImmed_t(k, 1) + 1;

                    arr->pop0 -= cnt;
                    break;
                }
                default: [[unlikely]] {
                    assert(false);
                    break;
                }
            }
        }

    } // namespace detail

    namespace conversions {

        // jbu_to_jbl
        // jbu_to_jbb
        // jbb_to_jbl
        // jll_to_jlb?

    } // namespace conversions

    JPM* judy_create() {

        JPM* jpm = memory::allocate_zeros<JPM>();
        
        if (!jpm) 
            return nullptr;

        jpm->total_memory = sizeof(JPM);

        return jpm;
    }

    // 0 - has already
    // 1 - successfully
    // -1 - error (check jpm)
    int judy_set(JPM* const arr, const word_t index) {
        word_t level, digit, offset, subexpanse, pop1, node_pop1, bitmap, bitmask;
        using namespace detail;

        JP* stack_jps               [word_size + 1]; // need +1 or no?
        std::size_t stack_levels    [word_size + 1];

        std::size_t stack_pos = 0;

        if(!arr) [[unlikely]] {
            return -1;
        }

        JP* curr_jp = &arr->top_jp;
     
        if(curr_jp->node.type == types::Null_JP) [[unlikely]] {
            JBL* jbl = memory::allocate<JBL>();
            if(!jbl) {
                arr->error.type = ErrorType::NoMem;
                return -1;
            }
            
            arr->pop0 = 0;
            arr->top_jp.node.addr = reinterpret_cast<word_t>(jbl);
            arr->top_jp.node.type = types::JBL_Level(root_level);
            
            JP new_jp{};
            set_immed1(&new_jp, index, root_level - 1);

            jbl->keys[0] = get_byte(index, root_level - 1);
            jbl->pointers[0] = new_jp;

            jbl->pop0 = 0;
            return 1;
        }

        while(true) {

            switch(curr_jp->node.type) {
                // only JBU
                case types::Null_JP: {
                    set_immed1(curr_jp, index, level - 1);

                    goto WalkExit;
                }
                // JBL
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    level = curr_jp->node.type - types::JBL_Base + 2;
                    digit = get_byte(index, level - 1);   
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        goto WalkExit;
                    }


                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    node_pop1 = get_pop0(curr_jp, level) + 1; 

                    // JBL -> JBU
                    if(node_pop1 > UNDER_BRANCH_LINEAR_MAX) {
                        
                        JBU* jbu = memory::allocate_zeros<JBU>();
                        if(!jbu) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        for(offset = 0; offset < pop1; ++offset) {
                            digit = jbl->keys[offset];
                            JP* digit_jp = &jbl->pointers[offset];

                            jbu->pointers[digit] = *digit_jp;
                        }
                        
                        memory::deallocate(jbl);

                        curr_jp->node.addr = reinterpret_cast<word_t>(jbu);
                        curr_jp->node.type = types::JBU_Level(level);

                        arr->lastpop0 = arr->pop0; 

                        continue;
                    }

                    // is there smth more optimal tha struct?
                    SearchResult search = leaf_linear_search<word8_t>(jbl->keys, pop1, 1, digit);
                    
                    if(search.found) {
                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;
                        
                        curr_jp = jbl->pointers + search.pos;
                        continue;
                    }

                    if(pop1 < BRANCH_LINEAR_MAX) {

                        JP new_jp{};
                        set_immed1(&new_jp, index, level - 1);

                        insert_inplace(jbl->keys, jbl->pop0 + 1UL, 1UL, 
                            reinterpret_cast<word8_t*>(&digit), search.pos);

                        insert_inplace<JP>(jbl->pointers, jbl->pop0 + 1UL, sizeof(JP), &new_jp, search.pos);                            

                        ++jbl->pop0;

                        stack_jps[stack_pos] = curr_jp;
                        stack_levels[stack_pos++] = level;
                        
                        goto WalkExit;
                    } else {
                        // JBL -> JBB
                        
                        JBB* jbb = build_jbb_from_linear(jbl->keys, jbl->pointers, jbl->pop0 + 1);
                        if(!jbb) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        memory::deallocate(jbl);

                        curr_jp->node.addr = reinterpret_cast<word_t>(jbb);
                        curr_jp->node.type = types::JBB_Level(level);

                        continue;
                    }
                    
                }
                // JBB
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        goto WalkExit;
                    }
                    
                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);

                    if(((arr->pop0 - arr->lastpop0) >= GLOBAL_HYSTERESYS)        &&
                        (arr->pop0 >= MIN_ARRAY_SIZE_TO_UNCOMPRESSED)            &&
                        (get_pop0(curr_jp, level) >= UNDER_BRANCH_BITMAP_MAX)   )
                    {
                        JBU* jbu = memory::allocate_zeros<JBU>();
                        if(!jbu) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            digit = subexpanse * word_bits_size;
                            
                            bitmap = jbb->subexpanses[subexpanse].bitmap;
                            JP* jps = jbb->subexpanses[subexpanse].pointers;

                            if(!bitmap) {
                                continue;
                            }
                            
                            // what's faster: copy 1 by 1 or copy ranges?
                            // need to add check for full bitmap?
                            
                            for(word_t bit = 0, offset = 0; bit < word_bits_size; ++bit, bitmap >>= 1, ++digit) {
                                jbu->pointers[digit] = ((bitmap & 1) ? jps[offset++] : null_jp);
                            }
                            memory::deallocate(jps);
                        }
                        memory::deallocate(jbb);

                        arr->lastpop0 = arr->pop0;

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

                    JP new_jp{};
                    set_immed1(&new_jp, index, level - 1);

                    std::size_t need_words = ((pop1 + 1) * sizeof(JP) + word_size - 1) / word_size;

                    if(!bitmap) {
                        
                        word_t* new_arr = memory::allocate<word_t>(memory::get_capacity(need_words));
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        JP* new_pointers = reinterpret_cast<JP*>(new_arr);
                        new_pointers[0] = new_jp;

                        jbb->subexpanses[subexpanse].pointers = new_pointers;

                    } else if(memory::can_insert_inplace(pop1, sizeof(JP))) { 
                        insert_inplace<JP>(pointers, pop1, sizeof(JP), &new_jp, offset);
                    } else {
                        JP* new_pointers = insert_with_realloc<JP>(pointers, pop1, sizeof(JP), &new_jp, offset);
                        if(!new_pointers) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }
                        memory::deallocate(pointers);
                        jbb->subexpanses[subexpanse].pointers = new_pointers;
                    }

                    jbb->subexpanses[subexpanse].bitmap |= bitmask;

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                // JBU
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;
                    digit = get_byte(index, level - 1);

                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        goto WalkExit;
                    }

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    curr_jp = jbu->pointers + digit;

                    continue;
                }
                // JLL
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                    level = curr_jp->node.type - types::JLL_Base + 1;
                    pop1 = get_pop0(curr_jp, level) + 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        goto WalkExit;
                    }

                    word_t next_words = ((pop1 + 1) * level + word_size - 1) / word_size;

                    word8_t* jll = get_ptr<word8_t>(curr_jp->node.addr);
                    SearchResult search = leaf_binary_search<word8_t>(jll, pop1, level, index);

                    if(search.found) {
                        return 0;
                    }
                    
                    if(next_words > 2 * CACHE_LINE_SIZE_WORDS || pop1 == LEAF_LINEAR_MAX
                        || (level == 1 && pop1 == LEAF1_LINEAR_MAX)) {
                        bool res = cascade(curr_jp, level, index);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        continue;
                    }

                    if(memory::can_insert_inplace(pop1, level)) {
                        insert_inplace<word8_t>(jll, pop1, level, reinterpret_cast<const word8_t*>(&index), 
                                                    search.pos);
                    } else { 
                    
                        word8_t* new_jll = insert_with_realloc<word8_t>(jll, pop1, level, 
                                                    reinterpret_cast<const word8_t*>(&index), search.pos);
                        if(!new_jll) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        memory::deallocate(jll);
                        
                        curr_jp->node.addr = reinterpret_cast<word_t>(new_jll);
                    }

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                // JLB1
                case types::JLB_1: {
                    level = 1;
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }
                        
                        goto WalkExit;
                    }
                    
                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    if(jlb->subexpanses[subexpanse] & bit_pos_mask(digit)) {
                        return 0;
                    }

                    jlb->subexpanses[subexpanse] |= bit_pos_mask(digit);

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    
                    pop1 = get_pop0(curr_jp, level) + 1;
                    if(pop1 + 1 == EXPANSE_COUNT) {
                        memory::deallocate(jlb);
                        
                        set_pop0(curr_jp, EXPANSE_COUNT - 1, level);
                        curr_jp->node.type = types::JLB_FULL;
                        
                        --stack_pos; 
                    }

                    goto WalkExit;
                }
                // FULLPOP
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        bool res = insert_branch_outlier(curr_jp, index, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        goto WalkExit;
                    }

                    return 0;
                }
                //JPIMMED
                case types::JPIMMED_Base...types::JP_max_type: {

                    std::size_t k = 1;
                    while(k < types::JP_max_type && 
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level(k + 1))) {
                        ++k;
                    }
                    word_t cnt = curr_jp->node.type - types::JPImmed_t(k, 1) + 1;

                    if(cnt == 1) {
                        word_t old_index = read_key(curr_jp->node.dcd_pop0, 0, k);
                        word_t new_index = index & first_n_bytes_mask(k);

                        if(old_index == new_index) {
                            return 0;
                        }

                        if(old_index < new_index) {
                            std::memcpy(curr_jp->immed.keys, &old_index, k);
                            std::memcpy(curr_jp->immed.keys + k, &new_index, k);
                        } else {
                            std::memcpy(curr_jp->immed.keys, &new_index, k);
                            std::memcpy(curr_jp->immed.keys + k, &old_index, k);
                        }

                        curr_jp->immed.type = types::JPImmed_t(k, 2);

                        goto WalkExit;
                    }

                    SearchResult search = leaf_linear_search<word8_t>(curr_jp->immed.keys, cnt, k, 
                                                                    index);
                    if(search.found) {
                        return 0;
                    }
                        
                    // jpimmed -> JLL
                    if((cnt + 1) * k > 2 * word_size - 1) {
                        word_t need_words = (k * (cnt + 1) + word_size - 1) / word_size;
                        word_t need_capacity = memory::get_capacity(need_words);
                        word_t* new_arr = memory::allocate<word_t>(need_capacity);
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        word8_t* jll = reinterpret_cast<word8_t*>(new_arr);

                        insert_copy<word8_t>(jll, curr_jp->immed.keys, cnt, k, 
                                                reinterpret_cast<const word8_t*>(&index), search.pos);

                        set_jp(curr_jp, reinterpret_cast<word_t>(jll), index, cnt, types::JLL_Level(k), k);
                    } else {
                        insert_inplace<word8_t>(curr_jp->immed.keys, cnt, k, 
                                                reinterpret_cast<const word8_t*>(&index), search.pos);
                        curr_jp->immed.type = types::JPImmed_t(k, cnt + 1);
                    }

                    goto WalkExit;
                }
                default: [[unlikely]] {
                    arr->error.type = ErrorType::Corrupt;
                    return -1;
                }
            }
        }
        WalkExit:

        while(stack_pos-- > 0) {
            curr_jp = stack_jps[stack_pos];
            level = stack_levels[stack_pos];

            word_t new_pop0 = get_pop0(curr_jp, level) + 1;
            set_pop0(curr_jp, new_pop0, level);
        }

        ++arr->pop0;
        
        return 1;
    }

    // 0 - already deleted
    // 1 - succesfully
    // -1 - error (check jpm)
   int judy_unset(JPM* const arr, const word_t index) {
        word_t level, digit, offset, subexpanse, pop1, node_pop1, bitmap, bitmask;
        using namespace detail;

        JP* stack_jps               [word_size + 1];
        std::size_t stack_levels    [word_size + 1];

        std::size_t stack_pos = 0;

        if(!arr) [[unlikely]] {
            return -1;
        }

        JP* curr_jp = &arr->top_jp;

        while(true) {
            switch(curr_jp->node.type) {
                case types::Null_JP: {
                    return 0;
                }
                case types::JBL_Level(2)...types::JBL_Level(root_level): {
                    level = curr_jp->node.type - types::JBL_Base + 2;
                    digit = get_byte(index, level - 1);   
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    node_pop1 = get_pop0(curr_jp, level) + 1; 

                    // JBL -> JLL
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold(level)) {
                        /* JBL -> JLL */
                        if(!index_under_branch(curr_jp, index, level)) {
                            return 0;
                        }
                        
                        bool res = branch_to_jll(curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        arr->error.type = ErrorType::NoMem;
                        return -1;
                    }
                    
                    SearchResult search = leaf_linear_search<word8_t>(jbl->keys, pop1, 1, digit);

                    if(!search.found) {
                        return 0;
                    }
                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;
                    
                    JP* next_jp = jbl->pointers + search.pos;
                    if(next_jp->node.type != types::JPImmed_t(level - 1, 1)) {
                        curr_jp = next_jp;
                        continue;
                    }
                    
                    if(!immed1_matched(next_jp, index, level - 1 )) {
                        return 0;
                    }
                    delete_inplace<word8_t>(jbl->keys, jbl->pop0 + 1, 1, search.pos);
                    delete_inplace<JP>(jbl->pointers, jbl->pop0 + 1, sizeof(JP), search.pos);
                    if(jbl->pop0 == 0) {
                        memory::deallocate(jbl);
                        set_null_jp(curr_jp);
                        --stack_pos; 
                        goto WalkExit;
                    }
                    --jbl->pop0;

                    if(level != root_level && jbl->pop0 == 0) {
                        *curr_jp = jbl->pointers[0];

                        memory::deallocate(jbl);
                        --stack_pos; 
                    }
                    goto WalkExit;
                }
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }
                    
                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);
                    node_pop1 = get_pop0(curr_jp, level) + 1; 
                    
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold(level)) {
                        /* JBB -> JLL */
                        if(!index_under_branch(curr_jp, index, level)) {
                            return 0;
                        }

                        bool res = branch_to_jll(curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        arr->error.type = ErrorType::NoMem;
                        return -1;
                    }

                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    bitmap = jbb->subexpanses[subexpanse].bitmap;
                    bitmask = bit_pos_mask(digit);
                    
                    if(!(bitmap & bitmask)) {
                        return 0;
                    }  
                    
                    pop1 = std::popcount(bitmap);
                    offset = std::popcount(bitmap & (bitmask - 1));
                    JP* pointers = jbb->subexpanses[subexpanse].pointers;

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    if(pointers[offset].immed.type != types::JPImmed_t(level - 1, 1)) {

                        curr_jp = pointers + offset;
                        continue;
                    }
                    
                    if(!immed1_matched(&pointers[offset], index, level - 1 )) {
                        return 0;
                    }

                    if(pop1 == 1) {
                        memory::deallocate(pointers);
                        jbb->subexpanses[subexpanse].pointers = nullptr;
                    } else if(memory::can_delete_inplace(pop1, sizeof(JP))) {
                        delete_inplace<JP>(pointers, pop1, sizeof(JP), offset);
                    } else {
                        JP* new_arr = delete_with_realloc<JP>(pointers, pop1, sizeof(JP), offset);
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }
                        memory::deallocate(pointers);
                        jbb->subexpanses[subexpanse].pointers = new_arr;
                    }

                    jbb->subexpanses[subexpanse].bitmap &= ~bitmask;

                    if(pop1 > BRANCH_LINEAR_MAX) {
                        goto WalkExit;
                    } 

                    pop1 = 0;
                    for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        pop1 += std::popcount(jbb->subexpanses[subexpanse].bitmap);
                    }

                    if(pop1 == 0) {
                        memory::deallocate(jbb);
                        set_null_jp(curr_jp);
                        --stack_pos;
                    }
                    else if(pop1 <= memory::branch_bitmap_to_linear_threshold()) {
                        // jbb_to_jbl
                        bool res = jbb_to_jbl(curr_jp, level);
                        if(!res) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }
                    }

                    goto WalkExit;
                }
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;
                    digit = get_byte(index, level - 1);

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);
                    node_pop1 = get_pop0(curr_jp, level) + 1; 
                    
                    if(level != root_level && (node_pop1 - 1) <= memory::branch_to_leaf_threshold(level)) {
                        /* JBU -> JLL */
                        if(!index_under_branch(curr_jp, index, level)) {
                            return 0;
                        }
                        
                        bool res = branch_to_jll(curr_jp, node_pop1, level);
                        if(res) {
                            continue;
                        }
                        
                        arr->error.type = ErrorType::NoMem;
                        return -1;
                    }

                    if(jbu->pointers[digit].immed.type == types::JPImmed_t(level - 1, 1)) {
                        
                        if(!immed1_matched(&jbu->pointers[digit], index, level - 1 )) {
                            return 0;
                        }

                        jbu->pointers[digit].immed.type = types::Null_JP;

                        if(get_pop0(curr_jp, level) == 0) {
                            memory::deallocate(jbu);
                            set_null_jp(curr_jp);
                            
                            goto WalkExit;
                        }
                        
                        // JBU->JBB
                        if constexpr(memory::branch_uncompressed_to_bitmap_threshold()) {
                            word_t pop1 = 0;
                            for(offset = 0; offset < EXPANSE_COUNT; ++offset) {
                                pop1 += (jbu->pointers[offset].node.type != types::Null_JP);
                            }

                            if(pop1 <= memory::branch_uncompressed_to_bitmap_threshold()) {
                                bool res = jbu_to_jbb(curr_jp, level);
                                if(!res) {
                                    arr->error.type = ErrorType::NoMem;
                                    return -1;
                                }
                            }
                        }
                        // JBU -> JBL
                         
                        // any way to store pop?
                        else if constexpr(memory::branch_uncompressed_to_linear_threshold()) {
                            word_t pop1 = 0;
                            for(offset = 0; offset < EXPANSE_COUNT; ++offset) {
                                pop1 += (jbu->pointers[offset].node.type != types::Null_JP);
                            }

                            if(pop1 <= memory::branch_uncompressed_to_linear_threshold()) {
                                bool res = jbu_to_jbl(curr_jp, level);
                                if(!res) {
                                    arr->error.type = ErrorType::NoMem;
                                    return -1;
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
                        return 0;
                    }

                    word8_t* jll = get_ptr<word8_t>(curr_jp->node.addr);
                    SearchResult search = leaf_binary_search<word8_t>(jll, pop1, level, index);

                    if(!search.found) {
                        return 0;
                    }

                    // jll->jll_up_level
                    if((parent_level - 1) > level && (pop1 == memory::leaf_max_pop1(level + 1))) {
                        word_t need_words = ((level + 1) * pop1 + word_size - 1) / word_size;
                        word_t* new_arr = memory::allocate<word_t>(memory::get_capacity(need_words));
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        word8_t* new_jll = reinterpret_cast<word8_t*>(new_arr);
                        std::size_t pos = 0;
                        // jll free inside
                        copy_immedleaf_to_jll(new_jll, pos, get_byte(index, level), curr_jp, level + 1);
                    
                        set_jp(curr_jp, reinterpret_cast<word_t>(new_jll), get_decode(curr_jp, level + 1),
                            pop1 - 1, types::JLL_Level(level + 1), level + 1);

                        continue;
                    } 

                    // jll IMMED
                    if((pop1 - 1) == types::immed_max_cnt(level)) {
                        if(pop1 == 2)  {
                            word_t immed_key = 0;
                            std::memcpy(&immed_key, jll + (search.pos ? 0 : level), level);
                            set_null_jp(curr_jp);
                            set_immed1(curr_jp, immed_key, level);
                        } else {
                            delete_copy<word8_t>(curr_jp->immed.keys, jll, pop1, level, search.pos);
                        }

                        curr_jp->immed.type = types::JPImmed_t(level, pop1 - 1);
                        memory::deallocate(jll);

                        goto WalkExit;
                    }

                    // JLL inplace/realloc
                    if(memory::can_delete_inplace(pop1, level)) {
                        delete_inplace<word8_t>(jll, pop1, level, search.pos);
                    } else {
                        word8_t* new_arr = delete_with_realloc<word8_t>(jll, pop1, level, search.pos);
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }
                        curr_jp->node.addr = reinterpret_cast<word_t>(new_arr);
                        memory::deallocate(jll);
                    }

                    
                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                    
                }
                case types::JLB_1: {
                    word_t parent_level = level;
                    level = 1;
                    
                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }
                    
                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;
                    bitmask = bit_pos_mask(digit);

                    if(!(jlb->subexpanses[subexpanse] & bitmask)) {
                        return 0;
                    }
                    
                    pop1 = get_pop0(curr_jp, level) + 1;
                                        
                    jlb->subexpanses[subexpanse] &= ~bit_pos_mask(digit);
                    
                    // jlb->jll_up_level
                    if((parent_level - 1) > level && (pop1 == memory::leaf_max_pop1(level + 1))) {
                        word_t need_words = ((level + 1) * pop1 + word_size - 1) / word_size;
                        word_t* new_arr = memory::allocate<word_t>(memory::get_capacity(need_words));
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        }

                        word8_t* new_jll = reinterpret_cast<word8_t*>(new_arr);
                        std::size_t pos = 0;
                        // jlb free inside
                        copy_immedleaf_to_jll(new_jll, pos, get_byte(index, level), curr_jp, level + 1);

                        set_jp(curr_jp, reinterpret_cast<word_t>(new_jll), get_decode(curr_jp, level + 1),
                            pop1 - 2, types::JLL_Level(level + 1), level + 1);

                        goto WalkExit;
                    } 

                    // jlb -> IMMED (when happen?)
                    if((pop1 - 1) == types::immed_max_cnt(level)) {
                        word8_t* keys = curr_jp->immed.keys;
                        std::size_t pos = 0;

                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            word_t bitmap = jlb->subexpanses[subexpanse];
                            
                            while(bitmap) {
                                const std::size_t first_bit = std::countr_zero(bitmap);
                                const word8_t digit = static_cast<word_t>(subexpanse * word_bits_size + first_bit);

                                keys[pos++] = digit;

                                bitmap &= (bitmap - 1);
                            }
                        }
                        memory::deallocate(jlb);

                        curr_jp->immed.type = types::JPImmed_t(level, pop1 - 1);

                        goto WalkExit;
                    }

                    // jlb -> jll
                    if(pop1 <= memory::leaf_bitmap_to_linear_threshold()) {
                        word_t need_words = ((level * pop1 + word_size - 1) / word_size);
                        word_t need_capacity = memory::get_capacity(need_words);
                        word_t* new_arr = memory::allocate<word_t>(need_capacity);
                        if(!new_arr) {
                            arr->error.type = ErrorType::NoMem;
                            return -1;
                        } 
                        word8_t* jll = reinterpret_cast<word8_t*>(new_arr);
                        std::size_t pos = 0;
                        for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                            word_t bitmap = jlb->subexpanses[subexpanse];

                            while(bitmap) {
                                std::size_t first_bit = std::countr_zero(bitmap);
                                word8_t digit = subexpanse * word_bits_size + first_bit;
                                jll[pos++] = digit;
                                bitmap &= (bitmap - 1);
                            }
                        }
                        memory::deallocate(jlb);
                        set_jp(curr_jp, reinterpret_cast<word_t>(jll), get_decode(curr_jp, level), pop1 - 2,
                            types::JLL_Level(level), level);

                        goto WalkExit;
                    }

                    // no changes

                    stack_jps[stack_pos] = curr_jp;
                    stack_levels[stack_pos++] = level;

                    goto WalkExit;
                }
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }
                    
                    // jlb_full -> jlb
                    JLB* jlb = memory::allocate<JLB>();
                    if(!jlb) {
                        arr->error.type = ErrorType::NoMem;
                        return -1;
                    }

                    for(subexpanse = 0; subexpanse < SUBEXPANSE_COUNT; ++subexpanse) {
                        jlb->subexpanses[subexpanse] = all_ones_mask;
                    }
                    
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;
                    
                    jlb->subexpanses[subexpanse] &= ~bit_pos_mask(digit);
                    
                    set_jp(curr_jp, reinterpret_cast<word_t>(jlb), get_decode(curr_jp, level), EXPANSE_COUNT - 2,
                    types::JLB_1, level);
                    
                    goto WalkExit;
                }
                case types::JPIMMED_Base...types::JP_max_type: {
                    std::size_t k = 1;
                    while(k < types::JP_max_type && 
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level(k + 1))) {
                        ++k;
                    }
                    word_t cnt = curr_jp->node.type - types::JPImmed_t(k, 1) + 1;

                    if(cnt == 1) [[unlikely]] /* should never happened here */{
                        word_t mask = first_n_bytes_mask(k);
                        if((curr_jp->raw.w[1] & mask) != (index & mask)) {
                            return 0;
                        }
                        set_null_jp(curr_jp);
                        
                        goto WalkExit;
                    }

                    SearchResult search = leaf_linear_search<word8_t>(curr_jp->immed.keys, cnt, k, 
                                                                    index);
                    if(!search.found) {
                        return 0;
                    }
                    
                    if(cnt == 2) {
                        word_t first_key = 0, immed_key = 0;
                        std::memcpy(&first_key, curr_jp->immed.keys, k);
                        offset = (first_key == (index & first_n_bytes_mask(k)));
                        
                        std::memcpy(&immed_key, curr_jp->immed.keys + (offset ? k : 0), k);
                        set_null_jp(curr_jp);
                        set_immed1(curr_jp, immed_key, k);
                    } else {
                        // delete from JPIMMED
                        delete_inplace<word8_t>(curr_jp->immed.keys, cnt, k, search.pos);
                        curr_jp->immed.type = types::JPImmed_t(k, cnt - 1);
                    }
                    goto WalkExit;
                }
                default: [[unlikely]] {
                    arr->error.type = ErrorType::Corrupt;
                    return -1;
                }
            }
        }
        WalkExit:

        while(stack_pos-- > 0) {
            curr_jp = stack_jps[stack_pos];
            level = stack_levels[stack_pos];

            word_t new_pop0 = get_pop0(curr_jp, level) - 1;
            set_pop0(curr_jp, new_pop0, level);
        }

        --arr->pop0;

        return 1;
    }

    // 0 - no index
    // 1 - has index
    // -1 - error (check jpm)
    int judy_test(JPM* const arr, word_t index) {
    word_t level, digit, offset, subexpanse, pop1, bitmap, bitmask, cnt;
    using namespace detail;

        if(!arr) [[unlikely]] {
            return -1;
        }
        
        JP* curr_jp = &arr->top_jp;

        while(true) {
            switch(curr_jp->node.type) {
                case types::Null_JP: {
                    return 0;
                }
                // JBL
                case types::JBL_Level(2)...types::JBL_Level(root_level): {

                    level = curr_jp->node.type - types::JBL_Base + 2;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    digit = get_byte(index, level - 1);     
                    JBL* jbl = get_ptr<JBL>(curr_jp->node.addr);
                    pop1 = jbl->pop0 + 1;

                    offset = 0;
                    while(offset < pop1 && jbl->keys[offset] < digit) {
                        ++offset;
                    }

                    if(offset == pop1 || jbl->keys[offset] != digit) {
                        return 0;
                    }

                    curr_jp = jbl->pointers + offset;
                    continue;
                }
                // JBB
                case types::JBB_Level(2)...types::JBB_Level(root_level): {
                    level = curr_jp->node.type - types::JBB_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }
                    
                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    JBB* jbb = get_ptr<JBB>(curr_jp->node.addr);

                    bitmap          = jbb->subexpanses[subexpanse].bitmap;
                    JP* pointers    = jbb->subexpanses[subexpanse].pointers;
                    
                    bitmask = bit_pos_mask(digit);
                    if(!(bitmap & bitmask)) {
                        return 0;
                    }

                    offset = std::popcount(bitmap & (bitmask - 1));
                    curr_jp = pointers + offset;
                    continue;
                }
                // JBU
                case types::JBU_Level(2)...types::JBU_Level(root_level): {
                    level = curr_jp->node.type - types::JBU_Base + 2;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    digit = get_byte(index, level - 1);

                    JBU* jbu = get_ptr<JBU>(curr_jp->node.addr);
                    curr_jp = jbu->pointers + digit;
                    continue;
                }
                // JLL
                case types::JLL_Level(1)...types::JLL_Level(root_level - 1): {
                     level = curr_jp->node.type - types::JLL_Base + 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    pop1 = get_pop0(curr_jp, level) + 1;
                    word8_t* jll = get_ptr<word8_t>(curr_jp->node.addr);

                    return leaf_binary_search<word8_t>(jll, pop1, level, index).found;
                }
                // JLB1
                case types::JLB_1: {
                    level = 1;

                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }

                    digit = get_byte(index, level - 1);
                    subexpanse = digit / word_bits_size;

                    JLB* jlb = get_ptr<JLB>(curr_jp->node.addr);

                    bitmap = jlb->subexpanses[subexpanse];
                    bitmask = bit_pos_mask(digit);

                    return (bitmap & bitmask) != 0;
                }
                // FULLPOP
                case types::JLB_FULL: {
                    level = 1;
                    if(decode_not_matched(index, curr_jp, level)) {
                        return 0;
                    }
                    return 1;
                }
                //JPIMMED
                case types::JPIMMED_Base...types::JP_max_type: {

                    std::size_t k = 1;
                    while(k < types::JP_max_type && /* to avoid memory dumps */
                        curr_jp->node.type >= (types::JPIMMED_Base + types::immed_level(k + 1))) {
                        ++k;
                    }
                    cnt = curr_jp->node.type - types::JPImmed_t(k, 1) + 1;

                    if(cnt == 1) {
                        word_t mask = first_n_bytes_mask(k);
                        return (curr_jp->raw.w[1] & mask) == (index & mask);
                    }
                    
                    return leaf_linear_search<word8_t>(curr_jp->immed.keys, cnt, k, index).found;
                }
                default: [[unlikely]] {
                    arr->error.type = ErrorType::Corrupt;
                    return -1;
                }
            }
        }
    }

    // return popultaion [start_index, end_index]
    // or -1 (if has error - check jpm)
    word_t judy_count(JPM* const arr, const word_t start_index, const word_t end_index) {
        return static_cast<word_t>(0);
    }

    // 1 - ans contain first 1 at [index, -1]
    // 0 - don't have 1 at [index, -1]
    // -1 - error (check jpm)
    int judy_first(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_next(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_last(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_prev(JPM* const arr, const word_t index) {
        return 0;
    }
    
    int judy_first_empty(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_next_empty(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_last_empty(JPM* const arr, const word_t index) {
        return 0;
    }

    int judy_prev_empty(JPM* const arr, const word_t index) {
        return 0;
    }

    void judy_clear_array(JPM* jpm) {
        if(!jpm) {
            return;
        }
        detail::free_jp(&jpm->top_jp, jpm);
        jpm->top_jp.raw.w[0] = jpm->top_jp.raw.w[1] = 0;
        jpm->pop0 = 0;
    }

    word_t by_count(JPM* const jpm, std::size_t pos) {
        return 0;
    }

    void judy_free_array(JPM* jpm) {
        if(!jpm) {
            return;
        }

        judy_clear_array(jpm);
        memory::deallocate(jpm);
    }

    
    // full copy of judy array, return pointer to new judy array
    JPM* judy_copy_array() {
        return nullptr;
    }

    using judy1 = JPM;

    class Judy1 {
    public:

        Judy1()  {
            arr_ = judy::judy_create();
            if(!arr_) {
                ;// throw? or outside should check
            }
        }

        int get(const judy::word_t index) const {
            return judy::judy_test(arr_, index);
        }

        int set(const judy::word_t index) {
            return judy::judy_set(arr_, index);
        }

        int unset(const judy::word_t index) {
            return judy::judy_unset(arr_, index);
        }

        judy::word_t count(const judy::word_t left, const judy::word_t right) const {
            return judy::judy_count(arr_, left, right);
        }

        judy::word_t next(const judy::word_t index) const {
            return judy::judy_next(arr_, index);
        }

        judy::word_t prev(const judy::word_t index) const {
            return judy::judy_prev(arr_, index);
        }

        judy::word_t first(const judy::word_t index) const  {
            return judy::judy_first(arr_, index);
        }

        judy::word_t last(const judy::word_t index) const {
            return judy::judy_last(arr_, index);
        }

        judy::word_t next_empty(const judy::word_t index) const {
            return judy::judy_next_empty(arr_, index);
        }

        judy::word_t prev_empty(const judy::word_t index) const {
            return judy::judy_prev_empty(arr_, index);
        }

        judy::word_t first_empty(const judy::word_t index) const {
            return judy::judy_first_empty(arr_, index);
        }

        judy::word_t last_empty(const judy::word_t index) const {
            return judy::judy_last_empty(arr_, index);
        }

        judy::word_t by_count(std::size_t pos) const {
            return judy::by_count(arr_, pos);
        }

        void clear() {
            judy::judy_clear_array(arr_);
        }

        ~Judy1() {
            if(arr_) {
                judy::judy_free_array(arr_);
            }
        }

        bool is_ok()    const {
            return (arr_->error.type == ErrorType::NotError);
        }

    private:

        JPM* arr_;

    };


};
