#include <test_tools.hpp>

#include <apcf.hpp>

#include <iostream>
#include <fstream>
#include <cassert>
#include <random>
#include <chrono>



namespace {

	using namespace std::string_literals;
	using apcf::Config;

	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eFailure = utest::ResultType::eFailure;

	template<bool pretty, unsigned rootGroups, unsigned depth>
	const std::string cfgFilePath =
		"test/.big_autogen_"s +
		(pretty? "pretty_"s : "compact_"s) +
		std::to_string(rootGroups) + "x"s +
		std::to_string(depth) + ".cfg"s;

	auto rng = std::minstd_rand();
	Config cfgWr, cfgRd;


	std::string genKey(unsigned depth) {
		#define PUSH_DIGITS_ { for(size_t i=0; i < levelLength; ++i) r.push_back(digits[rng() % base]); }
		constexpr unsigned levelLength = 5;
		constexpr unsigned base = 64;
		constexpr const char digits[] =
			"_-0123456789"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		static_assert(base == sizeof(digits)-1);
		std::string r;
		r.reserve(levelLength + (depth * (levelLength + 1)));
		PUSH_DIGITS_
		while(depth > 0) {
			r.push_back('.');
			PUSH_DIGITS_;
			-- depth;
		}
		return r;
		#undef PUSH_DIGITS_
	}

	apcf::RawData genStr() {
		constexpr unsigned length = 32;
		constexpr unsigned base = 62;
		constexpr const char digits[] =
			"0123456789"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		static_assert(base == sizeof(digits)-1);
		apcf::RawData r = apcf::RawData::allocString(length);
		for(unsigned i=0; i < length; ++i) {
			r.data.stringValue[i] = digits[rng() % base];
		}
		return r;
	}

	apcf::RawData genFloat() {
		apcf::float_t den;
		do {
			den = apcf::float_t(rng() % 0x100);
		} while(! (den < apcf::float_t(0.0) || den > apcf::float_t(0.0)));
		auto gen =
			apcf::float_t(rng() % 0x100) +
			apcf::float_t(0x100) / den;
		return gen;
	}

	apcf::RawData genArray() {
		constexpr unsigned size0 = 4;
		constexpr unsigned size1 = 3;
		apcf::RawData r = apcf::RawData::allocArray(size0);
		for(unsigned i=0; i < size0; ++i) {
			auto& arrayVal = r.data.arrayValue[i];
			arrayVal = apcf::RawData::allocArray(size1);
			for(unsigned j=0; j < size1; ++j) {
				arrayVal.data.arrayValue[j] = apcf::int_t(rng() % 0x100);
			}
		}
		return r;
	};

	void genEntries(
			Config* cfg,
			const std::string& superKey,
			unsigned minDepth, unsigned maxDepth
	) {
		if(minDepth > maxDepth) return;
		if(minDepth > 0) {
			genEntries(cfg,
				superKey + '.' + genKey(0),
				minDepth - 1, maxDepth - 1);
		} else {
			constexpr unsigned repeat = 5;
			for(unsigned i=0; i < repeat; ++i) {
				apcf::Key key = apcf::Key(superKey + '.' + genKey(0));
				switch(rng() % 5) {
					default: [[fallthrough]];
					case 0: cfg->set(key, 1 == rng() % 1); break;
					case 1: cfg->set(key, apcf::int_t(rng())); break;
					case 2: cfg->set(key, genFloat()); break;
					case 3: cfg->set(key, genStr()); break;
					case 4: cfg->set(key, genArray()); break;
				}
			}
			genEntries(cfg,
				superKey,
				minDepth + 1, maxDepth);
		}
	}

	uint_fast64_t nowUs() {
		using Clock = std::chrono::steady_clock;
		using Us = std::chrono::duration<uint_fast64_t, std::micro>;
		using std::chrono::duration_cast;
		static const auto epoch = Clock::now();
		return duration_cast<Us>(Clock::now() - epoch).count();
	}


	template<bool pretty, unsigned rootGroups, unsigned depth>
	bool testPerformanceRd(std::ostream& out) {
		using namespace std::string_literals;
		auto begTime = nowUs();
		cfgRd = Config::read(std::ifstream(cfgFilePath<pretty, rootGroups, depth>));
		auto endTime = (nowUs() - begTime);
		out
			<< "Parsing " << cfgRd.entryCount()
			<< " entries took " << endTime << "us" << std::endl;
		for(const auto& wrEntry : cfgWr) {
			apcf::SerializationRules rules = { };  rules.flags = apcf::SerializationRules::eCompactArrays;
			const auto& rdValueOpt = cfgRd.get(wrEntry.first);
			if(! rdValueOpt.has_value()) {
				out << "Config mismatch: failed to get existing key `" << wrEntry.first << "`" << std::endl;
				return false;
			}
			const auto& rdValue = *rdValueOpt.value();
			const auto& wrValue = wrEntry.second;
			if(rdValue != wrValue) {
				out
					<< "Config mismatch: key `" << wrEntry.first << "` has been written as `"
					<< wrValue.serialize(rules) << "`, but parsed as `" << rdValue.serialize(rules) << std::endl;
				return false;
			}
		}
		return true;
	}


	template<bool pretty, unsigned rootGroups, unsigned depth>
	bool testPerformanceWr(std::ostream& out) {
		using Rules = apcf::SerializationRules;
		constexpr unsigned ROOT_GROUPS = rootGroups;
		cfgWr = Config();
		for(unsigned i=0; i < ROOT_GROUPS; ++i) {
			genEntries(&cfgWr, genKey(0), 0, depth);
		}
		auto begTime = nowUs();
		Rules rules = { };
		rules.indentationSize = 3;
		if constexpr(pretty) {
			rules.flags = Rules::eCompactArrays;
		} else {
			rules.flags = Rules::eCompact | Rules::eCompactArrays;
		}
		cfgWr.write(std::ofstream(cfgFilePath<pretty, rootGroups, depth>), rules);
		auto endTime = (nowUs() - begTime);
		out
			<< "Serializing " << cfgWr.entryCount()
			<< " entries took " << endTime << "us" << std::endl;
		return true;
	}


	template<bool pretty, unsigned rootGroups, unsigned depth>
	utest::ResultType testPerformance(std::ostream& out) {
		return
			(
				testPerformanceWr<pretty, rootGroups, depth>(out) &
				testPerformanceRd<pretty, rootGroups, depth>(out)
			)? eNeutral : eFailure;
	}

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	batch
		.run("Parse/serialize benchmark (pretty, 8x4)", testPerformance<true, 8, 4>)
		.run("Parse/serialize benchmark (compact, 8x4)", testPerformance<false, 8, 4>)
		.run("Parse/serialize benchmark (pretty, 20x24)", testPerformance<true, 20, 24>)
		.run("Parse/serialize benchmark (compact, 20x24)", testPerformance<false, 20, 24>);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
