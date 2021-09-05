# cfg-string-generator

A library that generates strings for context free grammars

## How to use

To get a container with the derivations call:

```cpp
template <bool derivation = false, int repetition = cfg_string_gen::repetition_mode::disabled, bool low_memory = false, template <typename Key, typename T> typename RulesMap = std::unordered_map>
auto cfg_string_gen::cfg_string_generator(const RulesMap<char, std::vector<std::string>>& rules, std::size_t max_depth)
```

The template parameters modify the behavior of the generator as follows:

* `derivation`: enables storage of derivations. If disabled, a simple container containing generated strings will be returned, if enabled, a `std::unordered_map` will be returned, containing the the generated string as keys and a `std::vector` containing string derivations. Default is `false`
* `repetition`: dictates how the generator should handle repeated string (happens on ambiguous grammars). Use a `cfg_string_gen::repetition_mode` here. Possible values are:
  * `repetition_mode::disabled` (aka `0`): repetitions won't be recorded. If `derivation` is false, this'll make the function return a `std::unordered_set` of strings, if not, each string on the map will have only one derivation (the first one to have generated the string)
  * `repetition_mode::enabled` (aka `1`): repetitions will be recorded. If `derivation` is false, a `std::vector` will be returned containing all strings, if not, each string on the map will have all possible derivations
  * `repetition_mode::count` (aka `2`): count the number of repetitions. This'll have effect only if `derivation` is `false`, if not behavior is equal of `repetition_mode::enabled`. Returns a `std::unordered_map` containing the strings as keys and the number of occurrences as values
* `low_memory`: controls if derivations should also store the position of the nonterminal (redundant information). If `true` these positions aren't stored. The memory impact of this depends on the grammar
* `RulesMap`: the type of the rules parameter, which should be a map type like `std::map`. Should be deducted from the function parameters

The parameters are as follows:

* `rules`: a map containing the context free grammar rules. The keys should be the nonterminals and the values a container with the rules. Nonterminals are expected to be a single char, and the nonterminal 'S' must be present
* `max_depth`: the max depth which the generator should go. Since the generation has exponential behavior and grammars may generate infinite languages, this controls memory usage. A depth of 0 means no derivation has been made, and a depth of 1 means all possible rules were applied with the leftmost nonterminal

There's usage examples in `examples` directory

## Return values and structures

The return type of the generator depends on the template parameters, they will be:

* `std::vector<std::string>`: Contains all generated strings in some order. The primary ordering is the number of derivations necessary to generate the string an the secondary is dependent on the ordering of the nonterminals on the rules map. Returned when `derivation` is `false` and `repetition` is `repetition_mode::enabled`
* `std::unordered_set<std::string>`: Contains all generated strings without repetitions. Returned when `derivation` is `false` and `repetition` is `repetition_mode::disabled`
* `std::unordered_map<std::string, std::size_t>`: Contains all generated strings with a occurrence count. Returned when `derivation` is `false` and `repetition` is `repetition_mode::count`
* `std::unordered_map<std::string, std::vector<std::vector<std::pair<std::size_t, const std::string*>>>>`: Contains strings and it's derivations. The vector of pairs is a single derivation. The first member of pair is the position of the nonterminal replaced, and the second member is the substitution made. Returned when `derivation` is `true` and `low_memory` is `false`
* `std::unordered_map<std::string, std::vector<std::vector<const std::string*>>>`: Contains strings and it's derivations. The vector of pointers is a single derivation. The pointer have the address of the derivation string on the rules_map. Returned when `derivation` is `true` and `low_memory` is `true`

Note that derivation will have pointers to strings on the rules map. Changing the map may cause dangling pointers

## Warning

Due to the exponential nature of string generation this will use a **lot** of memory, use with caution, controlling the `max_depth`. Ways to get minimum memory usage:

* Enable storage of derivations just when needed
* Consider enabling `low_memory`. In simpler grammars the effect of this is noticeable (see one in the example directory)
* Enable repetition storage just when needed, both for derivation storage or not. In ambiguous grammars this'll save a lot of memory
* If you need repetitions in just string generation, consider using `repetition_mode::count`. It gives the occurrence number and saves memory

Of course all memory saving measures may affect performance, mostly a small bit.

However **always** take care of `max_depth` value. Just an increment may increase memory usage by a lot (manly when repetitions are being stored)

## Dependency

This library use a fork of BlockingCollection. This doesn't use the BlockingCollection, since this is not multithreaded, but its queue types

## Contributing

Any performance, functionality and memory consumption improvements are welcome. Bug reports and documentation improvements are also welcome
