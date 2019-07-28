#include "pch.h"
#include "hsx_data.h"
#include "hsx_internals.h"

namespace hsp_sdk_ext {
	auto params(HSPCTX const* ctx) -> Slice<STRUCTPRM> {
		auto size = (std::size_t)std::max(0, ctx->hsphed->max_minfo) / sizeof(STRUCTPRM);
		return Slice<STRUCTPRM>{ ctx->mem_minfo, size };
	}

	auto param_type_to_name(HspParamType param_type) -> std::optional<char const*> {
		switch (param_type) {
		case MPTYPE_LABEL:
			return u8"label";

		case MPTYPE_DNUM:
			return u8"double";

		case MPTYPE_LOCALSTRING:
			return u8"str";

		case MPTYPE_INUM:
			return u8"int";

		case MPTYPE_SINGLEVAR:
			return u8"var";

		case MPTYPE_ARRAYVAR:
			return u8"array";

		case MPTYPE_LOCALVAR:
			return u8"local";

		case MPTYPE_MODULEVAR:
		case MPTYPE_IMODULEVAR:
		case MPTYPE_TMODULEVAR:
			return u8"modvar";

		default:
			return std::nullopt;
		}
	}
}
