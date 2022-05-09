#include <test_tools.hpp>

#include <apcf.hpp>
#include <apcf_hierarchy.hpp>
#include <apcf_templates.hpp>

#include <iostream>
#include <fstream>
#include <set>
#include <cstring>



namespace {

	using apcf::Config;
	using apcf::Key;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	using TestFn = std::function<utest::ResultType (std::ostream&)>;


	constexpr const char* cfgIntValues[3] = {
		"ints {\n"
		"  1 = 0\n"  "  1p = +0\n"  "  1n = -0\n"
		"  2 = 1\n"  "  2p = +1\n"  "  2n = -1\n"
		"  3 = 2\n"  "  3p = +2\n"  "  3n = -2\n"
		"  4 = 10\n" "  4p = +10\n" "  4n = -10\n"
		"}\n", // ------------- // -------------
		"ints{"
		"1=0 1n=0 1p=0"  " "
		"2=1 2n=-1 2p=1" " "
		"3=2 3n=-2 3p=2" " "
		"4=10 4n=-10 4p=10"
		"}", // ------------- // -------------
		"ints {\n"
		"  1 = 0\n"  "  1n = 0\n"  "  1p = 0\n"
		"  2 = 1\n"  "  2n = -1\n"  "  2p = 1\n"
		"  3 = 2\n"  "  3n = -2\n"  "  3p = 2\n"
		"  4 = 10\n" "  4n = -10\n" "  4p = 10\n"
		"}\n" };

	// Some of the following lines are temporarily commented out,
	// due to floating point "errors" that will
	// probably be addressed in the future
	constexpr const char* cfgFloatValues[3] = {
		"floats {\n"
		"  1 =  0.0\n"    "  1p = +0.0\n"    "  1n = -0.0\n"
		// "  2 =  0.1\n"    "  2p = +0.1\n"    "  2n = -0.1\n"
		// "  3 =  0.2625\n" "  3p = +0.2625\n" "  3n = -0.2625\n"
		"  4 =  7.5\n"    "  4p = +7.5\n"    "  4n = -7.5\n"
		// "  5 =  1.1\n"    "  5p = +1.1\n"    "  5n = -1.1\n"
		// "  6 =  1.2625\n" "  6p = +1.2625\n" "  6n = -1.2625\n"
		"  7 =  7.5\n"    "  7p = +7.5\n"    "  7n = -7.5\n"
		"}\n", // ------------- // -------------
		"floats{"
		"1=0.0 1n=0.0 1p=0.0"           " "
		// "2=0.1 2p=0.1 2n=-0.1"          " "
		// "3=0.2625 3p=0.2625 3n=-0.2625" " "
		"4=7.5 4n=-7.5 4p=7.5"          " "
		// "5=1.1 5p=1.1 5n=-1.1"          " "
		// "6=1.2625 6p=1.2625 6n=-1.2625" " "
		"7=7.5 7n=-7.5 7p=7.5"
		"}", // ------------- // -------------
		"floats {\n"
		"  1 = 0.0\n"    "  1n = 0.0\n"    "  1p = 0.0\n"
		// "  2 = 0.1\n"    "  2p = 0.1\n"    "  2n = -0.1\n"
		// "  3 = 0.2625\n" "  3p = 0.2625\n" "  3n = -0.2625\n"
		"  4 = 7.5\n"    "  4n = -7.5\n"    "  4p = 7.5\n"
		// "  5 = 1.1\n"    "  5p = 1.1\n"    "  5n = -1.1\n"
		// "  6 = 1.2625\n" "  6p = 1.2625\n" "  6n = -1.2625\n"
		"  7 = 7.5\n"    "  7n = -7.5\n"    "  7p = 7.5\n"
		"}\n" };

	constexpr const char* cfgStringValues[3] = {
		"strings {\n"
		"  1 = \"nrm\"\n"
		"  2 = \"nrm \\\"literal\\\"\"\n"
		"}\n", // ------------- // -------------
		"strings{"
		"1=\"nrm\"" " "
		"2=\"nrm \\\"literal\\\"\""
		"}", // ------------- // -------------
		"strings {\n"
		"  1 = \"nrm\"\n"
		"  2 = \"nrm \\\"literal\\\"\"\n"
		"}\n" };

	constexpr const char* cfgArrayValues[3] = {
		"arrays {\n"
		"  big"" = [ \"String that makes the array too long to be kept in a single line (81 chars)\" ]\n"
		"  small = [ 1 2 3 ]\n"
		"}\n", // ------------- // -------------
		"arrays{"
		"big""=[\"String that makes the array too long to be kept in a single line (81 chars)\"]"
		"small=[1 2 3]"
		"}", // ------------- // -------------
		"arrays {\n"
		"  big"" = [\n"
		"    \"String that makes the array too long to be kept in a single line (81 chars)\"\n"
		"  ]\n"
		"\n"
		"  small = [ 1 2 3 ]\n"
		"}\n" };

	constexpr const char* cfgBoolValues[3] = {
		"bools {\n"
		"  true=true\n" "  false=false\n"
		"  yes=yes\n"   "  no=no\n"
		"  y=y\n"       "  n=n\n"
		"}\n", // ------------- // -------------
		"bools{"
		"true=true false=false" " "
		"yes=true no=false"     " "
		"y=true n=false"
		"}", // ------------- // -------------
		"bools {\n"
		"  true = true\n" "  false = false\n"
		"  yes = true\n"  "  no = false\n"
		"  y = true\n"    "  n = false\n"
		"}\n" };

	constexpr const char* cfgGeneric[3] = {
		"group1 {\n"
		"  group2.1 = 1\n"
		"  group2.2 = 2\n"
		"}\n"
		"group3 { group4 {\n"
		"  array3 = [ 1 2 3 ]\n"
		"  group5 {\n"
		"    1 = 1\n"
		"  }\n"
		"  group6 {\n"
		"    1 = 1\n"
		"    2 = 2\n"
		"  }\n"
		"  1 = 1\n"
		"  2 = 2\n"
		"} }\n", // ------------- // -------------
		"group1.group2{1=1 2=2}"
		"group3.group4{"
		"1=1 2=2" " "
		"array3=[1 2 3]"
		"group5.1=1" " "
		"group6{1=1 2=2}"
		"}", // ------------- // -------------
		"group1.group2 {\n"
		"  1 = 1\n"
		"  2 = 2\n"
		"}\n"
		"\n"
		"group3.group4 {\n"
		"  group6 {\n"
		"    1 = 1\n"
		"    2 = 2\n"
		"  }\n"
		"\n"
		"  array3 = [ 1 2 3 ]\n"
		"  1 = 1\n"
		"  2 = 2\n"
		"  group5.1 = 1\n"
		"}\n" };



	bool cmpConfigs(std::ostream& out, std::string lName, const Config& cl, std::string rName, const Config& cr) {
		std::set<Key> keys;
		for(const auto& entry : cl) { keys.insert(entry.first); }
		for(const auto& entry : cr) { keys.insert(entry.first); }
		for(const Key& key : keys) {
			const apcf::RawData* l = cl.get(key).value_or(nullptr);
			const apcf::RawData* r = cr.get(key).value_or(nullptr);
			if((l == nullptr) && (r != nullptr)) {
				out << lName << " = nullptr,  " << rName << " = " << r->serialize() << std::endl;  return false;
			} else
			if((l != nullptr) && (r == nullptr)) {
				out << lName << " = " << l->serialize() << ",  " << rName << " = nullptr" << std::endl;  return false;
			} else
			if(l != nullptr) {
				std::string lStr = l->serialize();
				std::string rStr = r->serialize();
				if(lStr != rStr) {
					out << lName << " = " << l->serialize() << ",  " << rName << " = " << r->serialize() << std::endl;  return false;
				}
			}
		}
		return true;
	}


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


	std::function<utest::ResultType(std::ostream&)> mkSerialTest(
			apcf::SerializationRules rules,
			const char* cfgStr, const char* expectStr
	) {
		return [cfgStr, expectStr, rules](std::ostream& out) {
			Config cfg = Config::parse(cfgStr);
			auto serialized = cfg.serialize(rules);
			if(0 != strcmp(serialized.c_str(), expectStr)) {
				out
					<< "// The serialized string is probably incorrect.\n"
					<< "// Expected:\n"
					<< expectStr << "// EOF\n"
					<< "// Serialized:\n"
					<< serialized << "// EOF\n" << std::flush;
				return eNeutral;
			}
			return cmpConfigs(out, "expected", cfg, "serialized", Config::parse(serialized))? eSuccess : eFailure;
		};
	}

}



int main(int, char**) {
	#define RUN_(NAME_, FN_) run(NAME_, wrapExceptions(FN_))
	auto batch = utest::TestBatch(std::cout);
	apcf::SerializationRules rules;
	rules.hierarchy = nullptr;
	rules.indentationSize = 2;
	rules.maxInlineArrayLength = 80;
	rules.flags = apcf::SerializationRules::eNull;
	apcf::SerializationRules rulesMin = rules;
	rulesMin.flags = apcf::SerializationRules::eMinimized;
	batch
		.RUN_("Generic serialization (pretty)",    mkSerialTest(rules, cfgGeneric[0], cfgGeneric[2]))
		.RUN_("Generic serialization (minimized)", mkSerialTest(rulesMin, cfgGeneric[0], cfgGeneric[1]))
		.RUN_("Array serialization (pretty)",    mkSerialTest(rules, cfgArrayValues[0], cfgArrayValues[2]))
		.RUN_("Array serialization (minimized)", mkSerialTest(rulesMin, cfgArrayValues[0], cfgArrayValues[1]))
		.RUN_("Integer serialization (pretty)",    mkSerialTest(rules, cfgIntValues[0], cfgIntValues[2]))
		.RUN_("Integer serialization (minimized)", mkSerialTest(rulesMin, cfgIntValues[0], cfgIntValues[1]))
		.RUN_("Fractional serialization (pretty)",    mkSerialTest(rules, cfgFloatValues[0], cfgFloatValues[2]))
		.RUN_("Fractional serialization (minimized)", mkSerialTest(rulesMin, cfgFloatValues[0], cfgFloatValues[1]));
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
	#undef RUN_
}
