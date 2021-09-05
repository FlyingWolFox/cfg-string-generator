#include <iostream>
#include <cstdlib>
#include "../cfg_string_generator.hpp"

template <typename Container>
void print_strings(Container& strings)
{
    for (auto& s: strings) {
            std::cout << s;
            std::cout << std::endl;
    }
    std::cout << std::endl;
}

template <bool low_mem, typename Container>
void print_derivations(Container& derivations)
{
    for (auto& s: derivations) {
        std::cout << s.first << " -> " << std::endl;
        for (auto& derivation: s.second) {
            for (auto& der: derivation) {
                if constexpr(low_mem)
                    std::cout << "(" << *der << "), ";
                else
                    std::cout << "(" << der.first << ", " << *der.second << "), ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

template <typename Container>
void print_count(Container& strings)
{
    for (auto& s: strings) {
            std::cout << s.first << " -> " << s.second << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::unordered_map<char, std::vector<std::string>> rules;
    size_t max_depth = 6;

    rules['S'] = {"0A", "1B"};
    rules['A'] = {"0AA", "1S", "1"};
    rules['B'] = {"1BB", "0S", "0"};

    auto derivations1 = cfg_string_gen::cfg_string_generator<true, cfg_string_gen::enabled, true>(rules, max_depth);
    auto derivations2 = cfg_string_gen::cfg_string_generator<true, cfg_string_gen::disabled, false>(rules, max_depth);

    auto strings = cfg_string_gen::cfg_string_generator<false, cfg_string_gen::enabled, false>(rules, max_depth);
    auto count_strings = cfg_string_gen::cfg_string_generator<false, cfg_string_gen::count, false>(rules, max_depth);

    std::cout << "Strings:" << std::endl;
    print_strings(strings);
    std::cout << std::endl;

    std::cout << "With derivations, without nonterminal index:" << std::endl;
    print_derivations<true>(derivations1);
    std::cout << std::endl;

    std::cout << "With one derivation per string:" << std::endl;
    print_derivations<false>(derivations2);

    std::cout << "Strings with count:" << std::endl;
    print_count(count_strings);
}
