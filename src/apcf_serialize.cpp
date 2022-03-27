#include "apcf_.hpp"

#include <cmath>
#include <limits>



namespace {

	void sortEntries(
			const apcf::ConfigHierarchy& hierarchy,
			const std::map<apcf::Key, apcf::RawData>& map,
			const std::set<apcf::Key>& parenthood,
			std::set<apcf::Key>& groupsDst,
			std::set<apcf::Key>& arraysDst,
			std::set<apcf::Key>& singleEntriesDst
	) {
		for(const auto& childKey : parenthood) {
			auto autocompKey = hierarchy.autocomplete(childKey);
			auto child = map.find(autocompKey);
			if(child == map.end()) {
				groupsDst.insert(autocompKey);
			} else {
				switch(child->second.type) {
					case apcf::DataType::eArray: arraysDst.insert(autocompKey); break;
					default: singleEntriesDst.insert(autocompKey); break;
				}
			}
		}
		assert(parenthood.empty() == (groupsDst.empty() && arraysDst.empty() && singleEntriesDst.empty()));
	}

}



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
			const apcf::RawArray& data, std::string& dst
	) {
		using Rules = apcf::SerializationRules;
		dst.push_back(GRAMMAR_ARRAY_BEGIN);
		if(! (rules.flags & Rules::eCompact)) {
			if(rules.flags & Rules::eCompactArrays) {
				dst.push_back(' ');
				if(data.size() > 0) {
					dst.append(data.data()->serialize(rules, state.indentationLevel)); }
				for(size_t i=1; i < data.size(); ++i) {
					dst.push_back(' ');
					dst.append(data[i].serialize(rules, state.indentationLevel));
				}
				dst.push_back(' ');
			} else {
				if(data.size()) {
					pushIndent(rules, state);
					for(size_t i=0; i < data.size(); ++i) {
						dst.push_back(GRAMMAR_NEWLINE);
						dst.append(state.indentation);
						dst.append(data[i].serialize(rules, state.indentationLevel));
					}
					popIndent(rules, state);
					dst.push_back(GRAMMAR_NEWLINE);
						dst.append(state.indentation);
				} else {
					dst.push_back(' ');
				}
			}
		} else {
			if(data.size() > 0) {
				dst.append(data.data()->serialize(rules, state.indentationLevel)); }
			for(size_t i=1; i < data.size(); ++i) {
				if(
					data[i-1].type != apcf::DataType::eArray &&
					data[i].type != apcf::DataType::eArray
				) {
					dst.push_back(' ');
				}
				dst.append(data[i].serialize(rules, state.indentationLevel));
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
					+ rawData.data.stringValue.length() /* The length of the string itself */
					+ 4 /* Four literal double-quote characters */ );
				r = GRAMMAR_STRING_DELIM;
				auto end = rawData.data.stringValue.data() + rawData.data.stringValue.length();
				for(auto iter = rawData.data.stringValue.data(); iter < end; ++iter) {
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
		bool thisLineIsArray = entryValue.type == apcf::DataType::eArray;
		bool doSpaceArray =
			thisLineIsArray &&
			! (
				(entryValue.data.arrayValue.size() == 0) ||
				(sd.rules.flags & apcf::SerializationRules::eCompactArrays)
			);
		constexpr auto writeValue = [](SerializeData& sd, const apcf::RawData& value) {
			std::string serializedValue = value.serialize(
				sd.rules,
				sd.state.indentationLevel );
			sd.dst.writeChars(serializedValue);
		};
		assert(apcf::isKeyValid(key));
		if(! (sd.rules.flags & apcf::SerializationRules::eCompact)) {
			if(
				getFlags<uint_fast8_t>(sd.lastLineFlags,
					lineFlagsArrayEndBit | lineFlagsGroupEndBit | lineFlagsGroupEntryBit
				) ||
				doSpaceArray
			) {
				sd.dst.writeChar('\n');
			}
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChars(key);
			sd.dst.writeChars(" = "s);
			writeValue(sd, entryValue);
			sd.dst.writeChar('\n');
		} else {
			if(
				getFlags<uint_fast8_t>(sd.lastLineFlags, lineFlagsOwnEntryBit) &&
				! getFlags<uint_fast8_t>(sd.lastLineFlags, lineFlagsArrayEndBit)
			) {
				sd.dst.writeChar(' ');
			}
			sd.dst.writeChars(key);
			sd.dst.writeChar('=');
			writeValue(sd, entryValue);
		}
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, false, lineFlagsGroupEndBit);
		// sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, false, lineFlagsGroupEntryBit); // Do NOT reset this bit, its meaning is tied to the next line (and will be reset then)
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, true, lineFlagsOwnEntryBit);
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, thisLineIsArray, lineFlagsArrayEndBit);
	}


	void serializeLineGroupBeg(
			SerializeData& sd,
			const apcf::Key& key
	) {
		if(! (sd.rules.flags & apcf::SerializationRules::eCompact)) {
			if(
				getFlags<uint_fast8_t>(sd.lastLineFlags,
					lineFlagsGroupEndBit | lineFlagsArrayEndBit | lineFlagsOwnEntryBit
				) &&
				! getFlags<uint_fast8_t>(sd.lastLineFlags, lineFlagsOwnEntryBit)
			) {
				sd.dst.writeChar('\n');
			}
			sd.dst.writeChars(sd.state.indentation);
			pushIndent(sd.rules, sd.state);
			sd.dst.writeChars(key);
			sd.dst.writeChar(' ');
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
			sd.dst.writeChar('\n');
		} else {
			if(
				getFlags<uint_fast8_t>(sd.lastLineFlags, lineFlagsOwnEntryBit) &&
				! getFlags<uint_fast8_t>(sd.lastLineFlags, lineFlagsArrayEndBit)
			) {
				sd.dst.writeChar(' ');
			}
			sd.dst.writeChars(key);
			sd.dst.writeChar(GRAMMAR_GROUP_BEGIN);
		}
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, false,
			lineFlagsGroupEndBit | lineFlagsArrayEndBit |
			lineFlagsOwnEntryBit | lineFlagsGroupEntryBit );
	}


	void serializeLineGroupEnd(
			SerializeData& sd
	) {
		if(! (sd.rules.flags & apcf::SerializationRules::eCompact)) {
			popIndent(sd.rules, sd.state);
			sd.dst.writeChars(sd.state.indentation);
			sd.dst.writeChar(GRAMMAR_GROUP_END);
			sd.dst.writeChar('\n');
		} else {
			sd.dst.writeChar(GRAMMAR_GROUP_END);
		}
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, true, lineFlagsGroupEndBit);
		sd.lastLineFlags = setFlags<uint_fast8_t>(sd.lastLineFlags, false,
			lineFlagsArrayEndBit | lineFlagsOwnEntryBit | lineFlagsGroupEntryBit );
	}


	void serializeHierarchy(
			SerializeHierarchyParams& state,
			const Key& key, const Key& parent
	) {
		// Calculate the basename of `key` by using the known parent size
		Key keyBasename;
		KeySpan keyBasenameSpan;
		bool keyIsRoot = key.empty();
		const auto& parenthood = state.hierarchy->getSubkeys(key);
		{
			size_t parentOffset = 0;
			if(! parent.empty()) parentOffset = parent.size() + 1;
			keyBasenameSpan = KeySpan(
				key.data() + parentOffset,
				key.size() - parentOffset );
			keyBasename = Key(keyBasenameSpan);
		}

		if(! keyIsRoot) {
			// Serialize entry, if one exists
			const auto& got = state.map->find(key);
			if(got != state.map->end()) {
				state.sd->lastLineFlags = setFlags<uint_fast8_t>(state.sd->lastLineFlags, ! parenthood.empty(), lineFlagsGroupEntryBit);
				serializeLineEntry(*state.sd, keyBasename, got->second);
			}
		} else {
			assert(false); // See comment on next `assert(false)`
		}

		// Serialize group, if one exists
		{
			if(! parenthood.empty()) {
				if(keyIsRoot) {
					assert(false); // This branch makes no sense, but I may have had a reason for writing this...? An empty key isn't allowed to be in a hierarchy, not anymore.
					for(const auto& child : parenthood) {
						serializeHierarchy(state, state.hierarchy->autocomplete(child), key);
					}
				} else {
					#define SERIALIZE_(SET_) { for(const auto& childKey : SET_) serializeHierarchy(state, childKey, std::move(key)); }
					if(! (state.sd->rules.flags & apcf::SerializationRules::eCompact)) {
						std::set<Key> groups;
						std::set<Key> arrays;
						std::set<Key> singleEntries;
						sortEntries(
							*state.hierarchy, *state.map, parenthood,
							groups, arrays, singleEntries );

						serializeLineGroupBeg(*state.sd, keyBasename);
						SERIALIZE_(groups)
						SERIALIZE_(arrays)
						SERIALIZE_(singleEntries)
						serializeLineGroupEnd(*state.sd);
					} else {
						serializeLineGroupBeg(*state.sd, keyBasename);
						for(const auto& childKey : parenthood) {
							serializeHierarchy(state, state.hierarchy->autocomplete(childKey), std::move(key));
						}
						serializeLineGroupEnd(*state.sd);
					}
					#undef SERIALIZE_
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
			const apcf::ConfigHierarchy* hierarchyPtr;

			if(sd.rules.hierarchy == nullptr) {
				hierarchy = apcf::ConfigHierarchy(map);
				hierarchyPtr = &hierarchy;
			} else {
				hierarchyPtr = sd.rules.hierarchy;
			}

			SerializeHierarchyParams saParams = {
				.sd = &sd,
				.map = &map,
				.hierarchy = hierarchyPtr };

			const std::set<Key>& subkeys = hierarchyPtr->getSubkeys({ });

			#define SERIALIZE_(SET_) { for(const auto& rootChild : SET_) serializeHierarchy(saParams, rootChild, { }); }
			if(! (sd.rules.flags & Rules::eCompact)) {
				std::set<Key> groups;
				std::set<Key> arrays;
				std::set<Key> singleEntries;
				sortEntries(*hierarchyPtr, map, subkeys, groups, arrays, singleEntries);
				SERIALIZE_(groups)
				SERIALIZE_(arrays)
				SERIALIZE_(singleEntries)
			} else {
				for(const auto& rootChild : subkeys) {
					serializeHierarchy(saParams, hierarchyPtr->autocomplete(rootChild), { });
				}
			}
			#undef SERIALIZE_
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
		auto wr = io::StringWriter(&r, 0);
		SerializeData serializeData = {
			.dst = wr,
			.rules = sr,
			.state = state,
			.lastLineFlags = 0 };

		apcf_serialize::serialize(serializeData, data_);

		return r;
	}

	void Config::write(io::Writer& out, SerializationRules sr) const {
		SerializationState state = { };
		SerializeData serializeData = {
			.dst = out,
			.rules = sr,
			.state = state,
			.lastLineFlags = 0 };
		apcf_serialize::serialize(serializeData, data_);
	}

	void Config::write(std::ostream& out, SerializationRules sr) const {
		SerializationState state = { };
		auto wr = io::StdStreamWriter(out);
		SerializeData serializeData = {
			.dst = wr,
			.rules = sr,
			.state = state,
			.lastLineFlags = 0 };
		apcf_serialize::serialize(serializeData, data_);
	}

}
