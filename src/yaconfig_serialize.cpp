#include <yaconfig.hpp>



namespace {
namespace yacfg_serialize {

	using namespace yacfg_util;


	class Writer {
	public:
		virtual void writeChar(char) = 0;
		virtual void writeChars(const char* begin, const char* end) = 0;
		virtual void writeChars(const std::string& str) = 0;
	};


	class StringWriter : public Writer {
	public:
		std::string* dst;
		size_t cursor;

		#ifdef NDEBUG
			StringWriter() { }
		#else
			StringWriter(): dst(nullptr), cursor(0) { }
		#endif

		StringWriter(std::string* dst, size_t begin):
				dst(dst), cursor(begin)
		{ }

		void writeChar(char c) override {
			assert(dst != nullptr);
			dst->push_back(c);
			++cursor;
		}

		void writeChars(
				const char* begin,
				const char* end
		) override {
			assert(dst != nullptr);
			size_t distance = std::distance(begin, end);
			dst->insert(dst->size(), begin, distance);
			cursor += distance;
		}

		void writeChars(
				const std::string& str
		) override {
			assert(dst != nullptr);
			dst->insert(dst->size(), str.data(), str.size());
			cursor += str.size();
		}
	};


	class FileWriter : public Writer {
	public:
		std::ostream* dst;

		#ifdef NDEBUG
			FileWriter() { }
		#else
			FileWriter(): dst(nullptr) { }
		#endif

		FileWriter(std::ostream& dst):
				dst(&dst)
		{ }

		void writeChar(char c) override {
			assert(dst != nullptr);
			dst->write(&c, 1);
		}

		void writeChars(
				const char* begin,
				const char* end
		) override {
			assert(dst != nullptr);
			size_t distance = std::distance(begin, end);
			dst->write(begin, distance);
		}

		void writeChars(
				const std::string& str
		) override {
			assert(dst != nullptr);
			dst->write(str.data(), str.size());
		}
	};


	struct SerializationState {
		std::string indentation;
		unsigned indentationLevel;
	};


	struct SerializeData {
		Writer& dst;
		yacfg::SerializationRules rules;
		SerializationState& state;
		bool lastLineWasEntry;
	};


	std::string mkIndent(yacfg::SerializationRules rules, size_t levels) {
		static constexpr char indentChar[] = { ' ', '\t' };
		std::string r;
		std::string singleLvl = [](size_t size, char c) {
			std::string r;
			for(size_t i=0; i < size; ++i) r.push_back(c);
			return r;
		} (
			rules.indentationSize,
			indentChar[(rules.flags & yacfg::SerializationRules::eIndentWithTabs) != 0]
		);
		r.reserve(levels * rules.indentationSize);
		for(size_t i = 0; i < levels; ++i) r.append(singleLvl);
		return r;
	}

	void pushIndent(yacfg::SerializationRules rules, SerializationState& state) {
		++state.indentationLevel;
		state.indentation.append(mkIndent(rules, 1));
	}

	void popIndent(yacfg::SerializationRules rules, SerializationState& state) {
		size_t newSize = (state.indentation.size() >= rules.indentationSize)?
			(state.indentation.size() - rules.indentationSize) :
			0;
		state.indentation.resize(newSize);
		--state.indentationLevel;
	}


	void serializeArray(
			yacfg::SerializationRules rules, SerializationState state,
			const yacfg::RawData::Data::Array& data, std::string& dst
	) {
		using Rules = yacfg::SerializationRules;
		dst.push_back(GRAMMAR_ARRAY_BEGIN);
		if(rules.flags & Rules::ePretty) {
			if(data.size) {
				pushIndent(rules, state);
				for(size_t i=0; i < data.size; ++i) {
					dst.push_back(GRAMMAR_NEWLINE);
					dst.append(state.indentation);
					dst.append(data.ptr[i].serialize(rules, state.indentationLevel));
				}
				popIndent(rules, state);
				dst.push_back(GRAMMAR_NEWLINE);
					dst.append(state.indentation);
			} else {
				dst.push_back(' ');
			}
		} else {
			if(data.size > 0) {
				dst.append(data.ptr->serialize(rules, state.indentationLevel)); }
			for(size_t i=1; i < data.size; ++i) {
				dst.push_back(' ');
				dst.append(data.ptr[i].serialize(rules, state.indentationLevel));
			}
		}
		dst.push_back(GRAMMAR_ARRAY_END);
	}


	std::string serializeDataRecursive(
			const yacfg::RawData& rawData,
			yacfg::SerializationRules rules,
			SerializationState state
	) {
		using namespace yacfg;
		std::string r;
		switch(rawData.type) {
			case DataType::eNull: { r = "null"; } break;
			case DataType::eBool: { r = rawData.data.boolValue? "true" : "false"; } break;
			case DataType::eInt: { r = num::serializeIntNumber(rawData.data.intValue); } break;
			case DataType::eFloat: { r = num::serializeFloatNumber(rawData.data.floatValue); } break;
			case DataType::eString: {
				r =
					GRAMMAR_STRING_DELIM +
					std::string(rawData.data.stringValue.ptr, rawData.data.stringValue.size) +
					GRAMMAR_STRING_DELIM;
			} break;
			case DataType::eArray: {
				serializeArray(rules, state, rawData.data.arrayValue, r);
			} break;
		}
		return r;
	}


	void serializeLineEntry(
			SerializeData& sd,
			const yacfg::Key& key,
			const yacfg::RawData& entryValue
	) {
		using namespace std::string_literals;
		constexpr auto writeValue = [](SerializeData& sd, const yacfg::RawData& value) {
			std::string serializedValue = value.serialize(
				sd.rules,
				sd.state.indentationLevel );
			sd.dst.writeChars(serializedValue);
		};
		assert(yacfg::isKeyValid(key));
		if(sd.rules.flags & yacfg::SerializationRules::ePretty) {
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChars(key);
			sd.dst.writeChars(" = "s);
			writeValue(sd, entryValue);
			sd.dst.writeChar('\n');
		} else {
			if(sd.lastLineWasEntry) {
				sd.dst.writeChar('\n');
			}
			sd.dst.writeChars(key);
			sd.dst.writeChar('=');
			writeValue(sd, entryValue);
		}
		sd.lastLineWasEntry = true;
	}


	void serializeLineGroupBeg(
			SerializeData& sd,
			const yacfg::Key& key
	) {
		if(sd.rules.flags & yacfg::SerializationRules::ePretty) {
			sd.dst.writeChars(sd.state.indentation);
			pushIndent(sd.rules, sd.state);
			sd.dst.writeChars(key);
			sd.dst.writeChar(' ');
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
			sd.dst.writeChar('\n');
		} else {
			sd.dst.writeChars(key);
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
		}
		sd.lastLineWasEntry = false;
	}


	void serializeLineGroupEnd(
			SerializeData& sd
	) {
		if(sd.rules.flags & yacfg::SerializationRules::ePretty) {
			popIndent(sd.rules, sd.state);
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChar('\n');
		} else {
			sd.dst.writeChar(GRAMMAR_GROUP_END);
		}
		sd.lastLineWasEntry = false;
	}


	void serialize(SerializeData& sd, const std::map<yacfg::Key, yacfg::RawData>& map) {
		using Rules = yacfg::SerializationRules;
		if(sd.rules.flags & Rules::eExpandKeys) {
			// Just write every entry in its own "line"
			for(const auto& entry : map) {
				serializeLineEntry(sd, entry.first, entry.second);
			}
		} else {
			// Some MacGyver shit is needed here, idk
			throw yacfg::ConfigParsingError("Serialization without the flag `eExpandKeys` is not yet implemented");
		}
	}

}
}



namespace yacfg {

	using namespace yacfg_serialize;


	std::string RawData::serialize(SerializationRules rules, unsigned indentation) const {
		return serializeDataRecursive(
			*this, rules,
			{
				.indentation = mkIndent(rules, indentation),
				.indentationLevel = indentation
			} );
	}


	std::string Config::serialize(SerializationRules sr) const {
		std::string r;
		SerializationState state = { };
		auto wr = StringWriter(&r, 0);
		SerializeData serializeData = {
			.dst = wr,
			.rules = sr,
			.state = state,
			.lastLineWasEntry = false };

		yacfg_serialize::serialize(serializeData, data_);

		return r;
	}

	void Config::write(OutputStream& out, SerializationRules sr) const {
		SerializationState state = { };
		auto wr = FileWriter(out);
		SerializeData serializeData = {
			.dst = wr,
			.rules = sr,
			.state = state,
			.lastLineWasEntry = false };

		yacfg_serialize::serialize(serializeData, data_);
	}

}
