﻿// struct ModcmdCallInfo

#ifndef IG_STRUCT_MODCMD_CALL_INFO_H
#define IG_STRUCT_MODCMD_CALL_INFO_H
#ifdef with_WrapCall

#include "../main.h"
#include "../VarTreeNodeData_fwd.h"

namespace WrapCall
{

// 呼び出し直前の情報
struct ModcmdCallInfo
{
	using shared_ptr_type = std::shared_ptr<ModcmdCallInfo const>;
	using weak_ptr_type = std::weak_ptr<ModcmdCallInfo const>;

	// 呼び出されたコマンド
	stdat_t const stdat;

	// 呼び出し直前での prmstk
	void* const prmstk_bak;

	// 呼び出し直前でのネストレベル
	int const sublev;
	int const looplev;

	// 呼び出された位置
	char const* const fname;
	int const line; //0-based
	
	// g_stkCallInfo における位置
	size_t const idx;

public:
	ModcmdCallInfo(stdat_t stdat, void* prmstk_bak, int sublev, int looplev, char const* fname, int line, size_t idx)
		: stdat(stdat), prmstk_bak(prmstk_bak), sublev(sublev), looplev(looplev), fname(fname), line(line), idx(idx)
	{ }

	auto tryGetPrev() const -> shared_ptr_type;
	auto tryGetNext() const -> shared_ptr_type;

	//prmstk: この呼び出しの実引数情報。
	//safety: このprmstkは確実に正しいものであるか。
	auto tryGetPrmstk() const -> std::pair<void*, bool>;

	// この呼び出しを実引数式に含む呼び出し
	auto tryGetDependedCallInfo() const -> shared_ptr_type;

	auto name() const -> string { return hpiutil::STRUCTDAT_name(stdat); }
};

} //namespace WrapCall

#endif //defined(with_WrapCall)
#endif
