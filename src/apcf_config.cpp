#include "apcf_.hpp"

#include <cstring>



#define INVALID_VALUE_STR(TYPE_) ( \
	"cannot get " + \
	std::string(apcf::dataTypeStringOf(TYPE_)) + \
	" value" \
)



namespace {

	// Used exclusively as a pseudo-almost-functor for ConfigHierarchy::autocomplete
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

	void Config::merge(const Config& r) {
		for(const auto& entry : r.data_) {
			data_.insert_or_assign(entry.first, entry.second);
		}
	}

	void Config::merge(Config&& r) {
		for(auto& entry : r.data_) {
			data_.insert_or_assign(std::move(entry.first), std::move(entry.second));
		}
	}


	decltype(Config::data_)::const_iterator Config::begin() const {
		return data_.begin();
	}

	decltype(Config::data_)::const_iterator Config::end() const {
		return data_.end();
	}


	size_t Config::keyCount() const { return data_.size(); }


	ConfigHierarchy Config::getHierarchy() const {
		return ConfigHierarchy(data_);
	}


	std::optional<const RawData*> Config::get(const Key& key) const {
		std::optional<const RawData*> r = std::nullopt;
		auto found = data_.find(key.asString());
		if(found != data_.end()) r = &found->second;
		return r;
	}

	std::optional<bool> Config::getBool(const Key& key) const {
		using namespace std::string_literals;
		auto found = get(key);
		if(found.has_value()) {
			assert((found.value() != nullptr) && (found.value()->type != DataType::eNull));
			if(found.value()->type != DataType::eBool) {
				throw InvalidValue(
					found.value()->serialize(), found.value()->type,
					INVALID_VALUE_STR(found.value()->type) + " \""s + key.asString() + "\" as a bool value");
			}
			return found.value()->data.boolValue;
		} else {
			return std::nullopt;
		}
	}

	std::optional<int_t> Config::getInt(const Key& key) const {
		using namespace std::string_literals;
		auto found = get(key);
		if(found.has_value()) {
			assert((found.value() != nullptr) && (found.value()->type != DataType::eNull));
			switch(found.value()->type) {
				case DataType::eInt: {
					return found.value()->data.intValue;
				}
				case DataType::eFloat: {
					return found.value()->data.floatValue;
				}
				default: {
					throw InvalidValue(
						found.value()->serialize(), found.value()->type,
						INVALID_VALUE_STR(found.value()->type) + " \""s + key.asString() + "\" as an integer value");
				}
			}
			return found.value()->data.boolValue;
		} else {
			return std::nullopt;
		}
	}

	std::optional<float_t> Config::getFloat(const Key& key) const {
		using namespace std::string_literals;
		auto found = get(key);
		if(found.has_value()) {
			assert(found.value() != nullptr);
			switch(found.value()->type) {
				case DataType::eInt: {
					return found.value()->data.intValue;
				}
				case DataType::eFloat: {
					return found.value()->data.floatValue;
				}
				default: {
					throw InvalidValue(
						found.value()->serialize(), found.value()->type,
						INVALID_VALUE_STR(found.value()->type) + " \""s + key.asString() + "\" as a fractional value");
				}
			}
			return found.value()->data.boolValue;
		} else {
			return std::nullopt;
		}
	}

	std::optional<string_t> Config::getString(const Key& key) const {
		using namespace std::string_literals;
		auto found = get(key);
		if(found.has_value()) {
			assert((found.value() != nullptr) && (found.value()->type != DataType::eNull));
			const auto& data = found.value()->data;
			std::string concat;
			switch(found.value()->type) {
				case DataType::eInt: {
					return std::to_string(data.intValue);
				}
				case DataType::eFloat: {
					return std::to_string(data.floatValue);
				}
				case DataType::eString: {
					return std::string(data.stringValue.ptr, data.stringValue.size);
				}
				case DataType::eArray: {
					concat = GRAMMAR_ARRAY_BEGIN + " "s;
					for(size_t i=0; i < data.arrayValue.size; ++i) {
						concat.append(data.arrayValue.ptr[i].serialize());
						concat.push_back(' ');
					}
					concat.push_back(GRAMMAR_ARRAY_END);
					return concat;
				}
				default: {
					throw InvalidValue(
						found.value()->serialize(), found.value()->type,
						INVALID_VALUE_STR(found.value()->type) + " \""s + key.asString() + "\" as a string");
				}
			}
			return std::string(data.stringValue.ptr, data.stringValue.size);
		} else {
			return std::nullopt;
		}
	}

	std::optional<array_span_t> Config::getArray(const Key& key) const {
		using namespace std::string_literals;
		array_span_t r;
		auto found = get(key);
		if(found.has_value()) {
			assert((found.value() != nullptr) && (found.value()->type != DataType::eNull));
			switch(found.value()->type) {
				case DataType::eArray: {
					const auto& data = found.value()->data;
					r = array_span_t(data.arrayValue.ptr, data.arrayValue.size);
					return r;
				}
				default: {
					const apcf::RawData& value = *found.value();
					r = array_span_t(&value, 1);
					return r;
				}
			}
		} else {
			return std::nullopt;
		}
	}


	void Config::set(const Key& key, RawData data) {
		data_[key] = std::move(data);
	}

	void Config::setBool(const Key& key, bool value) {
		data_[key] = RawData(value);
	}

	void Config::setInt(const Key& key, int_t value) {
		data_[key] = RawData(value);
	}

	void Config::setFloat(const Key& key, float_t value) {
		data_[key] = RawData(value);
	}

	void Config::setString(const Key& key, string_t value) {
		data_[key] = RawData(value);
	}

	void Config::setArray(const Key& key, array_t array) {
		data_[key] = RawData::moveArray(array.data(), array.size());
	}



	apcf::ConfigHierarchy::ConfigHierarchy(const std::map<apcf::Key, apcf::RawData>& cfg) {
		for(const auto& entry : cfg) {
			putKey(entry.first);
		}
	}


	void apcf::ConfigHierarchy::putKey(const apcf::Key& key) {
		constexpr auto splitKey = [](const apcf::Key& key) {
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


	const std::set<apcf::Key>& apcf::ConfigHierarchy::getSubkeys(const Key& ref) const {
		static const std::set<Key> emptySet = { };
		auto found = tree_.find(ref);
		if(found == tree_.end()) return emptySet;
		return found->second;
	}


	bool apcf::ConfigHierarchy::collapse_(KeySpan ref, KeySpan parent) {
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


	const Key& apcf::ConfigHierarchy::autocomplete(const Key& base) const {
		if(base.empty()) return base;
		auto subkeys = getSubkeys(base);
		if(subkeys.size() == 1) {
			return AutocompleteRecursiveState(*this, base);
		} else {
			return base;
		}
	}


	// void apcf::ConfigHierarchy::collapse() {
	// 	std::set<KeySpan> collapsed;
	//
	// 	std::vector<KeySpan> candidates;
	// 	auto mkCandidates = [&]() {
	// 		const auto& candidatesSet = tree_[{ }];
	// 		candidates.clear();
	// 		if(candidates.capacity() < candidatesSet.size()) {
	// 			candidates.reserve(candidatesSet.size());
	// 		}
	// 		for(const auto& entry : candidatesSet) {
	// 			candidates.push_back(KeySpan(entry));
	// 		}
	// 	};
	//
	// 	/* Whenever `collapse_(...)` returns true, the tree has been modified
	// 	 * and the iterators are out of date. */
	// 	auto runPass = [&]() {
	// 		mkCandidates();
	// 		auto iter = candidates.begin();
	// 		auto end = candidates.end();
	// 		while(iter != end) {
	// 			if(collapse_(*iter, { })) {
	// 				return true;
	// 			}
	// 			++iter;
	// 		}
	// 		return false;
	// 	};
	//
	// 	while(runPass()) {
	// 		// NOP
	// 	}
	// }


	// void ConfigHierarchy::stretch() {
	// 	/* This implementation just reconstruct the hierarchy, which is
	// 	 * way heavier than necessary but it saves me a headache. */
	// 	auto oldTree = std::move(tree_);
	// 	tree_.clear();
	//
	// 	for(const auto& parenthood : oldTree) {
	// 		for(const auto& childKey : parenthood.second) {
	// 			putKey(childKey);
	// 		}
	// 	}
	// }

}
