#include <test_tools.hpp>

#include <apcf.hpp>
#include <apcf_hierarchy.hpp>
#include <apcf_templates.hpp>

#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <set>



namespace {

	using apcf::Config;
	using apcf::Key;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	using TestFn = std::function<utest::ResultType (std::ostream&)>;


	constexpr const char* genericConfigSrc =
		"1 = 1\n"
		"1.1 = 1.5\n"
		"1.2 = 0.925\n"
		"rootvalue-int = 1\n"
		"rootvalue-int.negative = -1\n"
		"rootvalue-int.positive = +1\n"
		"rootvalue-float=1.5\n"
		"rootvalue-string = \"str \\\"literal\\\"\"\n"
		"rootvalue-array-one = [\n"
		"  1  2.25  [  \"3\" ][\"4\" \"5\"  ]\n"
		"]\n"
		"rootvalue-array-two = [ ]\n"
		"rootvalue-array-three = [ ]\n"
		"group1 {\n"
		"  value1 = 1\n"
		"  value2 = 2\n"
		"  group2.value1= 3\n"
		"  group2.value2 =4\n"
		"  group3 {\n"
		"    value1=5"
		"  }\n"
		"}\n"
		"long.single.line.group.1 {\n"
		"  value1 = 6\n"
		"  value2 = 7\n"
		"  2 { value1=8 value2=9 }\n"
		"}\n";

	constexpr const char* tmpFileBase = "test/";


	auto wrapExceptions(TestFn fn) {
		return [fn](std::ostream& cerr) -> utest::ResultType {
			using namespace apcf;
			try {
				return std::move(fn)(cerr);
			} catch(InvalidKey& err) {
				cerr << "Caught apcf::InvalidKey: " << err.what() << std::endl;
			} catch(InvalidValue& err) {
				cerr << "Caught apcf::InvalidValue: " << err.what() << std::endl;
			} catch(UnexpectedChar& err) {
				cerr << "Caught apcf::UnexpectedChar: " << err.what() << std::endl;
			} catch(UnexpectedEof& err) {
				cerr << "Caught apcf::UnexpectedEof: " << err.what() << std::endl;
			}
			return eFailure;
		};
	}


	template<typename T>
	bool checkValue(
			const Config& cfg, std::ostream& out,
			const Key& key, T expect
	) {
		auto got = getCfgValue<T>(cfg, key);
		if(! got.has_value()) {
			out
				<< "Expected entry `" << key << '='
				<< expect << "`, found none" << std::endl;
			return false;
		}
		if(got.value() != expect) {
			out
				<< "Expected entry `" << key << '=' << expect
				<< "`, found value " << got.value() << std::endl;
			return false;
		}
		return true;
	};


	utest::ResultType testVirtualRdWr(std::ostream&) {
		struct Reader : public apcf::io::Reader {
			bool isAtEof() const { return true; }
			char getChar() const { return '\0'; }
			bool fwdOrEof() { return false; }
		};
		struct Writer : public apcf::io::Writer {
			void writeChar(char) { }
			void writeChars(const char*, const char*) { }
			void writeChars(const std::string&) { }
		};
		auto cfg = Config::read(Reader());
		cfg.write(Writer());
		return eSuccess;
	}


	utest::ResultType testSetGetBool(std::ostream& out) {
		Config cfg;
		cfg.setBool("key.subkey.bool", true);
		return checkValue<bool>(cfg, out, "key.subkey.bool", true)? eSuccess : eFailure;
	}

	utest::ResultType testSetGetInt(std::ostream& out) {
		Config cfg;
		cfg.setInt("key.subkey.int", 7);
		return checkValue<apcf::int_t>(cfg, out, "key.subkey.int", 7)? eSuccess : eFailure;
	}

	utest::ResultType testSetGetFloat(std::ostream& out) {
		Config cfg;
		cfg.setFloat("key.subkey.float", 7.2);
		return checkValue<apcf::float_t>(cfg, out, "key.subkey.float", 7.2)? eSuccess : eFailure;
	}

	utest::ResultType testSetGetString(std::ostream& out) {
		Config cfg;
		const std::string testString = "strValue";
		cfg.setString("key.subkey.string", testString);
		return checkValue<apcf::string_t>(cfg, out, "key.subkey.string", testString)? eSuccess : eFailure;
	}

	utest::ResultType testSetGetArray(std::ostream& out) {
		Config cfg;
		const apcf::array_t testArray = { apcf::int_t(3), apcf::int_t(5) };
		cfg.setArray("key.subkey.array", testArray);

		auto got = getCfgValue<apcf::array_span_t>(cfg, "key.subkey.array");
		if(! got.has_value()) {
			out << "Expected entry `key.subkey.array=[3 5]`, found none" << std::endl;
			return eFailure;
		}
		bool cmpEq = testArray.size() == got.value().size();
		if(cmpEq) {
			for(size_t i=0; i < testArray.size(); ++i) {
				if(got.value()[i].type != apcf::DataType::eInt) { cmpEq = false; break; }
				if(got.value()[i].data.intValue != testArray[i].data.intValue) { cmpEq = false; break; }
			}
		}
		if(! cmpEq) {
			out << "Expected entry `key.subkey.array=[3 5]`, found value `[";
			if(! got.value().empty()) {
				out << got.value()[0].serialize();
				for(size_t i=1; i < got.value().size(); ++i) {
					out << ' ' << got.value()[i].serialize();
				}
			}
			out << "]`" << std::endl;
			return eFailure;
		}

		return eSuccess;
	}


	bool testInvalidKey(const std::string& key, std::ostream& out) {
		try {
			Config cfg;
			cfg.setInt(key, key.size());
			out << "Invalid key \"" << key << "\" results valid" << std::endl;
			return false;
		} catch(apcf::InvalidKey& ex) {
			return true;
		}
	}

	bool testValidKey(const std::string& key, std::ostream& out) {
		try {
			Config cfg;
			cfg.setInt(key, key.size());
			return true;
		} catch(apcf::InvalidKey& ex) {
			out << "Valid key \"" << key << "\" results invalid" << std::endl;
			return false;
		}
	}

	utest::ResultType testValidKeys(std::ostream& out) {
		return
			(
				testValidKey("key", out) &&
				testValidKey("key.key", out) &&
				testValidKey("key.key.key", out)
			)? eSuccess : eFailure;
	}

	utest::ResultType testInvalidKeys(std::ostream& out) {
		return
			(
				testInvalidKey("key..key", out) &&
				testInvalidKey("key...key", out) &&
				testInvalidKey(".key.key", out) &&
				testInvalidKey("key.key.", out) &&
				testInvalidKey("key .key", out) &&
				testInvalidKey(".", out) &&
				testInvalidKey("..", out)
			)? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEof(std::ostream&) {
		Config cfg = Config::parse("// comment + eof */");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEol(std::ostream&) {
		Config cfg = Config::parse("// comment + eol */\n");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEmpty(std::ostream&) {
		Config cfg = Config::parse("//");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}


	utest::ResultType testGroups(std::ostream& out) {
		Config cfg = Config::parse(
			"nothing = 0\n"
			"group1 {\n"
			"  value1 = 1\n"
			"  value2 = 2\n"
			"  group2{value1=3 value2=4}\n"
			"  group3 {\n"
			"    group4 { value1 = 5 }"
			"  }\n"
			"}\n" );
		return
			(
				checkValue<apcf::int_t>(cfg, out, "group1.value1", 1) &
				checkValue<apcf::int_t>(cfg, out, "group1.value2", 2) &
				checkValue<apcf::int_t>(cfg, out, "group1.group2.value1", 3) &
				checkValue<apcf::int_t>(cfg, out, "group1.group2.value2", 4) &
				checkValue<apcf::int_t>(cfg, out, "group1.group3.group4.value1", 5)
			)? eSuccess : eFailure;
	}

	utest::ResultType testUnmatchedGroupClosure(std::ostream& out) {
		try {
			Config cfg = Config::parse("group1{group2{}} }");
			out << "Expected a UnmatchedGroupClosure error to be thrown" << std::endl;
			return eFailure;
		} catch(apcf::UnmatchedGroupClosure&) {
			return eSuccess;
		}
	}

	utest::ResultType testUnclosedGroup(std::ostream& out) {
		try {
			Config cfg = Config::parse(" group1 { group2 { } ");
			out << "Expected a UnclosedGroup error to be thrown" << std::endl;
			return eFailure;
		} catch(apcf::UnclosedGroup&) {
			return eSuccess;
		}
	}


	template<bool doMove>
	utest::ResultType testMerge(std::ostream& out) {
		constexpr const char* cfg1src = "cfg1 { value=1 override=1 }";
		constexpr const char* cfg2src = "cfg1 { override=2 } cfg2 { value=2 }";
		Config cfg;
		if constexpr(doMove) {
			cfg
				<< Config::parse(cfg1src)
				<< Config::parse(cfg2src);
		} else {
			Config cfg1 = Config::parse(cfg1src);
			Config cfg2 = Config::parse(cfg2src);
			cfg << cfg1 << cfg2;
		}
		auto result = true;
		auto checkEntry = [&out, &cfg, &result](const apcf::Key& key, apcf::int_t value) {
			auto entry = cfg.getInt(key);
			if(! entry.has_value()) {
				out << "Missing entry for `" << key << '`' << std::endl;
				result = false;
			}
			if(entry.value() != value) {
				out << "Entry `" << key << "` has value " << entry.value() << ", expected " << value << std::endl;
				result = false;
			}
		};
		checkEntry("cfg1.value", 1);
		checkEntry("cfg2.value", 2);
		checkEntry("cfg1.override", 2);
		return result? eSuccess : eFailure;
	}


	utest::ResultType testGetSubkeys(std::ostream& out) {
		constexpr unsigned expectedSubkeys = 3;
		Config cfg = Config::parse("a=1 a.b=2 a.c=3 a.d.e=4 a.d.f=5 g=6 h.i=7");
		auto hierarchy = cfg.getHierarchy();
		bool success = true;
		auto subkeys = hierarchy.getSubkeys("a");
		if(subkeys.size() != expectedSubkeys) {
			success = false;
			out
				<< "Config::getSubkeysOf returned " << subkeys.size()
				<< " keys, expected " << expectedSubkeys << std::endl;
		}
		return success? eSuccess : eFailure;
	}


	utest::ResultType testStrConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = \"zero\"\n generic.key = \"one backslash \\\\ \\\"double quote\\\"\"");
		return checkValue<apcf::string_t>(cfg, out, "generic.key", "one backslash \\ \"double quote\"")? eSuccess : eFailure;
	}

	utest::ResultType testIntConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = 51\n generic.key = 62");
		return checkValue<apcf::int_t>(cfg, out, "generic.key", 62)? eSuccess : eFailure;
	}

	utest::ResultType testFloatConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = 51.4\n generic.key = 62.75");
		return checkValue<apcf::float_t>(cfg, out, "generic.key", 62.75)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigTrue(std::ostream& out) {
		Config cfg = Config::parse("nothing = false\n generic.key = true");
		return checkValue<bool>(cfg, out, "generic.key", true)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigYes(std::ostream& out) {
		Config cfg = Config::parse("nothing = no\n generic.key = yes");
		return checkValue<bool>(cfg, out, "generic.key", true)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigY(std::ostream& out) {
		Config cfg = Config::parse("nothing = n\n generic.key = y");
		return checkValue<bool>(cfg, out, "generic.key", true)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigFalse(std::ostream& out) {
		Config cfg = Config::parse("nothing = true\n generic.key = false");
		return checkValue<bool>(cfg, out, "generic.key", false)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigNo(std::ostream& out) {
		Config cfg = Config::parse("nothing = yes\n generic.key = no");
		return checkValue<bool>(cfg, out, "generic.key", false)? eSuccess : eFailure;
	}

	utest::ResultType testBoolConfigN(std::ostream& out) {
		Config cfg = Config::parse("nothing = y\n generic.key = n");
		return checkValue<bool>(cfg, out, "generic.key", false)? eSuccess : eFailure;
	}


	utest::ResultType testSerialFullPretty(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
			"group1 {\n"
			"  group2 {\n"
			"    value1 = 3\n"
			"    value2 = 4\n"
			"  }\n"
			"\n"
			"  group3.value1 = 5\n"
			"  value1 = 1\n"
			"  value2 = 2\n"
			"}\n"
			"\n"
			"long.single.line.group.1 {\n"
			"  2 {\n"
			"    value1 = 8\n"
			"    value2 = 9\n"
			"  }\n"
			"\n"
			"  value1 = 6\n"
			"  value2 = 7\n"
			"}\n"
			"\n"
			"rootvalue-array-one = [\n"
			"  1\n"
			"  2.25\n"
			"  [\n"
			"    \"3\"\n"
			"  ] [\n"
			"    \"4\"\n"
			"    \"5\"\n"
			"  ]\n"
			"]\n"
			"\n"
			"rootvalue-array-three = [ ]\n"
			"\n"
			"rootvalue-array-two = [ ]\n"
			"\n"
			"1 = 1\n"
			"1 {\n"
			"  1 = 1.5\n"
			"  2 = 0.925\n"
			"}\n"
			"\n"
			"rootvalue-float = 1.5\n"
			"\n"
			"rootvalue-int = 1\n"
			"rootvalue-int {\n"
			"  negative = -1\n"
			"  positive = 1\n"
			"}\n"
			"\n"
			"rootvalue-string = \"str \\\"literal\\\"\"\n";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eNull;
		rules.indentationSize = 2;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return eSuccess;
	}

	utest::ResultType testSerialFullCompact(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
			"1=1 "
			"1{1=1.5 2=0.925}"
			"group1{"
			"group2{value1=3 value2=4}"
			"group3.value1=5 "
			"value1=1 "
			"value2=2"
			"}"
			"long.single.line.group.1{"
			"2{value1=8 value2=9"
			"}"
			"value1=6 "
			"value2=7"
			"}"
			"rootvalue-array-one=[1 2.25[\"3\"][\"4\" \"5\"]]"
			"rootvalue-array-three=[]"
			"rootvalue-array-two=[]"
			"rootvalue-float=1.5 "
			"rootvalue-int=1 "
			"rootvalue-int{negative=-1 positive=1}"
			"rootvalue-string=\"str \\\"literal\\\"\"";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eMinimized;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return eSuccess;
	}


	utest::ResultType testSerialNan(std::ostream& out) {
		Config cfg;
		bool r = true;
		cfg.setFloat("inf", INFINITY);
		cfg.setFloat("nan", NAN);

		try {
			auto serial = cfg.get("inf").value()->serialize();
			out
				<< "Positive infinity was successfully serialized as `"
				<< serial << "`,\nwithout the `eFloatNoFail` flag set." << std::endl;
			r = false;
		} catch(apcf::InvalidValue&) { }

		try {
			auto serial = cfg.get("nan").value()->serialize();
			out
				<< "NaN was successfully serialized as `"
				<< serial << "`,\nwithout the `eFloatNoFail` flag set." << std::endl;
			r = false;
		} catch(apcf::InvalidValue&) { }

		{
			apcf::SerializationRules rules = { };
			rules.flags = apcf::SerializationRules::eFloatNoFail;
			std::string serial;
			serial = cfg.get("inf").value()->serialize(rules);
			out << "Positive infinity serialized as `" << serial << "`\n";
			serial = cfg.get("nan").value()->serialize(rules);
			out << "NaN serialized as `" << serial << '`' << std::endl;
		}

		return r? eSuccess : eFailure;
	}


	utest::ResultType testFileWrite(std::ostream&) {
		using namespace std::string_literals;
		Config cfg = Config::parse(genericConfigSrc);
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eNull;
		cfg.write(
			std::ofstream(tmpFileBase + ".test_generic.cfg"s),
			rules );
		return eSuccess;
	}


	utest::ResultType testFileRead(std::ostream& out) {
		using namespace std::string_literals;
		Config cfgCmp = Config::parse(genericConfigSrc);
		Config cfgFile = Config::read(std::ifstream(tmpFileBase + ".test_generic.cfg"s));
		if(cfgCmp.entryCount() != cfgFile.entryCount()) {
			out
				<< "Key number mismatch: read " << cfgFile.entryCount()
				<< ", expected " << cfgCmp.entryCount() << std::endl;
			return eFailure;
		}
		for(auto& cmpEntry : cfgCmp) {
			auto fileEntryFound = cfgFile.get(cmpEntry.first);
			if(! fileEntryFound.has_value()) {
				out << "Entry not found for `" << cmpEntry.first << '`' << std::endl;
				return eFailure;
			}
			if(fileEntryFound.value()->type != cmpEntry.second.type) {
				out
					<< "Type mismatch for `" << cmpEntry.first << "`:\nread "
					<< apcf::dataTypeStringOf(fileEntryFound.value()->type)
					<< ",\nexpected " << apcf::dataTypeStringOf(cmpEntry.second.type)
					<< std::endl;
				return eFailure;
			}
		}
		return eSuccess;
	}


	utest::ResultType testSubconfigNoMatch(std::ostream&) {
		Config cfg = Config::parse("group{subgroup{1=2 3=4}5=6}rootval=7");
		Config subCfg = cfg.getSubconfig("grou" /* the last letter is intentionally omitted */);
		return subCfg.entryCount() == 0? eSuccess : eFailure;
	}


	utest::ResultType testSubconfig(std::ostream& out) {
		Config cfg = Config::parse("group{subgroup{1=2 3=4}5=6}rootval=7");
		Config subCfg = cfg.getSubconfig("group");
		return
			bool(
				checkValue<apcf::int_t>(subCfg, out, "subgroup.1", 2) &
				checkValue<apcf::int_t>(subCfg, out, "subgroup.3", 4) &
				checkValue<apcf::int_t>(subCfg, out, "5", 6)
			)? eSuccess : eFailure;
	}


	template<bool existing>
	utest::ResultType testMergeAsGroup(std::ostream& out) {
		Config cfg = Config::parse("group1.1=11 group1.2=12");
		if constexpr(existing) {
			cfg.mergeAsGroup("group2", Config::parse("3=23 4=24"));
		} else {
			cfg.mergeAsGroup("group1", Config::parse("3=13 4=14"));
		}
		if constexpr(existing) {
			return
				bool(
					checkValue<apcf::int_t>(cfg, out, "group2.3", 23) &
					checkValue<apcf::int_t>(cfg, out, "group2.4", 24)
				)? eSuccess : eFailure;
		} else {
			return
				bool(
					checkValue<apcf::int_t>(cfg, out, "group1.3", 13) &
					checkValue<apcf::int_t>(cfg, out, "group1.4", 14)
				)? eSuccess : eFailure;
		}
	}

}



int main(int, char**) {
	#define RUN_(NAME_, FN_) run(NAME_, wrapExceptions(FN_))
	auto batch = utest::TestBatch(std::cout);
	batch
		.RUN_("Virtual reader and writer", testVirtualRdWr)
		.RUN_("Getter and setter (bool)", testSetGetBool)
		.RUN_("Getter and setter (int)", testSetGetInt)
		.RUN_("Getter and setter (float)", testSetGetFloat)
		.RUN_("Getter and setter (string)", testSetGetString)
		.RUN_("Getter and setter (array)", testSetGetArray)
		.RUN_("Valid keys", testValidKeys)
		.RUN_("Invalid keys", testInvalidKeys)
		.RUN_("Config merge (copy)", testMerge<false>)
		.RUN_("Config merge (move)", testMerge<true>)
		.RUN_("Get subkeys", testGetSubkeys)
		.RUN_("Subconfig (no match)", testSubconfigNoMatch)
		.RUN_("Subconfig", testSubconfig)
		.RUN_("Merge as group", testMergeAsGroup<false>)
		.RUN_("Merge as group (existing)", testMergeAsGroup<true>)
		.RUN_("[parse] Single line comment, then EOL", testReadOnelineCommentEol)
		.RUN_("[parse] Single line comment, then EOF", testReadOnelineCommentEof)
		.RUN_("[parse] Single line empty comment", testReadOnelineCommentEmpty)
		.RUN_("[parse] String value", testStrConfig)
		.RUN_("[parse] Integer value", testIntConfig)
		.RUN_("[parse] Fractional value", testFloatConfig)
		.RUN_("[parse] Boolean value (TRUE/false)", testBoolConfigTrue)
		.RUN_("[parse] Boolean value (YES/no)", testBoolConfigYes)
		.RUN_("[parse] Boolean value (Y/n)", testBoolConfigY)
		.RUN_("[parse] Boolean value (FALSE/true)", testBoolConfigFalse)
		.RUN_("[parse] Boolean value (NO/yes)", testBoolConfigNo)
		.RUN_("[parse] Boolean value (N/y)", testBoolConfigN)
		.RUN_("[parse] Groups", testGroups)
		.RUN_("[parse] Unclosed group", testUnclosedGroup)
		.RUN_("[parse] Unmatched group closure", testUnmatchedGroupClosure)
		.RUN_("[serial] Simple serialization (pretty)", testSerialFullPretty)
		.RUN_("[serial] Simple serialization (compact)", testSerialFullCompact)
		.RUN_("[serial] Simple serialization (float NaN, infinity)", testSerialNan)
		.RUN_("[file] Write to file", testFileWrite)
		.RUN_("[file] Read from file", testFileRead);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
	#undef RUN_
}
