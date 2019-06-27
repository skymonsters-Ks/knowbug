#include "hpiutil/hpiutil.hpp"
#include "hpiutil/dinfo.hpp"
#include "HspDebugApi.h"

#undef max

#define UNSAFE(E) (E)

static auto pval_to_type(PVal const* pval) -> HspType {
	return (HspType)pval->flag;
}

static auto str_ptr_to_data(HspStr value) -> HspData {
	return HspData{ HspType::Str, (PDAT*)value };
}

static auto int_ptr_to_data(HspInt* ptr) -> HspData {
	return HspData{ HspType::Int, (PDAT*)ptr };
}

HspDebugApi::HspDebugApi(HSP3DEBUG* debug)
	: debug_(debug)
	, context_(debug->hspctx)
	, exinfo_(debug->hspctx->exinfo2)
{
}

auto HspDebugApi::current_file_ref_name() const -> std::optional<char const*> {
	auto file_ref_name = debug_->fname;

	if (file_ref_name == nullptr || std::strcmp(file_ref_name, u8"???") == 0) {
		return std::nullopt;
	}

	return std::make_optional(file_ref_name);
}

auto HspDebugApi::current_line() const -> std::size_t {
	auto line_number = debug_->line;

	if (line_number <= 0) {
		assert(false && u8"line number should start with 1");
		throw std::exception{};
	}

	return (std::size_t)(line_number - 1);
}

auto HspDebugApi::static_vars() -> PVal* {
	return context()->mem_var;
}

auto HspDebugApi::static_var_count() -> std::size_t {
	return context()->hsphed->max_val;
}

auto HspDebugApi::static_var_find_by_name(char const* var_name) -> std::optional<std::size_t> {
	assert(var_name != nullptr);

	auto index = exinfo()->HspFunc_seekvar(var_name);

	if (index < 0 || (std::size_t)index >= static_var_count()) {
		return std::nullopt;
	}

	return std::make_optional((std::size_t)index);
}

auto HspDebugApi::static_var_find_name(std::size_t static_var_id) -> std::optional<std::string> {
	assert(static_var_id < static_var_count());

	auto var_name = exinfo()->HspFunc_varname((int)static_var_id);
	if (!var_name) {
		return std::nullopt;
	}

	return std::make_optional<std::string>(var_name);
}

auto HspDebugApi::static_var_to_pval(std::size_t static_var_id) -> PVal* {
	if (static_var_id >= static_var_count()) {
		assert(false && u8"Unknown static_var_id");
		throw new std::exception{};
	}

	return static_vars() + static_var_id;
}

auto HspDebugApi::var_to_type(PVal* pval) -> HspType {
	return pval_to_type(pval);
}

auto HspDebugApi::var_to_data(PVal* pval) -> HspData {
	auto type = pval_to_type(pval);
	auto pdat = hpiutil::PVal_getPtr(pval);
	return HspData{ type, pdat };
}

auto HspDebugApi::var_to_lengths(PVal* pval) const -> HspIndexes {
	assert(pval != nullptr);

	auto lengths = HspIndexes{};
	for (auto i = std::size_t{}; i < hpiutil::ArrayDimMax; i++) {
		lengths[i] = pval->len[i + 1];
	}

	return lengths;
}

bool HspDebugApi::var_is_array(PVal* pval) const {
	return hpiutil::PVal_isStandardArray(pval);
}

auto HspDebugApi::var_to_element_count(PVal* pval) -> std::size_t {
	return hpiutil::PVal_cntElems((PVal*)pval);
}

auto HspDebugApi::var_element_to_indexes(PVal* pval, std::size_t aptr) -> HspIndexes {
	// FIXME: 2次元以上のケースを実装
	auto indexes = HspIndexes{};
	indexes[0] = aptr;
	return indexes;
}

auto HspDebugApi::var_element_to_aptr(PVal* pval, HspIndexes const& indexes) -> std::size_t {
	// FIXME: 2次元以上のケースを実装
	return indexes[0];
}

auto HspDebugApi::var_element_to_data(PVal* pval, std::size_t aptr) -> HspData {
	auto type = pval_to_type(pval);
	auto ptr = hpiutil::PVal_getPtr(pval, aptr);
	return HspData{ type, ptr };
}

auto HspDebugApi::var_data_to_block_memory(PVal* pval, PDAT* pdat) -> HspDebugApi::BlockMemory {
	assert(pval != nullptr && pdat != nullptr);

	auto varproc = hpiutil::varproc(pval->flag);

	int buffer_size;
	auto data = varproc->GetBlockSize(pval, pdat, &buffer_size);

	if (buffer_size <= 0 || data == nullptr) {
		return BlockMemory{ 0, nullptr };
	}

	return BlockMemory{ (std::size_t)buffer_size, data };
}

auto HspDebugApi::var_to_block_memory(PVal* pval) -> HspDebugApi::BlockMemory {
	assert(pval != nullptr);

	return var_data_to_block_memory(pval, pval->pt);
}

auto HspDebugApi::var_element_to_block_memory(PVal* pval, std::size_t aptr) -> HspDebugApi::BlockMemory {
	assert(pval != nullptr);

	auto pdat = hpiutil::PVal_getPtr(pval, aptr);
	return var_data_to_block_memory(pval, pdat);
}

auto HspDebugApi::data_to_label(HspData const& data) const -> HspLabel {
	if (data.type() != HspType::Label) {
		assert(false && u8"Invalid cast to label");
		throw new std::bad_cast{};
	}

	return UNSAFE(*(HspLabel const*)data.ptr());
}

auto HspDebugApi::data_to_str(HspData const& data) const -> HspStr {
	if (data.type() != HspType::Str) {
		assert(false && u8"Invalid cast to string");
		throw new std::bad_cast{};
	}

	return UNSAFE((HspStr)data.ptr());
}

auto HspDebugApi::data_to_double(HspData const& data) const -> HspDouble {
	if (data.type() != HspType::Double) {
		assert(false && u8"Invalid cast to double");
		throw new std::bad_cast{};
	}

	return UNSAFE(*(HspDouble const*)data.ptr());
}

auto HspDebugApi::data_to_int(HspData const& data) const -> HspInt {
	if (data.type() != HspType::Int) {
		assert(false && u8"Invalid cast to int");
		throw new std::bad_cast{};
	}

	return UNSAFE(*(HspInt const*)data.ptr());
}

auto HspDebugApi::data_to_flex(HspData const& data) const -> FlexValue* {
	if (data.type() != HspType::Struct) {
		assert(false && u8"Invalid cast to struct");
		throw new std::bad_cast{};
	}

	return UNSAFE((FlexValue*)data.ptr());
}

auto HspDebugApi::static_labels() -> HspCodeOffset const* {
	return context()->mem_ot;
}

auto HspDebugApi::static_label_count() -> std::size_t {
	if (context()->hsphed->max_ot < 0) {
		assert(false && u8"max_ot must be positive.");
		throw std::exception{};
	}

	return (std::size_t)context()->hsphed->max_ot;
}

auto HspDebugApi::static_label_to_label(std::size_t static_label_id) -> std::optional<HspLabel> {
	if (static_label_id >= static_label_count()) {
		return std::nullopt;
	}

	return std::make_optional(context()->mem_mcs + context()->mem_ot[static_label_id]);
}

bool HspDebugApi::flex_is_nullmod(FlexValue* flex) const {
	assert(flex != nullptr);
	return !flex->ptr || flex->type == FLEXVAL_TYPE_NONE;
}

bool HspDebugApi::flex_is_clone(FlexValue* flex) const {
	assert(flex != nullptr);
	return flex->type == FLEXVAL_TYPE_CLONE;
}

auto HspDebugApi::flex_to_module_struct(FlexValue* flex) const -> STRUCTDAT const* {
	return hpiutil::FlexValue_module(flex);
}

auto HspDebugApi::flex_to_module_tag(FlexValue* flex) const -> STRUCTPRM const* {
	return hpiutil::FlexValue_structTag(flex);
}

auto HspDebugApi::flex_to_member_count(FlexValue* flex) const -> std::size_t {
	assert(flex != nullptr);

	auto struct_dat = flex_to_module_struct(flex);
	auto param_count = struct_to_param_count(struct_dat);

	// NOTE: STRUCT_TAG というダミーのパラメータがあるため、メンバ変数の個数は1つ少ない。
	if (param_count == 0) {
		assert(false && u8"STRUCT_TAG を持たないモジュールはモジュール変数のインスタンスを作れないはず");
		return 0;
	}

	return param_count - 1;
}

auto HspDebugApi::flex_to_member_at(FlexValue* flex, std::size_t member_index) const -> HspParamData {
	auto member_count = flex_to_member_count(flex);
	if (member_index >= member_count) {
		assert(false && u8"Invalid member_index in flex");
		throw new std::exception{};
	}

	// 先頭の STRUCT_TAG を除いて数える。
	auto param_index = member_index + 1;

	auto&& param_stack = flex_to_param_stack(flex);
	return param_stack_to_data_at(param_stack, param_index);
}

auto HspDebugApi::flex_to_param_stack(FlexValue* flex) const -> HspParamStack {
	auto struct_dat = flex_to_module_struct(flex);
	return HspParamStack{ struct_dat, flex->ptr };
}

auto HspDebugApi::structs() const -> STRUCTDAT const* {
	return hpiutil::finfo().begin();
}

auto HspDebugApi::struct_count() const -> std::size_t {
	return hpiutil::finfo().size();
}

auto HspDebugApi::struct_to_name(STRUCTDAT const* struct_dat) const -> char const* {
	return hpiutil::STRUCTDAT_name(struct_dat);
}

auto HspDebugApi::struct_to_param_count(STRUCTDAT const* struct_dat) const -> std::size_t {
	return hpiutil::STRUCTDAT_params(struct_dat).size();
}

auto HspDebugApi::struct_to_param_at(STRUCTDAT const* struct_dat, std::size_t param_index) const -> STRUCTPRM const* {
	return hpiutil::STRUCTDAT_params(struct_dat).begin() + param_index;
}

auto HspDebugApi::params() const -> STRUCTPRM const* {
	return hpiutil::minfo().begin();
}

auto HspDebugApi::param_count() const -> std::size_t {
	return hpiutil::minfo().size();
}

auto HspDebugApi::param_to_param_id(STRUCTPRM const* param) const -> std::size_t {
	auto id = hpiutil::STRUCTPRM_miIndex(param);
	if (id < 0) {
		assert(false && u8"Invalid STRUCTPRM");
		throw new std::exception{};
	}

	return (std::size_t)id;
}

auto HspDebugApi::param_to_name(STRUCTPRM const* param, hpiutil::DInfo const& debug_segment) const -> std::string {
	auto struct_dat = hpiutil::STRUCTPRM_stdat(param);
	auto param_count = struct_to_param_count(struct_dat);

	// この struct_dat は param を引数に持つので、少なくとも1つの引数を持つ。
	assert(param_count >= 1);
	auto first_param = struct_to_param_at(struct_dat, 0);

	assert(first_param <= param && param < first_param + param_count);
	auto param_index = (std::size_t)(param - first_param);

	return hpiutil::nameFromStPrm(param, (int)param_index, debug_segment);
}

auto HspDebugApi::param_stack_to_data_count(HspParamStack const& param_stack) const -> std::size_t {
	return hpiutil::STRUCTDAT_params(param_stack.struct_dat()).size();
}

auto HspDebugApi::param_stack_to_data_at(HspParamStack const& param_stack, std::size_t param_index) const -> HspParamData {
	if (param_index >= param_stack_to_data_count(param_stack)) {
		assert(false && u8"Invalid param_index");
		throw new std::exception{};
	}

	auto param = hpiutil::STRUCTDAT_params(param_stack.struct_dat()).begin() + param_index;
	auto ptr = (void*)((char const*)param_stack.ptr() + param->offset);
	return HspParamData{ param, param_index, ptr };
}

auto HspDebugApi::param_data_to_type(HspParamData const& param_data) const -> HspParamType {
	return param_data.param()->mptype;
}

auto HspDebugApi::param_data_as_local_var(HspParamData const& param_data) const -> PVal* {
	if (param_data_to_type(param_data) != MPTYPE_LOCALVAR) {
		assert(false && u8"Casting to local var");
		throw new std::bad_cast{};
	}
	return UNSAFE((PVal*)param_data.ptr());
}

auto HspDebugApi::param_data_to_data(HspParamData const& param_data) const -> std::optional<HspData> {
	switch (param_data_to_type(param_data)) {
	case MPTYPE_LOCALSTRING:
		{
			auto str = UNSAFE(*(char**)param_data.ptr());
			if (!str) {
				assert(false && u8"str param must not be null");
				return std::nullopt;
			}
			return std::make_optional(str_ptr_to_data(str));
		}
	case MPTYPE_INUM:
		{
			auto ptr = UNSAFE((HspInt*)param_data.ptr());
			return std::make_optional(int_ptr_to_data(ptr));
		}
	default:
		return std::nullopt;
	}
}
