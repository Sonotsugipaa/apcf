#include <yaconfig.hpp>

#include <cassert>

// Files for the quasi-unity build
#include "yaconfig_util.cpp"
#include "yaconfig_num.cpp"
#include "yaconfig_parse.cpp"
#include "yaconfig_serialize.cpp"



namespace yacfg {

	bool isKeyValid(const std::string& str) {
		assert(findKeyError(str) <= str.size());
		return findKeyError(str) == str.size();
	}



	InvalidKey::InvalidKey(std::string key, size_t charPos):
			ConfigError("invalid key: \"" + key + "\" [" + std::to_string(charPos) + ']'),
			key_(std::move(key)),
			pos_(charPos)
	{ }



	InvalidValue::InvalidValue(
			const std::string& valueRep, DataType type,
			const std::string& reason
	):
			ConfigError("invalid value: " + reason),
			value_(std::move(valueRep)),
			type_(type)
	{ }



	UnexpectedChar::UnexpectedChar(
			size_t line, size_t lineChar, char whichChar, const std::string& expected
	):
			ConfigParsingError(
				"unexpected character " + plainCharRep(whichChar) +
				" at " + std::to_string(line) + ':' + std::to_string(lineChar) +
				std::string(", expected ") + expected
			),
			expected_(expected),
			line_(line),
			lineChar_(lineChar),
			whichChar_(whichChar)
	{ }



	UnexpectedEof::UnexpectedEof(
			const std::string& expected
	):
			ConfigParsingError("unexpected end of file, expected " + expected),
			expected_(expected)
	{ }



	UnclosedGroup::UnclosedGroup(
			const Key& tos
	):
			ConfigParsingError("unclosed group (top of stack: `" + tos + "`)"),
			tos_(tos)
	{ }



	UnmatchedGroupClosure::UnmatchedGroupClosure(
			size_t line, size_t lineChar
	):
			ConfigParsingError(
				std::string("mismatched `") + GRAMMAR_GROUP_END + std::string("` at ") +
				std::to_string(line) + ':' + std::to_string(lineChar) ),
			line_(line), lineChar_(lineChar)
	{ }



	Key::Key(std::string str) {
		{
			size_t err = findKeyError(str);
			if(err < str.size()) throw InvalidKey(str, err);
		}
		std::string::operator=(std::move(str));
	}


	Key::Key(std::initializer_list<const char*> initLs) {
		reserve(2 * initLs.size()); // Allocate the (almost) minimum final size
		{
			auto iter = initLs.begin();
			auto end = initLs.end();
			if(initLs.size() > 0) {
				this->append(*(iter++)); }
			while(iter != end) {
				this->push_back(GRAMMAR_KEY_SEPARATOR);
				this->append(*(iter++));
			}
		} {
			size_t err = findKeyError(*this);
			if(err < this->size()) throw InvalidKey(*this, err);
		}
	}


	Key::Key(const KeySpan& keySpan):
			std::string(keySpan.data(), keySpan.size())
	{
		#ifndef NDEBUG
			if(! empty()) {
				size_t err = findKeyError(*this);
				assert(err == size());
			}
		#endif
	}


	Key Key::ancestor(size_t offset) const {
		size_t cursor = size()-1;
		{
			size_t separators = 0;
			if(empty()) return Key();
			while((separators > offset) && (cursor > 0)) {
				if((*this)[cursor] == GRAMMAR_KEY_SEPARATOR) {
					++separators;
				}
				--cursor;
			}
		} {
			return std::string(begin(), begin() + cursor + 1);
		}
	}

	std::string Key::basename() const {
		size_t cursor = size()-1;
		if(empty()) return "";
		while(((*this)[cursor] != GRAMMAR_KEY_SEPARATOR) && (cursor > 0)) {
			--cursor;
		}
		if(cursor > 0) ++cursor;
		return substr(cursor);
	}



	KeySpan::KeySpan(): depth(1) { }

	KeySpan::KeySpan(const Key& key):
			KeySpan(key.data(), key.size())
	{ }

	KeySpan::KeySpan(const Key& key, size_t end):
			KeySpan(key.data(), end)
	{
		assert(end <= key.size());
	}

	KeySpan::KeySpan(const char* data, size_t size):
			std::span<const char>(data, size),
			depth(0)
	{
		assert(yacfg::isKeyValid(std::string(data, size)));
		if(size != 0) {
			depth = 1;
			for(char c : *this) {
				if(c == GRAMMAR_KEY_SEPARATOR) {
					++ depth;
				}
			}
		}
	}

	bool KeySpan::operator<(KeySpan r) const noexcept {
		if(size() == r.size()) {
			return 0 > strncmp(data(), r.data(), size());
		} else {
			auto cmp = strncmp(data(), r.data(), std::min(size(), r.size()));
			if(cmp == 0) {
				return size() < r.size();
			} else {
				return 0 > cmp;
			}
		}
	}

	bool KeySpan::operator==(KeySpan r) const noexcept {
		if(size() == r.size()) {
			return 0 == strncmp(data(), r.data(), size());
		} else {
			return false;
		}
	}



	RawData::RawData(const char* cStr):
			type(DataType::eString)
	{
		data.stringValue.size = std::strlen(cStr);
		data.stringValue.ptr = (char*) ::operator new[](data.stringValue.size);
		std::strncpy(data.stringValue.ptr, cStr, data.stringValue.size);
	}

	RawData::RawData(const std::string& str):
			type(DataType::eString)
	{
		data.stringValue.size = str.size();
		data.stringValue.ptr = (char*) ::operator new[](data.stringValue.size);
		std::strncpy(data.stringValue.ptr, str.data(), data.stringValue.size);
	}

	RawData RawData::allocString(size_t len) {
		RawData r;
		r.type = DataType::eString;
		r.data.stringValue.size = len;
		r.data.stringValue.ptr = new char[len];
		return r;
	}

	RawData RawData::allocArray(size_t len) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size = len;
		r.data.arrayValue.ptr = new RawData[len];
		return r;
	}

	RawData RawData::copyArray(const RawData* cpPtr, size_t size) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size = size;
		r.data.arrayValue.ptr = new RawData[size];
		for(size_t i=0; i < size; ++i) {
			r.data.arrayValue.ptr[i] = cpPtr[i];
		}
		return r;
	}

	RawData RawData::moveArray(RawData* mvPtr, size_t size) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size = size;
		r.data.arrayValue.ptr = new RawData[size];
		for(size_t i=0; i < size; ++i) {
			r.data.arrayValue.ptr[i] = std::move(mvPtr[i]);
		}
		return r;
	}


	RawData::RawData(const RawData& cp):
			type(cp.type),
			data(cp.data)
	{
		if(type == DataType::eString) {
			data.stringValue.ptr = (char*) ::operator new[](data.stringValue.size);
			std::strncpy(data.stringValue.ptr, cp.data.stringValue.ptr, data.stringValue.size);
		}
		else
		if(type == DataType::eArray) {
			data.arrayValue.ptr = new RawData[data.arrayValue.size];
			for(size_t i=0; i < data.arrayValue.size; ++i) {
				data.arrayValue.ptr[i] = cp.data.arrayValue.ptr[i];
			}
		}
	}

	RawData& RawData::operator=(const RawData& cp) {
		this->~RawData();
		return *new (this) RawData(cp);
	}


	RawData::RawData(RawData&& mv):
			type(mv.type),
			data(mv.data)
	{
		mv.type = DataType::eNull;
		#ifndef NDEBUG
			if(
				mv.type == DataType::eString ||
				mv.type == DataType::eArray
			) {
				mv.data.arrayValue.ptr = nullptr;
			}
		#endif
	}

	RawData& RawData::operator=(RawData&& mv) {
		this->~RawData();
		return *new (this) RawData(std::move(mv));
	}


	RawData::~RawData() {
		if(type == DataType::eString) {
			assert(data.stringValue.ptr != nullptr);
			delete[] data.stringValue.ptr;
			#ifndef NDEBUG
				data.stringValue.ptr = nullptr;
			#endif
		} if(type == DataType::eArray) {
			assert(data.arrayValue.ptr != nullptr);
			delete[] data.arrayValue.ptr;
			#ifndef NDEBUG
				data.arrayValue.ptr = nullptr;
			#endif
		}
	}



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
					const yacfg::RawData& value = *found.value();
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

}
