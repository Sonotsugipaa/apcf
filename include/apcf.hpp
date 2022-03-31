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



namespace apcf {

	namespace io {

		class Reader {
		protected:
			size_t lineCtr;
			size_t linePos;

		public:
			Reader(): lineCtr(0), linePos(0) { }
			virtual ~Reader() { }

			/** Returns `true` if the cursor is at (or past) the end of
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


		class StringWriter : public apcf::io::Writer {
		public:
			std::string* dst;
			size_t cursor;

			StringWriter();
			StringWriter(std::string* dst, size_t begin);

			void writeChar(char c) override;
			void writeChars(const char* begin, const char* end) override;
			void writeChars(const std::string& str) override;
		};


		class StdStreamWriter : public apcf::io::Writer {
		public:
			std::ostream* dst;

			StdStreamWriter();
			StdStreamWriter(std::ostream& dst);

			void writeChar(char c) override;
			void writeChars(const char* begin, const char* end) override;
			void writeChars(const std::string& str) override;
		};


		class StringReader : public apcf::io::Reader {
		public:
			std::span<const char, std::dynamic_extent> str;
			size_t cursor;
			size_t limit;

			StringReader() = default;
			StringReader(std::span<const char, std::dynamic_extent> str);

			bool isAtEof() const override;
			char getChar() const override;
			bool fwdOrEof() override;
		};


		class StdStreamReader : public apcf::io::Reader {
		public:
			std::istream* str;
			size_t charsLeft;
			char current;

			StdStreamReader() = default;
			StdStreamReader(std::istream& str, size_t limit);
			StdStreamReader(std::istream& str);

			bool isAtEof() const override { return charsLeft <= 0; }
			char getChar() const override { return current; }

			bool fwdOrEof() override;
		};

	}



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

	std::string_view dataTypeStringOf(DataType);


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
		Key(const char* str, size_t len): Key(std::string(str, len)) { }

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
			eNull              = 0b000000,
			eExpandKeys        = 0b000010,
			eIndentWithTabs    = 0b000100,
			eForceInlineArrays = 0b001000,
			eFloatNoFail       = 0b010000,
			eMinimized         = 0b100000,
			ePretty        [[deprecated("Deprecated in favor of `eMinimized`"      " - has no effect")]]       = 0b000001,
			eCompactArrays [[deprecated("Deprecated in favor of `eForceInlineArrays` - has the same effect")]] = 0b001000,
			eCompact       [[deprecated("Deprecated in favor of `eMinimized`"      " - has the same effect")]] = 0b100000
		};
		const ConfigHierarchy* hierarchy = nullptr;
		size_t indentationSize = 3;
		size_t maxInlineArrayLength = 32;
		unsigned flags = 0;
	};


	struct RawData;

	class RawString {
		private:
			friend RawData;

			size_t length_;
			char* ptr_;

			RawString(char*, size_t);

		public:
			RawString() = default;

			size_t length() const { return length_; }

			operator string_t() const { return std::string(ptr_, length_); }

			inline       char& operator[](size_t index);
			inline const char& operator[](size_t index) const;
			inline       char* data();
			inline const char* data() const;
	};

	class RawArray {
		private:
			friend RawData;

			size_t size_;
			RawData* ptr_;

			RawArray(RawData*, size_t);

		public:
			RawArray() = default;

			size_t size() const { return size_; }

			inline       RawData& operator[](size_t index);
			inline const RawData& operator[](size_t index) const;
			inline       RawData* data();
			inline const RawData* data() const;
	};


	struct RawData {
		DataType type;
		union Data {
			RawString stringValue;
			RawArray arrayValue;
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

		static RawData copyString(const char* valuesPtr, size_t n);
		static RawData moveString(char* valuesPtr, size_t n);

		RawData(const RawData&);  RawData& operator=(const RawData&);
		RawData(RawData&&);  RawData& operator=(RawData&&);

		~RawData();

		operator bool() const { return type != DataType::eNull; };
		bool operator!() const { return type == DataType::eNull; };

		std::string serialize(SerializationRules = { }, unsigned indentation = 0) const;
	};


	inline char& RawString::operator[](size_t index) { return ptr_[index]; }
	inline const char& RawString::operator[](size_t index) const { return ptr_[index]; }
	inline char* RawString::data() { return ptr_; }
	inline const char* RawString::data() const { return ptr_; }

	inline RawData& RawArray::operator[](size_t index) { return ptr_[index]; }
	inline const RawData& RawArray::operator[](size_t index) const { return ptr_[index]; }
	inline RawData* RawArray::data() { return ptr_; }
	inline const RawData* RawArray::data() const { return ptr_; }


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

		/** Copy every entry of the given Config. */
		void merge(const Config&);

		/** Copy every entry of the given Config. */
		void merge(Config&&);

		/** Copy every entry of the given Config, with their keys prefixed
		 * by the given group name and an implicit separator. */
		void mergeAsGroup(const Key& groupName, const Config&);

		/** Copy every entry of the given Config, with their keys prefixed
		 * by the given group name and an implicit separator. */
		void mergeAsGroup(const Key& groupName, Config&&);

		Config& operator<<(const Config& r) { merge(r); return *this; }
		Config& operator<<(Config&& r) { merge(std::move(r)); return *this; }
		Config& operator>>(Config& r) const { return r.operator<<(*this); }

		std::map<apcf::Key, apcf::RawData>::const_iterator begin() const;
		std::map<apcf::Key, apcf::RawData>::const_iterator end() const;

		size_t entryCount() const;

		[[deprecated("Use `entryCount` instead")]]
		size_t keyCount() const { return entryCount(); }

		/** Returns the Config's hierarchy, which can be used to implement logic
		 * that depends on the relationships between keys. */
		ConfigHierarchy getHierarchy() const;

		/** Filters each key with the given group name,
		 * removing the latter from the former.
		 * The group name is followed by an implicit separator. */
		Config getSubconfig(const Key& group) const;

		std::optional<const RawData*> get(const Key&) const noexcept;
		std::optional<bool>           getBool(const Key&) const;
		std::optional<int_t>          getInt(const Key&) const;
		std::optional<float_t>        getFloat(const Key&) const;
		std::optional<string_t>       getString(const Key&) const;
		std::optional<array_span_t>   getArray(const Key&) const;

		void set      (Key, RawData) noexcept;
		void setBool  (Key, bool value) noexcept;
		void setInt   (Key, int_t value) noexcept;
		void setFloat (Key, float_t value) noexcept;
		void setString(Key, string_t value) noexcept;
		void setArray (Key, array_t value) noexcept;
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


	class ConfigSerializationError : public ConfigError {
	protected:
		using ConfigError::ConfigError;
	};

}
