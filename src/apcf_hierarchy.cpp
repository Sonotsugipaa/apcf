#include "apcf_.hpp"



namespace {

	struct AutocompleteRecursiveState {
		const apcf::ConfigHierarchy* hierarchy;
		const apcf::Key* returnKey;

		bool recurse() {
			const auto& subkeys = hierarchy->getSubkeys(*returnKey);
			if(subkeys.size() == 1) {
				returnKey = &(*subkeys.begin());
				return true;
			} else {
				return false;
			}
		}

		AutocompleteRecursiveState(
				const apcf::ConfigHierarchy& hierarchy,
				const apcf::Key& key
		):
				hierarchy(&hierarchy),
				returnKey(&key)
		{
			while(recurse()) {
				// NOP
			}
		}

		operator const apcf::Key&() const {
			return *returnKey;
		}
	};

}



namespace apcf {

	ConfigHierarchy::ConfigHierarchy(const std::map<Key, RawData>& cfg) {
		for(const auto& entry : cfg) {
			putKey(entry.first);
		}
	}


	void ConfigHierarchy::putKey(const Key& key) {
		constexpr auto splitKey = [](const Key& key) {
			constexpr size_t keyBasenameSizeHeuristic = 5;
			std::vector<KeySpan> r;
			r.reserve(key.size() / keyBasenameSizeHeuristic);
			const char* beg = key.data();
			const char* end = key.data() + key.size();
			const char* cur = beg;

			assert(beg != end);
			while(cur != end) {
				++ cur;
				while(cur != end && *cur != GRAMMAR_KEY_SEPARATOR) {
					++ cur;
				}
				r.push_back(KeySpan(beg, size_t(cur-beg)));
			}

			return r;
		};

		Key lastKey = { };
		auto insertKey = [&](Key dstKeySpan) {
			tree_[lastKey].insert(dstKeySpan);
			lastKey = std::move(dstKeySpan);
		};

		auto split = splitKey(key);
		size_t size = split.size();
		for(size_t i=0; i < size; ++i) {
			insertKey(Key(split[i]));
		}
	}


	const std::set<Key>& ConfigHierarchy::getSubkeys(const Key& ref) const {
		static const std::set<Key> emptySet = { };
		auto found = tree_.find(ref);
		if(found == tree_.end()) return emptySet;
		return found->second;
	}


	bool ConfigHierarchy::collapse_(KeySpan ref, KeySpan parent) {
		/* Recursive condition on `n` (number of subkeys mapped to `ref`)
		 * n=0  =>  Return value is false; end of recursion branch.
		 * n=1  =>  Return value is true; if possible, move the subkey mapping to `parent`.
		 * n>0  =>  Return value is true; recursively call `collapse` on every subkey. */
		if(ref.empty()) return false;
		auto subkeys0set = &getSubkeys(Key(ref));
		bool r = false;
		if(! subkeys0set->empty()) {
			std::vector<Key> subkeys0 = apcf_util::setToVec(*subkeys0set);
			if(subkeys0.size() == 1) {
				auto subkey0 = *subkeys0.begin();
				tree_[Key(parent)].insert(subkey0);
				tree_.erase(Key(ref));
				collapse_(KeySpan(subkey0), parent);
				r = true;
			} else {
				for(const auto& subkey0 : subkeys0) {
					r = collapse_(KeySpan(subkey0), ref) || r;
				}
			}
		}
		return r;
	}


	const Key& ConfigHierarchy::autocomplete(const Key& base) const {
		if(base.empty()) return base;
		auto subkeys = getSubkeys(base);
		if(subkeys.size() == 1) {
			return AutocompleteRecursiveState(*this, base);
		} else {
			return base;
		}
	}

}
