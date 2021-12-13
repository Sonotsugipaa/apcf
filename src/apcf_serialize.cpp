#include "apcf_.hpp"

#include <cmath>
#include <limits>



namespace apcf_serialize {

	using namespace apcf_util;

	using apcf::Key;
	using apcf::KeySpan;


	std::string mkIndent(apcf::SerializationRules rules, size_t levels) {
		static constexpr char indentChar[] = { ' ', '\t' };
		std::string r;
		std::string singleLvl = [](size_t size, char c) {
			std::string r;
			for(size_t i=0; i < size; ++i) r.push_back(c);
			return r;
		} (
			rules.indentationSize,
			indentChar[(rules.flags & apcf::SerializationRules::eIndentWithTabs) != 0]
		);
		r.reserve(levels * rules.indentationSize);
		for(size_t i = 0; i < levels; ++i) r.append(singleLvl);
		return r;
	}

	void pushIndent(apcf::SerializationRules rules, SerializationState& state) {
		++state.indentationLevel;
		state.indentation.append(mkIndent(rules, 1));
	}

	void popIndent(apcf::SerializationRules rules, SerializationState& state) {
		size_t newSize = (state.indentation.size() >= rules.indentationSize)?
			(state.indentation.size() - rules.indentationSize) :
			0;
		state.indentation.resize(newSize);
		--state.indentationLevel;
	}


	void serializeArray(
			apcf::SerializationRules rules, SerializationState state,
			const apcf::RawData::Data::Array& data, std::string& dst
	) {
		using Rules = apcf::SerializationRules;
		dst.push_back(GRAMMAR_ARRAY_BEGIN);
		if(rules.flags & Rules::ePretty) {
			if(rules.flags & Rules::eCompactArrays) {
				dst.push_back(' ');
				if(data.size > 0) {
					dst.append(data.ptr->serialize(rules, state.indentationLevel)); }
				for(size_t i=1; i < data.size; ++i) {
					dst.push_back(' ');
					dst.append(data.ptr[i].serialize(rules, state.indentationLevel));
				}
				dst.push_back(' ');
			} else {
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
			const apcf::RawData& rawData,
			apcf::SerializationRules rules,
			SerializationState state
	) {
		using namespace apcf;
		std::string r;
		switch(rawData.type) {
			case DataType::eNull: { r = "null"; } break;
			case DataType::eBool: { r = rawData.data.boolValue? "true" : "false"; } break;
			case DataType::eInt: { r = apcf_num::serializeIntNumber(rawData.data.intValue); } break;
			case DataType::eFloat: {
				if(std::isfinite(rawData.data.floatValue)) [[likely]] {
					r = apcf_num::serializeFloatNumber(rawData.data.floatValue);
				} else {
					if(rules.flags & apcf::SerializationRules::eFloatNoFail) {
						if(std::isinf(rawData.data.floatValue)) {
							r = apcf_num::serializeFloatNumber(
								std::numeric_limits<apcf::float_t>::max() *
								((rawData.data.floatValue > 0)? 1.0f : -1.0f) );
						} else {
							r = apcf_num::serializeFloatNumber(0.0f);
						}
					} else {
						if(std::isinf(rawData.data.floatValue)) {
							if(rawData.data.floatValue > 0) r = "+infinity";
							else r = "-infinity";
						} else {
							r = "NaN";
						}
						throw InvalidValue(r, DataType::eFloat, "non-finite numbers cannot be serialized");
					}
				}
			} break;
			case DataType::eString: {
				r.reserve(
					+ rawData.data.stringValue.size /* The size of the string itself */
					+ 4 /* Four literal double-quote characters */ );
				r = GRAMMAR_STRING_DELIM;
				auto end = rawData.data.stringValue.ptr + rawData.data.stringValue.size;
				for(auto iter = rawData.data.stringValue.ptr; iter < end; ++iter) {
					char put = *iter;
					if(put == GRAMMAR_STRING_DELIM) r.push_back(GRAMMAR_STRING_ESCAPE);
					r.push_back(put);
				}
				r.push_back(GRAMMAR_STRING_DELIM);
			} break;
			case DataType::eArray: {
				serializeArray(rules, state, rawData.data.arrayValue, r);
			} break;
		}
		return r;
	}


	void serializeLineEntry(
			SerializeData& sd,
			const apcf::Key& key,
			const apcf::RawData& entryValue
	) {
		using namespace std::string_literals;
		constexpr auto writeValue = [](SerializeData& sd, const apcf::RawData& value) {
			std::string serializedValue = value.serialize(
				sd.rules,
				sd.state.indentationLevel );
			sd.dst.writeChars(serializedValue);
		};
		assert(apcf::isKeyValid(key));
		if(sd.rules.flags & apcf::SerializationRules::ePretty) {
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChars(key);
			sd.dst.writeChars(" = "s);
			writeValue(sd, entryValue);
			sd.dst.writeChar('\n');
		} else {
			if(sd.lastLineWasEntry) {
				sd.dst.writeChar(' ');
			}
			sd.dst.writeChars(key);
			sd.dst.writeChar('=');
			writeValue(sd, entryValue);
		}
		sd.lastLineWasEntry = true;
	}


	void serializeLineGroupBeg(
			SerializeData& sd,
			const apcf::Key& key
	) {
		if(sd.rules.flags & apcf::SerializationRules::ePretty) {
			sd.dst.writeChars(sd.state.indentation);
			pushIndent(sd.rules, sd.state);
			sd.dst.writeChars(key);
			sd.dst.writeChar(' ');
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
			sd.dst.writeChar('\n');
		} else {
			if(sd.lastLineWasEntry) {
				sd.dst.writeChar(' ');
			}
			sd.dst.writeChars(key);
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
		}
		sd.lastLineWasEntry = false;
	}


	void serializeLineGroupEnd(
			SerializeData& sd
	) {
		if(sd.rules.flags & apcf::SerializationRules::ePretty) {
			popIndent(sd.rules, sd.state);
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChar(GRAMMAR_GROUP_END);
			sd.dst.writeChar('\n');
		} else {
			sd.dst.writeChar(GRAMMAR_GROUP_END);
		}
		sd.lastLineWasEntry = false;
	}


	void serializeHierarchy(
			SerializeHierarchyParams& state,
			const Key& key, const Key& parent
	) {
		// Calculate the basename of `key` by using the known parent size
		Key keyBasename;
		KeySpan keyBasenameSpan;
		{
			size_t parentOffset = 0;
			if(! parent.empty()) parentOffset = parent.size() + 1;
			keyBasenameSpan = KeySpan(
				key.data() + parentOffset,
				key.size() - parentOffset );
			keyBasename = Key(keyBasenameSpan);
		}

		// Serialize entry, if one exists
		if(! key.empty()) {
			const auto& got = state.map->find(key);
			if(got != state.map->end()) {
				serializeLineEntry(*state.sd, keyBasename, got->second);
			}
		}

		// Serialize group, if one exists
		{
			const auto& parenthood = state.hierarchy->getSubkeys(Key(key));
			if(! parenthood.empty()) {
				if(key.empty()) {
					for(const auto& child : parenthood) {
						serializeHierarchy(state, Key(child), key);
					}
				} else {
					serializeLineGroupBeg(*state.sd, keyBasename);
					for(const auto& child : parenthood) {
						serializeHierarchy(state, Key(child), key);
					}
					serializeLineGroupEnd(*state.sd);
				}
			}
		}
	}


	void serialize(SerializeData& sd, const std::map<apcf::Key, apcf::RawData>& map) {
		using Rules = apcf::SerializationRules;
		if(sd.rules.flags & Rules::eExpandKeys) {
			for(const auto& entry : map) {
				serializeLineEntry(sd, entry.first, entry.second);
			}
		} else {
			apcf::ConfigHierarchy hierarchy;
			if(sd.rules.hierarchy == nullptr) {
				hierarchy = apcf::ConfigHierarchy(map);
			} else {
				hierarchy = *sd.rules.hierarchy;
			}
			SerializeHierarchyParams saParams = {
				.sd = &sd,
				.map = &map,
				.hierarchy = &hierarchy };
			hierarchy.collapse();
			for(const auto& rootChild : hierarchy.getSubkeys({ })) {
				serializeHierarchy(saParams, Key(rootChild), "");
			}
		}
	}

}



namespace apcf {

	using namespace apcf_serialize;


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

		apcf_serialize::serialize(serializeData, data_);

		return r;
	}

	void Config::write(io::Writer& out, SerializationRules sr) const {
		SerializationState state = { };
		SerializeData serializeData = {
			.dst = out,
			.rules = sr,
			.state = state,
			.lastLineWasEntry = false };
		apcf_serialize::serialize(serializeData, data_);
	}

	void Config::write(std::ostream& out, SerializationRules sr) const {
		SerializationState state = { };
		auto wr = StdStreamWriter(out);
		SerializeData serializeData = {
			.dst = wr,
			.rules = sr,
			.state = state,
			.lastLineWasEntry = false };
		apcf_serialize::serialize(serializeData, data_);
	}

}
