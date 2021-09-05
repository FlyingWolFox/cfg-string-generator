#ifndef CFG_STRING_GENERATOR_H
#define CFG_STRING_GENERATOR_H

#include "BlockingCollection/BlockingCollection.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace cfg_string_gen
{
	namespace detail {

		template <typename T>
		class count_set {
			// a set that counts occurrences
			// it looks weird because is designed to be use by cfg_string_gen
			// and takes advantage that the generator will call a different insertion
			// function than the SetQueue. With this, the set can return strings to
			// the generator without losing track of occurrences.
			// this is done by last_removed, since upon removal derivation will be done
			// or a insertion on done_string will be made
			using map_type = std::unordered_map<T, std::size_t>;
			static std::pair<T, std::size_t> last_removed;
		public:
			map_type map; 

			// generator stuff

			void insert(typename map_type::iterator hint, std::string&& value)
			{
				this->map.insert(std::move(last_removed));
			}

			typename map_type::iterator end() { return map.end(); }

			// BlockingCollection stuff

			using value_type = T;
			using size_type = std::size_t;

			std::pair<const std::string*, bool> insert(const std::string& value)
			{
				auto [it, success] = map.insert({value, this->last_removed.second});
				if (!success) {
					it->second += this->last_removed.second;
				}
				return {&(it->first), success};
			}

			std::pair<const std::string*, bool>  insert(std::string&& value)
			{
				auto last_removed_count = this->last_removed.second;
				auto [it, success] = map.emplace(value, this->last_removed.second);
				if (!success) {
					it->second += last_removed_count;
				}
				return {&(it->first), success};
			}

			size_type erase(const std::string& key)
			{
				this->last_removed = {key, std::move(map[key])};
				return this->map.erase(key);
			}
		};
		template<typename T>
		std::pair<T, std::size_t> count_set<T>::last_removed("", 1);

		// generate strings using a controlled queue, that has a "null" string on it
		// to split different depths and stop when max depth has been reached
		// expects T to be a std::string
		template<typename T, typename OutContainer, typename QueueContainer, typename rules_type>
		OutContainer gen_controlled_queue(const rules_type& rules, std::size_t depth)
		{
			T nonterminals;
			nonterminals.reserve(rules.size());
			for (auto& c: rules) {
				nonterminals.push_back(c.first);
			}
			QueueContainer queue;
			queue.try_add({"S"});  // start nonterminal
			OutContainer done_strings;

			T null = {"00"};
			null[0] = '\0';
			for (;depth > 0; depth--) {
				// using string with null to split strings from different depths
				queue.try_add(null);
				while (true) {
					T s; 
					queue.try_take(s);
					if (s[0] == '\0' && s.size() > 0) break;
					std::size_t pos = s.find_first_of(nonterminals);
					if (pos == T::npos) {
						done_strings.insert(done_strings.end(), std::move(s));
						continue;
					}
					auto c = s.at(pos);
					for (auto& substitution: rules.at(c)) {
						T new_string(s);
						new_string.replace(pos, 1, substitution);
						queue.try_add(new_string);
					}

				}
			}
			// get the last strings generated
			while (queue.size() != 0) {
				T s; 
				queue.try_take(s);
				std::size_t pos = s.find_first_of(nonterminals);
				if (pos == T::npos) {
					done_strings.insert(done_strings.end(), std::move(s));
				}
			}

			return done_strings;
		}

		// generates strings + derivations using a free queue that, differently from controlled queue,
		// controls max depth by the size of the derivations vector
		// T is expected to be a std::pair<std::string, std::vector<derivations_type>>
		template<bool low_mem, typename T, typename OutContainer, typename QueueContainer, typename rules_type>
		OutContainer gen_free_queue(const rules_type& rules, std::size_t depth)
		{
			std::string nonterminals;
			nonterminals.reserve(rules.size());
			for (auto& c: rules) {
				nonterminals.push_back(c.first);
			}
			QueueContainer queue;
			queue.try_add({"S", {{}}});
			OutContainer done_strings;

			while (queue.size() != 0) {
				T s; 
				queue.try_take(s);
				std::size_t pos = s.first.find_first_of(nonterminals);
				if (pos == std::string::npos) {
					done_strings.insert(done_strings.end(), std::move(s));
					continue;
				}
				// derivate with the first substitution
				const auto& nonterminal = s.first.at(pos);
				const auto* substitution = &(rules.at(nonterminal).front());
				decltype(s.second) derivations;
				for (auto& derivation: s.second) {
					if (derivation.size() >= depth) continue;
					derivations.push_back(std::move(derivation));
					if constexpr(low_mem)
						derivations.back().push_back(substitution);
					else
						derivations.back().push_back({pos, substitution});
				}
				if (derivations.empty()) continue;

				std::string new_string(s.first);
				new_string.replace(pos, 1, *substitution);
				queue.try_add({std::move(new_string), derivations});

				// derivate the rest.
				// this way is probably more performant due to no copying of the derivations (or s.second) vector
				for (auto it = ++rules.at(nonterminal).begin(); it != rules.at(nonterminal).end(); it++) {
					substitution = &(*it);
					for (auto& derivation: derivations) {
						if constexpr(low_mem)
							derivation.back() = substitution;
						else
							derivation.back() = {pos, substitution};
					}
					std::string new_string(s.first);
					new_string.replace(pos, 1, *substitution);
					queue.try_add({std::move(new_string), derivations});
				}
			}

			return done_strings;
		}
	}

	enum repetition_mode {
		disabled,
		enabled,
		count
	};

	/*
	The template parameters modify the behavior of the generator as follows:

	* derivation: enables storage of derivations. If disabled, a simple container containing generated strings will be returned, if enabled, a std::unordered_map will be returned, containing the the generated string as keys and a std::vector containing string derivations

	* repetition: dictates how the generator should handle repeated string (happens on ambiguous grammars). Use a cfg_string_gen::repetition_mode here

	* low_memory: controls if derivations should also store the position of the nonterminal (redundant information). If true these positions aren't stored. The memory impact of this depends on the grammar

	* RulesMap: the type of the rules parameter, which should be a map type like std::map. Should be deducted from the function parameters

	The parameters are as follows:

	* rules: a map containing the context free grammar rules. The keys should be the nonterminals and the values a container with the rules. Nonterminals are expected to be a single char, and the nonterminal 'S' must be present

	* max_depth: the max depth which the generator should go. Since the generation has exponential behavior and grammars may generate infinite languages, this controls memory usage. A depth of 0 means no derivation has been made, and a depth of 1 means all possible rules were applied with the leftmost nonterminal
	*/
	template <bool derivation = false, int repetition = repetition_mode::disabled, bool low_memory = false, template <typename Key, typename T> typename RulesMap = std::unordered_map>
	auto cfg_string_generator(const RulesMap<char, std::vector<std::string>>& rules, std::size_t max_depth)
	{
		using derivation_type = std::conditional_t<low_memory, const std::string*, std::pair<std::size_t, const std::string*>>; 
		using derivations_type = std::vector<derivation_type>;
		using string_derivation_type = std::pair<std::string, std::vector<derivations_type>>;

		struct copy {
			void operator()(std::vector<derivations_type>& value, const std::vector<derivations_type>& new_value)
			{ 
				std::copy(new_value.begin(), new_value.end(), std::back_inserter(value));
			}
		};
		struct move {
			void operator()(std::vector<derivations_type>& value, std::vector<derivations_type>&& new_value)
			{
				std::move(new_value.begin(), new_value.end(), std::back_inserter(value));
			}
		};

		using additive_queue = code_machina::AdditiveMapQueueContainer<std::string, std::vector<derivations_type>, copy, move, std::unordered_map>;
		using conservative_queue = code_machina::ConservativeMapQueueContainer<std::string, std::vector<derivations_type>, std::unordered_map>;
		using queue = code_machina::QueueContainer<std::string>;
		using set_queue = code_machina::SetQueueContainer<std::string, std::unordered_set>;

		if constexpr(derivation) {
			using Container = std::unordered_map<std::string, std::vector<derivations_type>>;
			using QueueContainer = std::conditional_t<repetition, additive_queue, conservative_queue>;
			return detail::gen_free_queue<low_memory, string_derivation_type, Container, QueueContainer>(rules, max_depth);
		}
		else {
			if constexpr(repetition == repetition_mode::count) {
				using Container = detail::count_set<std::string>;
				using QueueContainer = code_machina::SetQueueContainer<std::string, detail::count_set>;
				detail::count_set count_set = detail::gen_controlled_queue<std::string, Container, QueueContainer>(rules, max_depth);
				return std::unordered_map<std::string, std::size_t>(std::move(count_set.map));
			}
			else {
				using Container = std::conditional_t<repetition, std::vector<std::string>, std::unordered_set<std::string>>;
				using QueueContainer = std::conditional_t<repetition, queue, set_queue>;
				return detail::gen_controlled_queue<std::string, Container, QueueContainer>(rules, max_depth);
			}
		}
	} 
}
#endif // CFG_STRING_GENERATOR_H
