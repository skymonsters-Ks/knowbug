
#include "pch.h"
#include <cassert>
#include <algorithm>
#include "CStrBuf.h"

static auto const stc_warning = "(too long)";
static auto const stc_warningLength = 10;

CStrBuf::CStrBuf()
	: lenLimit_(0xFFFFFFFF)
{
}

auto CStrBuf::is_full() const -> bool {
	return lenLimit_ == 0;
}

void CStrBuf::limit(size_t newLimit)
{
	assert(newLimit >= stc_warningLength);
	lenLimit_ = newLimit;
}

void CStrBuf::append(char const* s)
{
	append(s, std::strlen(s));
}

void CStrBuf::append(char const* s, size_t len)
{
	if ( lenLimit_ == 0 ) return;

	assert(len <= std::strlen(s));
	if ( len + stc_warningLength < lenLimit_ ) {
		buf_.append(s, s + len);
		lenLimit_ -= len;
	} else {
		assert( lenLimit_ >= stc_warningLength );
		auto const lenToWrite = lenLimit_ - stc_warningLength;
		buf_.append(s, lenToWrite);
		buf_.append(stc_warning);
		lenLimit_ = 0;
	}
}