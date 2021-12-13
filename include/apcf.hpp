#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <span>
#include <map>
#include <set>
#include <vector>
#include <optional>
#include <initializer_list>

#include <apcf_fwd.hpp>



namespace apcf::io {

	class Reader {
	protected:
		size_t lineCtr;
		size_t linePos;

	public:
		Reader(): lineCtr(0), linePos(0) { }
		virtual ~Reader() { }

		/** Returns `true` if the cursor is at (or past) the ond of
		 * the stream/file, `false` otherwise. */
		virtual bool isAtEof() const = 0;

		/** Returns the character at the cursor, or '\0' if the
		 * latter is past the end of the stream/file. */
		virtual char getChar() const = 0;

		/** Try and move the cursor forward:
		 * returns `true` if the operation succeeded, `false` if
		 * the cursor is already at the end of the stream/file. */
		virtual bool fwdOrEof() = 0;

		/** Return the 0-indexed line of the cursor. */
		size_t lineCounter() const { return lineCtr; }

		/** Return the 0-indexed line of the cursor. */
		size_t linePosition() const { return linePos; }
	};


	class Writer {
	public:
		virtual void writeChar(char) = 0;
		virtual void writeChars(const char* begin, const char* end) = 0;
		virtual void writeChars(const std::string& str) = 0;
	};

}



namespace apcf {

	enum class DataType { eNull, eBool, eInt, eFloat, eString, eArray };

	template<DataType type> std::string_view dataTypeString;
	#define MK_TYPE_STR_(ENUM_, STR_) template<> constexpr std::string_view dataTypeString<DataType::ENUM_> = std::string_view(STR_);
		MK_TYPE_STR_(eNull, "null")
		MK_TYPE_STR_(eBool, "bool")
		MK_TYPE_STR_(eInt, "int")
		MK_TYPE_STR_(eFloat, "float")
		MK_TYPE_STR_(eString, "string")
		MK_TYPE_STR_(eArray, "array")
	#undef MK_TYPE_STR_

	constexpr std::string_view dataTypeStringOf(DataType type) {
		#define MK_CASE_(ENUM_) case DataType::ENUM_: return dataTypeString<DataType::ENUM_>;
		switch(type) {
			MK_CASE_(eNull);
			MK_CASE_(eBool);
			MK_CASE_(eInt);
			MK_CASE_(eFloat);
			MK_CASE_(eString);
			MK_CASE_(eArray);
			default: return "?";
		}
	}


	using int_t = long long;
	using float_t = double;
	using string_t = std::string;


	/* Valid keys are sequences of alphanumeric characters,hyphens or underscores,
	 * separated by periods. */
	bool isKeyValid(const std::string&);


	class KeySpan : public std::span<const char> {
	private:
		friend Key;
		size_t depth_;

	public:
		KeySpan();
		explicit KeySpan(const Key&);
		KeySpan(const Key&, size_t end);
		KeySpan(const char* data, size_t size);

		bool operator<(KeySpan r) const noexcept;
		bool operator==(KeySpan r) const noexcept;
	};

	class Key : public std::string {
		friend KeySpan;

	public:
		Key(): std::string() { }
		Key(std::string);
		Key(std::initializer_list<const char*>);
		explicit Key(const KeySpan&);
		Key(const char* cStr): Key(std::string(cStr)) { }

		Key(const Key&) = default;
		Key(Key&&) = default;

		Key& operator=(const Key&) = default;
		Key& operator=(Key&&) = default;
		Key& operator=(const std::string&) = delete;
		Key& operator=(std::string&&) = delete;

		Key ancestor(size_t offset) const;
		Key parent() const { return ancestor(1); };

		size_t getDepth() const;

		std::string basename() const;

		const std::string& asString() const { return *this; }
	};


	struct SerializationRules {
		enum FlagBits : unsigned {
			eNull           = 0b00000,
			ePretty         = 0b00001,
			eExpandKeys     = 0b00010,
			eIndentWithTabs = 0b00100,
			eCompactArrays  = 0b01000,
			eFloatNoFail    = 0b10000
		};
		const ConfigHierarchy* hierarchy = nullptr;
		size_t indentationSize = 3;
		unsigned flags = 0;
	};


	struct RawData {
		DataType type;
		union Data {
			struct String {
				size_t size;
				char* ptr;

				char& operator[](size_t index) { return ptr[index]; }
				const char& operator[](size_t index) const { return ptr[index]; }
			} stringValue;
			struct Array {
				size_t size;
				RawData* ptr;

				RawData& operator[](size_t index) { return ptr[index]; }
				const RawData& operator[](size_t index) const { return ptr[index]; }
			} arrayValue;
			float_t floatValue;
			int_t intValue;
			bool boolValue;
		} data;

		RawData(): type(DataType::eNull) { }

		RawData(float_t value): type(DataType::eFloat) { data.floatValue = value; }
		RawData(int_t value): type(DataType::eInt) { data.intValue = value; }
		RawData(bool value): type(DataType::eBool) { data.boolValue = value; }

		RawData(const char* cStr);
		RawData(const string_t&);

		static RawData allocString(size_t length);
		static RawData allocArray(size_t size);

		static RawData copyArray(const RawData* valuesPtr, size_t n);
		static RawData moveArray(RawData* valuesPtr, size_t n);

		RawData(const RawData&);  RawData& operator=(const RawData&);
		RawData(RawData&&);  RawData& operator=(RawData&&);

		~RawData();

		operator bool() const { return type != DataType::eNull; };
		bool operator!() const { return type == DataType::eNull; };

		std::string serialize(SerializationRules = { }, unsigned indentation = 0) const;
	};


	using array_t = std::vector<RawData>;
	using array_span_t = std::span<const RawData>;



	class Config {
	private:
		std::map<Key, RawData> data_;

	public:
		static Config parse(const std::string& str) { return parse(std::string(str.data(), str.size())); }
		static Config parse(const char* cStr);
		static Config parse(const char* charSeqPtr, size_t length);
		static Config read(io::Reader&);
		static Config read(io::Reader&& tmp) { auto& tmpProxy = tmp; return read(tmpProxy); }
		static Config read(std::istream&);
		static Config read(std::istream&, size_t count);
		static Config read(std::istream&& tmp) { auto& tmpProxy = tmp; return read(tmpProxy); }

		std::string serialize(SerializationRules = { }) const;
		void write(io::Writer&, SerializationRules = { }) const;
		void write(io::Writer&& tmp, SerializationRules sr = { }) const { auto& tmpProxy = tmp; return write(tmpProxy, sr); }
		void write(std::ostream&, SerializationRules = { }) const;
		void write(std::ostream&& tmp, SerializationRules sr = { }) const { auto& tmpProxy = tmp; return write(tmpProxy, sr); }

		void merge(const Config&);
		void merge(Config&&);
		Config& operator<<(const Config& r) { merge(r); return *this; }
		Config& operator<<(Config&& r) { merge(std::move(r)); return *this; }
		Config& operator>>(Config& r) const { return r.operator<<(*this); }

		std::map<apcf::Key, apcf::RawData>::const_iterator begin() const;
		std::map<apcf::Key, apcf::RawData>::const_iterator end() const;

		size_t keyCount() const;

		/** Returns the Config's hierarchy, which can be used to implement logic
		 * that depends on the relationships between keys. */
		ConfigHierarchy getHierarchy() const;

		std::optional<const RawData*> get(const Key&) const;
		std::optional<bool>           getBool(const Key&) const;
		std::optional<int_t>          getInt(const Key&) const;
		std::optional<float_t>        getFloat(const Key&) const;
		std::optional<string_t>       getString(const Key&) const;
		std::optional<array_span_t>   getArray(const Key&) const;

		void set      (const Key&, RawData);
		void setBool  (const Key&, bool value);
		void setInt   (const Key&, int_t value);
		void setFloat (const Key&, float_t value);
		void setString(const Key&, string_t value);
		void setArray (const Key&, array_t value);
	};



	class ConfigError : public std::runtime_error {
	protected:
		using std::runtime_error::runtime_error;
	};


	class InvalidKey : public ConfigError {
	private:
		std::string key_;
		size_t pos_;

	public:
		InvalidKey(std::string key, size_t charPos);
		const std::string& key() const noexcept { return key_; }
		size_t invalidCharPosition() const noexcept { return pos_; }
	};


	class InvalidValue : public ConfigError {
	private:
		std::string value_;
		DataType type_;

	public:
		InvalidValue(const std::string& valueRep, DataType, const std::string& reason);
		const std::string& valueRep() const noexcept { return value_; }
		DataType dataType() const noexcept { return type_; }
	};


	class ConfigParsingError : public ConfigError {
	protected:
		using ConfigError::ConfigError;
	};


	class UnexpectedChar : public ConfigParsingError {
	private:
		std::string expected_;
		size_t line_;
		size_t lineChar_;
		char whichChar_;

	public:
		UnexpectedChar(size_t line, size_t lineChar, char whichChar, const std::string& expected);
		char unexpectedChar() const noexcept { return whichChar_; }
		const std::string& expected() const noexcept { return expected_; }
		size_t atLine() const noexcept { return line_; }
		size_t atLineChar() const noexcept { return lineChar_; }
	};


	class UnexpectedEof : public ConfigParsingError {
	private:
		std::string expected_;

	public:
		UnexpectedEof(const std::string& expected);
		const std::string& expected() const noexcept { return expected_; }
	};


	class UnclosedGroup : public ConfigParsingError {
	private:
		Key tos_;

	public:
		UnclosedGroup(const Key& topOfStack);
		const Key& topOfStack() const noexcept { return tos_; }
	};


	class UnmatchedGroupClosure : public ConfigParsingError {
	private:
		size_t line_;
		size_t lineChar_;

	public:
		UnmatchedGroupClosure(size_t line, size_t lineChar);
		size_t atLine() const noexcept { return line_; }
		size_t atLineChar() const noexcept { return lineChar_; }
	};

}
