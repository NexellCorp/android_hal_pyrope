/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_str.h"
#include "common/essl_common.h"

int _essl_string_cmp(string a, string b)
{
	int i;
	assert(a.len >= 0 && a.ptr != 0);
	assert(b.len >= 0 && b.ptr != 0);
	if (a.len == b.len && a.ptr == b.ptr)
	{
		return 0;
	}
	for (i = 0; i < a.len && i < b.len; ++i)
	{
		if (a.ptr[i] != b.ptr[i])
		{
			if (a.ptr[i] < b.ptr[i])
			{
				return -1;
			}
			else
			{
				return 1;
			}
		}
	}
	if (a.len < b.len)
	{
		return -1;
	}
	else if (a.len > b.len)
	{
		return 1;
	}
	return 0;
}

/** Compares string a with the c-like string b. The specified count is the number of characters that must match.
  * @param a The first string
  * @param b The second string, which must have atleast count number of characters
  * @param count The number of characters that must match
  * @return 0 if strings match for the first count characters, else non-zero is returned
  */
int _essl_string_cstring_count_cmp(const string a, const char *b, size_t count)
{
	if (a.len < 0 || ((size_t)a.len) < count)
	{
		return -1;
	}
	return memcmp(a.ptr, b, count);
}

const char *_essl_string_to_cstring(mempool *p, string src)
{
	char *dst;
	ESSL_CHECK_NONFATAL(dst = _essl_mempool_alloc(p, (size_t)(src.len + 1)));
	if (src.ptr == 0)
	{
		return "<null>";
	}
	strncpy(dst, src.ptr, (size_t)src.len);
	dst[src.len] = '\0';
	return dst;
}


string _essl_cstring_to_string(mempool *pool, const char *str)
{
	string s;
	size_t len = strlen(str);
	char *data = _essl_mempool_alloc(pool, len);
	if (!data)
	{
		s.ptr = 0;
		s.len = 0;
	}
	else
	{
		strncpy(data, str, len);
		s.ptr = data;
		s.len = (int)len;
	}
	return s;
}

string _essl_cstring_to_string_nocopy(const char *str)
{
	string s;
	s.ptr = str;
	s.len = (int)strlen(str);
	return s;
}



#ifdef UNIT_TEST

int main(void)
{
	int i, j;
	mempool_tracker tracker;
	mempool pool;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	for (i = 0; i < 500; ++i)
	{
		string stra;
		char bufa[100];
		sprintf(bufa, "test%i%i", i, (i * i % 37));
		stra = _essl_cstring_to_string(&pool, bufa);
		assert(stra.ptr && stra.len);
		assert(!strcmp(_essl_string_to_cstring(&pool, stra), bufa));
		for (j = 0; j < 500; ++j)
		{
			char bufb[100];
			string strb;
			int normresult;
			int esslresult;
			sprintf(bufb, "test%i%i", j, (j * j % 37));
			strb = _essl_cstring_to_string(&pool, bufb);
			assert(strb.ptr && stra.len);
			assert(!strcmp(_essl_string_to_cstring(&pool, strb), bufb));
			normresult = strcmp(bufa, bufb);
			esslresult = _essl_string_cmp(stra, strb);
			assert((normresult < 0) == (esslresult < 0));
			assert((normresult > 0) == (esslresult > 0));
			assert((normresult == 0) == (esslresult == 0));
		}
	}
	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;
}

#endif /* UNIT_TEST */
