#pragma once

#ifdef __cplusplus
	extern "C" {
#endif


#ifdef __cplusplus
	#include <apcf_fwd.hpp>
	#include <cstdlib>

	using apcf_Config = apcf::Config;
	using apcf_RawData = apcf::RawData;
#else
	#include <stdlib.h>
	#include <stdbool.h>

	typedef void apcf_Config;
	typedef void apcf_RawData;
#endif



struct apcf_ReaderFnSet {
	void* userPtr;
	bool (*isAtEof)(void* userPtr);
	char (*getChar)(void* userPtr);
	bool (*fwdOrEof)(void* userPtr);
};

struct apcf_WriterFnSet {
	void* userPtr;
	void (*writeChar)(void* userPtr, char);
	void (*writeChars)(void* userPtr, const char* begin, const char* end);
};

struct apcf_ArrayIterator {
	apcf_RawData* data;
	size_t index;
};



enum apcf_Result : unsigned {
	APCF_R_SUCCESS = 0,
	APCF_R_NFOUND,
	APCF_R_STDERRNO,
	APCF_R_BADKEY,
	APCF_R_BADTYPE
};

enum apcf_Type : unsigned {
	APCF_T_NONE = 0,
	APCF_T_BOOL,
	APCF_T_INT,
	APCF_T_FLOAT,
	APCF_T_STRING,
	APCF_T_ARRAY
};



typedef bool apcf_bool_t;
typedef long long apcf_int_t;
typedef double apcf_float_t;
typedef void* apcf_arrayval_t;



apcf_Config* apcf_create();
void apcf_destroy(apcf_Config*);

apcf_Result apcf_parse(apcf_Config*, const char* nullTermStr);
apcf_Result apcf_parse2(apcf_Config*, const char* charSeq, size_t length);

apcf_Result apcf_merge(apcf_Config*, const apcf_Config* src);
apcf_Result apcf_mergeAsGroup(apcf_Config*, const apcf_Config* src, const char* grp);

apcf_Result apcf_get(apcf_Config*, const char* key, apcf_RawData* dst);
apcf_Result apcf_getBool(apcf_Config*, const char* key, apcf_bool_t* dst);
apcf_Result apcf_getInt(apcf_Config*, const char* key, apcf_int_t* dst);
apcf_Result apcf_getFloat(apcf_Config*, const char* key, apcf_float_t* dst);
apcf_Result apcf_getString(apcf_Config*, const char* key, const char** dstBase, size_t* dstLen);

apcf_Type apcf_typeOf(apcf_Config*, const apcf_RawData*);
apcf_Result apcf_boolOf(apcf_Config*, const apcf_RawData*, apcf_bool_t* dst);
apcf_Result apcf_intOf(apcf_Config*, const apcf_RawData*, apcf_int_t* dst);
apcf_Result apcf_floatOf(apcf_Config*, const apcf_RawData*, apcf_float_t* dst);
apcf_Result apcf_stringOf(apcf_Config*, const apcf_RawData*, const char** dstBase, size_t* dstLen);
apcf_Result apcf_arrayIterOf(apcf_Config*, const apcf_RawData*, apcf_ArrayIterator* dst);

size_t apcf_arraySize(const apcf_ArrayIterator*);
bool apcf_arrayAtEnd(const apcf_ArrayIterator*);
const apcf_RawData* apcf_arrayGet(const apcf_ArrayIterator*);
const apcf_RawData* apcf_arrayFetch(apcf_ArrayIterator*);



#ifdef __cplusplus
	} // extern "C"
#endif
