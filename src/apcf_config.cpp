#include "apcf_.hpp"

#include <cstring>



#define INVALID_VALUE_STR(TYPE_) ( \
	"cannot get " + \
	std::string(apcf::dataTypeStringOf(TYPE_)) + \
	" value" \
)



namespace {

	bool cmpKeyPrefix(const apcf::Key& prefix, const apcf::Key& cmp) {
		if(cmp.size() < prefix.size() + 1) return false;
		if(0 != ::strncmp(prefix.data(), cmp.data(), prefix.size())) return false;
		return cmp[prefix.size()] == '.';
	}

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


	void Config::mergeAsGroup(const Key& groupKey, const Config& cfg) {
		for(const auto& entry : cfg) {
			set(groupKey + '.' + entry.first, entry.second);
		}
	}

	void Config::mergeAsGroup(const Key& groupKey, Config&& cfg) {
		for(const auto& entry : cfg) {
			set(groupKey + '.' + std::move(entry.first), std::move(entry.second));
		}
	}


	decltype(Config::data_)::const_iterator Config::begin() const {
		return data_.begin();
	}

	decltype(Config::data_)::const_iterator Config::end() const {
		return data_.end();
	}


	size_t Config::entryCount() const { return data_.size(); }


	ConfigHierarchy Config::getHierarchy() const {
		return ConfigHierarchy(data_);
	}


	Config Config::getSubconfig(const Key& key) const {
		Config r;
		auto cur = data_.lower_bound(key);
		auto end = data_.end();
		if(cur != end) {
			while(cmpKeyPrefix(key, cur->first)) {
				assert(cur->first.size() > key.size());
				auto oldSize = cur->first.size();
				auto newSize = oldSize - (key.size() + 1);
				r.set(Key(cur->first.data() + oldSize - newSize, newSize), cur->second);
				++ cur;
			}
		}
		return r;
	}


	std::optional<const RawData*> Config::get(const Key& key) const noexcept {
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
					return string_t(data.stringValue);
				}
				case DataType::eArray: {
					concat = GRAMMAR_ARRAY_BEGIN + " "s;
					for(size_t i=0; i < data.arrayValue.size(); ++i) {
						concat.append(data.arrayValue[i].serialize());
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
			return std::string(data.stringValue);
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
					r = array_span_t(data.arrayValue.data(), data.arrayValue.size());
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


	void Config::set(Key key, RawData data) noexcept {
		data_[key] = std::move(data);
	}

	void Config::setBool(Key key, bool value) noexcept {
		data_[key] = RawData(value);
	}

	void Config::setInt(Key key, int_t value) noexcept {
		data_[key] = RawData(value);
	}

	void Config::setFloat(Key key, float_t value) noexcept {
		data_[key] = RawData(value);
	}

	void Config::setString(Key key, string_t value) noexcept {
		data_[key] = RawData(value);
	}

	void Config::setArray(Key key, array_t array) noexcept {
		data_[key] = RawData::moveArray(array.data(), array.size());
	}

}
