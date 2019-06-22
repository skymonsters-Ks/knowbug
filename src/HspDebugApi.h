#pragma once

#include <optional>
#include <string>
#include "hpiutil/hpiutil_fwd.hpp"
#include "HspTypes.h"

// HSP SDK のラッパー
// 配列アクセスの範囲検査、null 検査、整数と enum の変換など、小さい仕事をする。
class HspDebugApi {
public:
	class BlockMemory;

private:
	HSPCTX* context_;

	HSP3DEBUG* debug_;

	HSPEXINFO* exinfo_;

public:
	HspDebugApi(HSP3DEBUG* debug);

	auto context() -> HSPCTX* {
		return context_;
	}

	auto debug() -> HSP3DEBUG* {
		return debug_;
	}

	auto exinfo() -> HSPEXINFO* {
		return exinfo_;
	}

	auto static_vars() -> PVal*;

	auto static_var_count() -> std::size_t;

	auto static_var_find_by_name(char const* var_name) -> std::optional<std::size_t>;

	auto static_var_find_name(std::size_t static_var_id) -> std::optional<std::string>;

	auto static_var_to_pval(std::size_t static_var_id) -> PVal*;

	auto var_to_type(PVal* pval) -> HspType;

	auto var_to_data(PVal* pval) -> HspData;

	auto var_to_lengths(PVal* pval) const -> HspIndexes;

	auto var_element_count(PVal* pval) -> std::size_t;

	auto var_element_to_indexes(PVal* pval, std::size_t aptr) -> HspIndexes;

	auto var_element_to_aptr(PVal* pval, HspIndexes const& indexes) -> std::size_t;

	auto var_element_to_data(PVal* pval, std::size_t aptr) -> HspData;

	auto var_data_to_block_memory(PVal* pval, PDAT* pdat) -> BlockMemory;

	auto var_to_block_memory(PVal* pval) -> BlockMemory;

	auto var_element_to_block_memory(PVal* pval, std::size_t aptr) -> BlockMemory;

	auto data_to_int(HspData const& data) const -> HspInt;
};

class HspDebugApi::BlockMemory {
	std::size_t size_;
	void const* data_;

public:
	BlockMemory(std::size_t size, void const* data)
		: size_(size)
		, data_(data)
	{
	}

	auto size() const -> std::size_t {
		return size_;
	}

	auto data() const -> void const* {
		return data_;
	}
};
