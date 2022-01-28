#include "apcf_.hpp"

#include <cassert>
#include <cstring>



namespace apcf {

	bool isKeyValid(const std::string& str) {
		assert(apcf_util::findKeyError(str) <= str.size());
		return apcf_util::findKeyError(str) == str.size();
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
			ConfigError("invalid value `" + valueRep + "`: " + reason),
			value_(std::move(valueRep)),
			type_(type)
	{ }



	UnexpectedChar::UnexpectedChar(
			size_t line, size_t lineChar, char whichChar, const std::string& expected
	):
			ConfigParsingError(
				"unexpected character " + apcf_util::plainCharRep(whichChar) +
				" at " + std::to_string(line+1) + ':' + std::to_string(lineChar+1) +
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
				std::to_string(line+1) + ':' + std::to_string(lineChar+1) ),
			line_(line), lineChar_(lineChar)
	{ }



	Key::Key(std::string str) {
		#ifndef NDEBUG
			/* Cargo-cult assertion to make sure that a empty string constructs an
			 * empty key, since constructor delegation isn't an option. */
			assert((! str.empty()) || Key() == str);
		#endif
		{
			size_t err = apcf_util::findKeyError(str);
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
			size_t err = apcf_util::findKeyError(*this);
			if(err < this->size()) throw InvalidKey(*this, err);
		}
	}


	Key::Key(const KeySpan& keySpan):
			std::string(keySpan.data(), keySpan.size())
	{
		#ifndef NDEBUG
			if(! empty()) {
				size_t err = apcf_util::findKeyError(*this);
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


	size_t Key::getDepth() const {
		size_t r = 1;
		for(char c : *this) {
			r += c == GRAMMAR_KEY_SEPARATOR;
		}
		return r;
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



	KeySpan::KeySpan(): depth_(1) { }

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
			depth_(1)
	{
		assert(apcf::isKeyValid(std::string(data, size)));
		for(char c : *this) {
			if(c == GRAMMAR_KEY_SEPARATOR) {
				++ depth_;
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
		data.stringValue.length_ = std::strlen(cStr);
		data.stringValue.ptr_ = (char*) ::operator new[](data.stringValue.length_);
		std::strncpy(data.stringValue.ptr_, cStr, data.stringValue.length_);
	}

	RawData::RawData(const std::string& str):
			type(DataType::eString)
	{
		data.stringValue.length_ = str.size();
		data.stringValue.ptr_ = (char*) ::operator new[](data.stringValue.length_);
		std::strncpy(data.stringValue.ptr_, str.data(), data.stringValue.length_);
	}

	RawData RawData::allocString(size_t len) {
		RawData r;
		r.type = DataType::eString;
		r.data.stringValue.length_ = len;
		r.data.stringValue.ptr_ = new char[len];
		return r;
	}

	RawData RawData::allocArray(size_t len) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size_ = len;
		r.data.arrayValue.ptr_ = new RawData[len];
		return r;
	}

	RawData RawData::copyArray(const RawData* cpPtr, size_t size) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size_ = size;
		r.data.arrayValue.ptr_ = new RawData[size];
		for(size_t i=0; i < size; ++i) {
			r.data.arrayValue.ptr_[i] = cpPtr[i];
		}
		return r;
	}

	RawData RawData::moveArray(RawData* mvPtr, size_t size) {
		RawData r;
		r.type = DataType::eArray;
		r.data.arrayValue.size_ = size;
		r.data.arrayValue.ptr_ = new RawData[size];
		for(size_t i=0; i < size; ++i) {
			r.data.arrayValue.ptr_[i] = std::move(mvPtr[i]);
		}
		return r;
	}

	RawData RawData::copyString(const char* cpPtr, size_t length) {
		RawData r;
		r.type = DataType::eString;
		r.data.stringValue.length_ = length;
		r.data.stringValue.ptr_ = new char[length];
		for(size_t i=0; i < length; ++i) {
			r.data.stringValue.ptr_[i] = cpPtr[i];
		}
		return r;
	}

	RawData RawData::moveString(char* mvPtr, size_t length) {
		RawData r;
		r.type = DataType::eString;
		r.data.stringValue.length_ = length;
		r.data.stringValue.ptr_ = std::move(mvPtr);
		return r;
	}


	RawData::RawData(const RawData& cp):
			type(cp.type),
			data(cp.data)
	{
		if(type == DataType::eString) {
			data.stringValue.ptr_ = (char*) ::operator new[](data.stringValue.length_);
			std::strncpy(data.stringValue.ptr_, cp.data.stringValue.ptr_, data.stringValue.length_);
		}
		else
		if(type == DataType::eArray) {
			data.arrayValue.ptr_ = new RawData[data.arrayValue.size_];
			for(size_t i=0; i < data.arrayValue.size_; ++i) {
				data.arrayValue.ptr_[i] = cp.data.arrayValue.ptr_[i];
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
				mv.data.arrayValue.ptr_ = nullptr;
			}
		#endif
	}

	RawData& RawData::operator=(RawData&& mv) {
		this->~RawData();
		return *new (this) RawData(std::move(mv));
	}


	RawData::~RawData() {
		if(type == DataType::eString) {
			assert(data.stringValue.ptr_ != nullptr);
			delete[] data.stringValue.ptr_;
			#ifndef NDEBUG
				data.stringValue.ptr_ = nullptr;
			#endif
		} if(type == DataType::eArray) {
			assert(data.arrayValue.ptr_ != nullptr);
			delete[] data.arrayValue.ptr_;
			#ifndef NDEBUG
				data.arrayValue.ptr_ = nullptr;
			#endif
		}
	}

}
