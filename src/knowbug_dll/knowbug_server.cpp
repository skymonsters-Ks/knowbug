#include "pch.h"
#include <array>
#include <memory>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <vector>
#include "../knowbug_core/encoding.h"
#include "../knowbug_core/hsp_object_path.h"
#include "../knowbug_core/hsp_object_writer.h"
#include "../knowbug_core/hsp_objects.h"
#include "../knowbug_core/hsx.h"
#include "../knowbug_core/platform.h"
#include "../knowbug_core/step_controller.h"
#include "../knowbug_core/string_writer.h"
#include "knowbug_app.h"
#include "knowbug_server.h"

#include "../knowbug_client/knowbug_protocol.h"

class KnowbugServerImpl;

static constexpr auto KNOWBUG_VERSION = u8"v2.0.0-beta3";

#ifdef _M_X64
static constexpr auto KNOWBUG_PLATFORM_SUFFIX = u8" (x64)";
#else //defined(_M_X64)
static constexpr auto KNOWBUG_PLATFORM_SUFFIX = u8"";
#endif

#ifdef HSP3_UTF8
static constexpr auto KNOWBUG_ENCODING_SUFFIX = u8" (UTF-8)";
#else
static constexpr auto KNOWBUG_ENCODING_SUFFIX = u8"";
#endif

static auto knowbug_version() -> Utf8String {
	auto suffix = Utf8String{ as_utf8(KNOWBUG_VERSION) };
	suffix += as_utf8(KNOWBUG_PLATFORM_SUFFIX);
	suffix += as_utf8(KNOWBUG_ENCODING_SUFFIX);
	return suffix;
}

// -----------------------------------------------
// オブジェクトリスト
// -----------------------------------------------

static auto constexpr MAX_CHILD_COUNT = std::size_t{ 300 };

class HspObjectIdProvider {
public:
	virtual auto path_to_object_id(HspObjectPath const& path)->std::size_t = 0;

	virtual auto object_id_to_path(std::size_t object_id)->std::optional<std::shared_ptr<HspObjectPath const>> = 0;
};

class HspObjectListExpansion {
public:
	virtual auto is_expanded(HspObjectPath const& path) const -> bool = 0;
};

class HspObjectListItem {
	std::size_t object_id_;
	std::size_t depth_;
	Utf8String name_;
	Utf8String value_;
	std::size_t child_count_;

public:
	HspObjectListItem(std::size_t object_id, std::size_t depth, Utf8String name, Utf8String value, std::size_t child_count)
		: object_id_(object_id)
		, depth_(depth)
		, name_(std::move(name))
		, value_(std::move(value))
		, child_count_(child_count)
	{
	}

	auto object_id() const -> std::size_t {
		return object_id_;
	}

	auto depth() const ->std::size_t {
		return depth_;
	}

	auto name() const ->Utf8StringView {
		return name_;
	}

	auto value() const ->Utf8StringView {
		return value_;
	}

	auto child_count() const -> std::size_t {
		return child_count_;
	}

	auto equals(HspObjectListItem const& other) const -> bool {
		return object_id() == other.object_id()
			&& depth() == other.depth()
			&& name() == other.name()
			&& value() == other.value()
			&& child_count() == other.child_count();
	}
};

class HspObjectList {
	std::vector<HspObjectListItem> items_;

public:
	auto items() const ->std::vector<HspObjectListItem> const& {
		return items_;
	}

	auto size() const -> std::size_t {
		return items().size();
	}

	auto operator[](std::size_t index) const -> HspObjectListItem const& {
		return items().at(index);
	}

	auto find_by_object_id(std::size_t object_id) const -> std::optional<HspObjectListItem const*> {
		for (auto&& item : items()) {
			if (item.object_id() == object_id) {
				return &item;
			}
		}
		return std::nullopt;
	}

	void add_item(HspObjectListItem item) {
		items_.push_back(std::move(item));
	}
};

// オブジェクトリストを構築する関数。
class HspObjectListWriter {
	HspObjects& objects_;
	HspObjectList& object_list_;
	HspObjectIdProvider& id_provider_;
	HspObjectListExpansion& expansion_;

	std::size_t depth_;

public:
	HspObjectListWriter(HspObjects& objects, HspObjectList& object_list, HspObjectIdProvider& id_provider, HspObjectListExpansion& expansion)
		: objects_(objects)
		, object_list_(object_list)
		, id_provider_(id_provider)
		, expansion_(expansion)
		, depth_()
	{
	}

	void add(HspObjectPath const& path) {
		if (path.child_count(objects()) == 1) {
			auto value_path = path.child_at(0, objects());
			switch (value_path->kind()) {
			case HspObjectKind::Label:
			case HspObjectKind::Str:
			case HspObjectKind::Double:
			case HspObjectKind::Int:
			case HspObjectKind::Unknown:
				add_value(path, *value_path);
				return;

			default:
				break;
			}
		}

		add_scope(path);
	}

	void add_children(HspObjectPath const& path) {
		if (!expansion_.is_expanded(path)) {
			return;
		}

		auto item_count = path.child_count(objects());
		for (auto i = std::size_t{}; i < std::min(MAX_CHILD_COUNT, item_count); i++) {
			auto item_path = path.child_at(i, objects());
			add(*item_path);
		}
	}

private:
	void add_scope(HspObjectPath const& path) {
		auto name = path.name(objects());
		auto item_count = path.child_count(objects());

		auto value = Utf8String{ as_utf8(u8"(") };
		value += as_utf8(std::to_string(item_count));
		value += as_utf8(u8"):");

		auto object_id = id_provider_.path_to_object_id(path);
		object_list_.add_item(HspObjectListItem{ object_id, depth_, name, value, item_count });
		depth_++;
		add_children(path);
		depth_--;
	}

	void add_value(HspObjectPath const& path, HspObjectPath const& value_path) {
		auto name = path.name(objects());

		auto value_writer = StringWriter{};
		HspObjectWriter{ objects(), value_writer }.write_flow_form(value_path);
		auto value = value_writer.finish();

		auto object_id = id_provider_.path_to_object_id(path);
		object_list_.add_item(HspObjectListItem{ object_id, depth_, name, value, 0 });
	}

	auto objects() -> HspObjects& {
		return objects_;
	}
};

class HspObjectListDelta {
public:
	enum class Kind {
		Insert,
		Remove,
		Update,
	};

	static auto kind_to_string(Kind kind) -> Utf8StringView {
		switch (kind) {
		case Kind::Insert:
			return as_utf8(u8"+");

		case Kind::Remove:
			return as_utf8(u8"-");

		case Kind::Update:
			return as_utf8(u8"!");

		default:
			throw std::exception{};
		}
	}

private:
	Kind kind_;
	std::size_t object_id_;
	std::size_t index_;
	std::size_t depth_;
	Utf8String name_;
	Utf8String value_;

public:
	HspObjectListDelta(Kind kind, std::size_t object_id, std::size_t index, std::size_t depth, Utf8String name, Utf8String value)
		: kind_(kind)
		, object_id_(object_id)
		, index_(index)
		, depth_(depth)
		, name_(std::move(name))
		, value_(std::move(value))
	{
	}

	static auto new_insert(std::size_t index, HspObjectListItem const& item) -> HspObjectListDelta {
		return HspObjectListDelta{
			Kind::Insert,
			item.object_id(),
			index,
			item.depth(),
			Utf8String{ item.name() },
			Utf8String{ item.value() }
		};
	}

	static auto new_remove(std::size_t object_id, std::size_t index) -> HspObjectListDelta {
		return HspObjectListDelta{
			Kind::Remove,
			object_id,
			index,
			std::size_t{},
			Utf8String{},
			Utf8String{}
		};
	}

	static auto new_update(std::size_t index, HspObjectListItem const& item) -> HspObjectListDelta {
		return HspObjectListDelta{
			Kind::Update,
			item.object_id(),
			index,
			item.depth(),
			Utf8String{ item.name() },
			Utf8String{ item.value() }
		};
	}

	auto kind() const -> Kind {
		return kind_;
	}

	auto object_id() const -> std::size_t {
		return object_id_;
	}

	auto index() const -> std::size_t {
		return index_;
	}

	auto write_to(StringWriter& w) {
		static auto constexpr SPACES = u8"                ";

		w.cat(kind_to_string(kind_));
		w.cat(u8",");
		w.cat_size(object_id());
		w.cat(u8",");
		w.cat_size(index_);
		w.cat(u8",");
		w.cat(as_utf8(SPACES).substr(0, depth_ * 2));
		w.cat(name_);
		w.cat(u8",");
		w.cat(value_);
		w.cat_crlf();
	}
};

static auto diff_object_list(HspObjectList const& source, HspObjectList const& target, std::vector<HspObjectListDelta>& diff) {
	auto source_done = std::vector<bool>{};
	source_done.resize(source.size());

	auto target_done = std::vector<bool>{};
	target_done.resize(target.size());

	// FIXME: 高速化
	for (auto si = std::size_t{}; si < source.size(); si++) {
		if (source_done[si]) {
			continue;
		}

		for (auto ti = std::size_t{}; ti < target.size(); ti++) {
			if (target_done[ti]) {
				continue;
			}

			if (source[si].object_id() == target[ti].object_id()) {
				source_done[si] = true;
				target_done[ti] = true;
				break;
			}
		}
	}

	{
		auto si = std::size_t{};
		auto ti = std::size_t{};

		while (si < source.size() || ti < target.size()) {
			if (ti == target.size() || (si < source.size() && !source_done[si])) {
				diff.push_back(HspObjectListDelta::new_remove(source[si].object_id(), ti));
				si++;
				continue;
			}

			if (si == source.size() || (ti < target.size() && !target_done[ti])) {
				diff.push_back(HspObjectListDelta::new_insert(ti, target[ti]));
				ti++;
				continue;
			}

			assert(si < source.size() && ti < target.size());
			assert(source_done[si] && target_done[ti]);

			if (source[si].object_id() == target[ti].object_id()) {
				auto&& s = source[si];
				auto&& t = target[ti];
				if (!s.equals(t)) {
					diff.push_back(HspObjectListDelta::new_update(ti, target[ti]));
				}

				si++;
				ti++;
				continue;
			}

			assert(false && u8"パスの順番が入れ替わるケースは未実装。");
			diff.clear();
			break;
		}
	}
}

class HspObjectListEntity
	: public HspObjectIdProvider
	, public HspObjectListExpansion
{
	HspObjectList object_list_;

	std::size_t last_id_;
	std::unordered_map<std::size_t, std::shared_ptr<HspObjectPath const>> id_to_paths_;
	std::unordered_map<std::shared_ptr<HspObjectPath const>, std::size_t> path_to_ids_;

	std::unordered_map<std::shared_ptr<HspObjectPath const>, bool> expanded_;

public:
	HspObjectListEntity()
		: object_list_()
		, last_id_()
		, id_to_paths_()
		, path_to_ids_()
		, expanded_()
	{
	}

	auto size() const -> std::size_t {
		return object_list_.size();
	}

	auto path_to_object_id(HspObjectPath const& path) -> std::size_t override {
		auto iter = path_to_ids_.find(path.self());
		if (iter == path_to_ids_.end()) {
			auto id = ++last_id_;
			path_to_ids_[path.self()] = id;
			id_to_paths_[id] = path.self();
			return id;
		}

		return iter->second;
	}

	auto object_id_to_path(std::size_t object_id) -> std::optional<std::shared_ptr<HspObjectPath const>> override {
		auto iter = id_to_paths_.find(object_id);
		if (iter == id_to_paths_.end()) {
			return std::nullopt;
		}

		return iter->second;
	}

	auto is_expanded(HspObjectPath const& path) const -> bool {
		auto iter = expanded_.find(path.self());
		if (iter == expanded_.end()) {
			// ルートの子要素は既定で開く。
			return path.parent().kind() == HspObjectKind::Root;
		}

		return iter->second;
	}

	auto update(HspObjects& objects) -> std::vector<HspObjectListDelta> {
		auto new_list = HspObjectList{};
		HspObjectListWriter{ objects, new_list, *this, *this }.add_children(objects.root_path());

		auto diff = std::vector<HspObjectListDelta>{};
		diff_object_list(object_list_, new_list, diff);

		for (auto&& delta : diff) {
			apply_delta(delta, new_list);
		}

		object_list_ = std::move(new_list);
		return diff;
	}

	void toggle_expand(std::size_t object_id) {
		auto path_opt = object_id_to_path(object_id);
		if (!path_opt) {
			return;
		}

		auto item_opt = object_list_.find_by_object_id(object_id);
		if(!item_opt) {
			return;
		}

		// 子要素のないノードは開閉しない。
		if ((**item_opt).child_count() == 0) {
			return;
		}

		expanded_[*path_opt] = !is_expanded(**path_opt);
	}

private:
	void apply_delta(HspObjectListDelta const& delta, HspObjectList& new_list) {
		switch (delta.kind()) {
		case HspObjectListDelta::Kind::Remove: {
			auto object_id = delta.object_id();
			auto iter = id_to_paths_.find(object_id);
			if (iter == id_to_paths_.end()) {
				assert(false);
				return;
			}

			auto path = iter->second;
			id_to_paths_.erase(iter);

			{
				auto iter = path_to_ids_.find(path);
				if (iter != path_to_ids_.end()) {
					path_to_ids_.erase(iter);
				}
			}

			{
				auto iter = expanded_.find(path);
				if (iter != expanded_.end()) {
					expanded_.erase(iter);
				}
			}
			return;
		}
		default:
			return;
		}
	}
};

// -----------------------------------------------
// ヘルパー
// -----------------------------------------------

class Win32CloseHandleFn {
public:
	using pointer = HANDLE;

	void operator()(HANDLE p) {
		CloseHandle(p);
	}
};

class Win32DestroyWindowFn {
public:
	using pointer = HWND;

	void operator()(HWND p) {
		DestroyWindow(p);
	}
};

class Win32UnmapViewOfFileFn {
public:
	using pointer = LPVOID;

	void operator()(LPVOID p) {
		UnmapViewOfFile(p);
	}
};

using MemoryMappedFile = std::unique_ptr<HANDLE, Win32CloseHandleFn>;

using MemoryMappedFileView = std::unique_ptr<LPVOID, Win32UnmapViewOfFileFn>;

using ProcessHandle = std::unique_ptr<HANDLE, Win32CloseHandleFn>;

using ThreadHandle = std::unique_ptr<HANDLE, Win32CloseHandleFn>;

using WindowHandle = std::unique_ptr<HWND, Win32DestroyWindowFn>;

static void fail_with(OsStringView reason) {
	MessageBox(HWND{}, to_owned(reason).data(), TEXT("knowbug"), MB_ICONWARNING);
	exit(EXIT_FAILURE);
}

// -----------------------------------------------
// メモリマップドファイル
// -----------------------------------------------

static auto create_memory_mapped_file(OsString const& name) -> MemoryMappedFile {
	auto mapping_handle = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		LPSECURITY_ATTRIBUTES{},
		PAGE_READWRITE,
		DWORD{},
		(DWORD)MEMORY_BUFFER_SIZE,
		name.data()
	);
	if (!mapping_handle) {
		MessageBox(HWND{}, TEXT("デバッグウィンドウの初期化に失敗しました。(サーバーがデータ交換バッファーを作成できませんでした。)"), TEXT("knowbug"), MB_ICONERROR);
		exit(EXIT_FAILURE);
	}

	return MemoryMappedFile{ mapping_handle };
}

static auto connect_memory_mapped_file(MemoryMappedFile const& mapping_handle) -> MemoryMappedFileView {
	auto data = MapViewOfFile(mapping_handle.get(), FILE_MAP_ALL_ACCESS, 0, 0, MEMORY_BUFFER_SIZE);
	if (!data) {
		MessageBox(HWND{}, TEXT("デバッグウィンドウの初期化に失敗しました。(サーバーがデータ交換バッファーへのビューを作成できませんでした。)"), TEXT("knowbug"), MB_ICONERROR);
		exit(EXIT_FAILURE);
	}

	return MemoryMappedFileView{ data };
}

// -----------------------------------------------
// 隠しウィンドウ
// -----------------------------------------------

static auto WINAPI process_hidden_window(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT;

static auto create_hidden_window(HINSTANCE instance) -> WindowHandle {
	static auto constexpr CLASS_NAME = TEXT("KnowbugHiddenWindowClass");
	static auto constexpr TITLE = TEXT("Knowbug Hidden Window");
	static auto constexpr STYLE = DWORD{};
	static auto constexpr POS_X = -1000;
	static auto constexpr POS_Y = -1000;
	static auto constexpr SIZE_X = 10;
	static auto constexpr SIZE_Y = 10;

	auto wndclass = WNDCLASS{};
	wndclass.lpfnWndProc = process_hidden_window;
	wndclass.hInstance = instance;
	wndclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndclass.lpszClassName = CLASS_NAME;
	RegisterClass(&wndclass);

	auto hwnd = CreateWindow(
		CLASS_NAME,
		TITLE,
		STYLE,
		POS_X,
		POS_Y,
		SIZE_X,
		SIZE_Y,
		HWND{},
		HMENU{},
		instance,
		LPARAM{}
	);
	if (!hwnd) {
		MessageBox(HWND{}, TEXT("デバッグウィンドウの初期化に失敗しました。(サーバーウィンドウを作成に失敗しました。)"), TEXT("knowbug"), MB_ICONERROR);
		exit(EXIT_FAILURE);
	}
	auto window_handle = WindowHandle{ hwnd };

	ShowWindow(window_handle.get(), SW_HIDE);
	return window_handle;
}

// -----------------------------------------------
// クライアントプロセス
// -----------------------------------------------

// FIXME: knowbug_config と重複
static auto get_hsp_dir() -> OsString {
	// DLL の絶対パスを取得する。
	auto buffer = std::array<TCHAR, MAX_PATH>{};
	GetModuleFileName(GetModuleHandle(nullptr), buffer.data(), buffer.size());
	auto full_path = OsString{ buffer.data() };

	// ファイル名の部分を削除
	while (!full_path.empty()) {
		auto last = full_path[full_path.length() - 1];
		if (last == TEXT('/') || last == TEXT('\\')) {
			break;
		}

		full_path.pop_back();
	}

	return full_path;
}

static auto start_client_process(HWND server_hwnd) -> std::pair<ThreadHandle, ProcessHandle> {
	auto name = get_hsp_dir();
	name += TEXT("knowbug_client.exe");

	auto server_hwnd_str = to_os(as_utf8(std::to_string((UINT_PTR)server_hwnd)));

	auto cmdline = OsString{ TEXT("\"") };
	cmdline += name;
	cmdline += TEXT("\" ");
	cmdline += server_hwnd_str;

	auto si = STARTUPINFO{ sizeof(STARTUPINFO) };
	auto pi = PROCESS_INFORMATION{};

	auto success = CreateProcess(
		LPCTSTR{},
		cmdline.data(),
		LPSECURITY_ATTRIBUTES{},
		LPSECURITY_ATTRIBUTES{},
		FALSE,
		NORMAL_PRIORITY_CLASS,
		LPTSTR{},
		LPCTSTR{},
		&si,
		&pi
	);
	if (!success) {
		MessageBox(HWND{}, TEXT("デバッグウィンドウの初期化に失敗しました。(クライアントプロセスを起動できませんでした。)"), TEXT("knowbug"), MB_ICONERROR);
		exit(EXIT_FAILURE);
	}

	auto thread_handle = ThreadHandle{ pi.hThread };
	auto process_handle = ProcessHandle{ pi.hProcess };
	return std::make_pair(std::move(thread_handle), std::move(process_handle));
}

// -----------------------------------------------
// メッセージ
// -----------------------------------------------

class Msg {
	int kind_;
	int wparam_;
	int lparam_;
	Utf8String text_;

public:
	Msg(int kind, int wparam, int lparam, Utf8String text)
		: kind_(kind)
		, wparam_(wparam)
		, lparam_(lparam)
		, text_(std::move(text))
	{
	}

	auto kind() const -> int {
		return kind_;
	}

	auto wparam() const -> int {
		return wparam_;
	}

	auto lparam() const -> int {
		return lparam_;
	}

	auto text() const -> Utf8StringView {
		return text_;
	}
};

// -----------------------------------------------
// サーバー
// -----------------------------------------------

static auto s_server = std::weak_ptr<KnowbugServerImpl>{};

class KnowbugServerImpl
	: public KnowbugServer
{
	// NOTE: メンバーの順番はデストラクタの呼び出し順序 (下から上へ) に影響する。

	HSP3DEBUG* debug_;

	HspObjects& objects_;

	HINSTANCE instance_;

	KnowbugStepController& step_controller_;

	bool started_;

	OsString server_buffer_name_;

	std::optional<MemoryMappedFile> server_buffer_opt_;

	std::optional<MemoryMappedFileView> server_buffer_view_opt_;

	OsString client_buffer_name_;

	std::optional<MemoryMappedFile> client_buffer_opt_;

	std::optional<MemoryMappedFileView> client_buffer_view_opt_;

	std::optional<WindowHandle> hidden_window_opt_;

	std::optional<HWND> client_hwnd_opt_;

	std::optional<ProcessHandle> client_process_opt_;

	std::optional<ThreadHandle> client_thread_opt_;

	std::vector<Msg> send_queue_;

	HspObjectListEntity object_list_entity_;

public:
	KnowbugServerImpl(HSP3DEBUG* debug, HspObjects& objects, HINSTANCE instance, KnowbugStepController& step_controller)
		: debug_(debug)
		, objects_(objects)
		, instance_(instance)
		, step_controller_(step_controller)
		, started_(false)
		, hidden_window_opt_()
		, server_buffer_name_(TEXT("KnowbugServerBuffer"))
		, server_buffer_opt_()
		, server_buffer_view_opt_()
		, client_buffer_name_(TEXT("KnowbugClientBuffer"))
		, client_buffer_opt_()
		, client_buffer_view_opt_()
		, client_hwnd_opt_()
		, client_process_opt_()
		, client_thread_opt_()
		, send_queue_()
		, object_list_entity_()
	{
	}

	void start() override {
		if (std::exchange(started_, true)) {
			assert(false && u8"double start");
			return;
		}

		hidden_window_opt_ = create_hidden_window(instance_);

		server_buffer_opt_ = create_memory_mapped_file(server_buffer_name_);

		server_buffer_view_opt_ = connect_memory_mapped_file(*server_buffer_opt_);

		client_buffer_opt_ = create_memory_mapped_file(client_buffer_name_);

		client_buffer_view_opt_ = connect_memory_mapped_file(*client_buffer_opt_);

		auto pair = start_client_process(hidden_window_opt_->get());
		client_thread_opt_ = std::move(pair.first);
		client_process_opt_ = std::move(pair.second);
	}

	void will_exit() override {
		send(KMTC_SHUTDOWN);
	}

	void logmes(HspStringView text) override {
		send(KMTC_LOGMES, WPARAM{}, LPARAM{}, to_utf8(text));
	}

	void debuggee_did_stop() override {
		send_location(KMTC_STOPPED);
	}

	void client_did_hello(HWND client_hwnd) {
		if (!client_hwnd) {
			fail_with(TEXT("The client sent hwnd=NULL"));
		}

		assert(!client_hwnd_opt_);
		client_hwnd_opt_ = client_hwnd;

		send(KMTC_HELLO_OK, int{}, int{}, knowbug_version());

		// キューに溜まっていたメッセージをすべて送る。
		for (auto&& msg : send_queue_) {
			send(msg.kind(), msg.wparam(), msg.lparam(), msg.text());
		}
		send_queue_.clear();
	}

	void client_did_terminate() {
		PostQuitMessage(EXIT_SUCCESS);
	}

	void client_did_step_continue() {
		hsx::debug_do_set_mode(HSPDEBUG_RUN, debug_);
		touch_all_windows();
	}

	void client_did_step_pause() {
		hsx::debug_do_set_mode(HSPDEBUG_STOP, debug_);
		touch_all_windows();
	}

	void client_did_step_in() {
		hsx::debug_do_set_mode(HSPDEBUG_STEPIN, debug_);
		touch_all_windows();
	}

	void client_did_step_over() {
		step_controller_.update(StepControl::new_step_over());
		touch_all_windows();
	}

	void client_did_step_out() {
		step_controller_.update(StepControl::new_step_out());
		touch_all_windows();
	}

	void client_did_location_update() {
		send_location(KMTC_LOCATION);
	}

	void client_did_source(std::size_t source_file_id) {
		if (auto full_path_opt = objects().source_file_to_full_path(source_file_id)) {
			send(KMTC_SOURCE_PATH, (int)source_file_id, int{}, *full_path_opt);
		}

		if (auto content_opt = objects().source_file_to_content(source_file_id)) {
			send(KMTC_SOURCE_CODE, (int)source_file_id, int{}, *content_opt);
		}
	}

	void client_did_list_update() {
		auto diff = object_list_entity_.update(objects());

		auto string_writer = StringWriter{};
		for (auto&& delta : diff) {
			delta.write_to(string_writer);
		}
		auto text = string_writer.finish();

		send(KMTC_LIST_UPDATE_OK, int{}, int{}, text);
	}

	void client_did_list_toggle_expand(int object_id) {
		if (object_id < 0) {
			OutputDebugString(TEXT("out of range"));
			return;
		}

		object_list_entity_.toggle_expand((std::size_t)object_id);

		client_did_list_update();
	}

	void client_did_list_details(int object_id) {
		if (object_id < 0) {
			OutputDebugString(TEXT("out of range"));
			return;
		}

		auto&& path_opt = object_list_entity_.object_id_to_path((std::size_t)object_id);
		if (!path_opt) {
			// FIXME: 情報がない旨を伝える。
			send(KMTC_LIST_DETAILS_OK, int{}, int{}, as_utf8(u8""));
			return;
		}

		auto string_writer = StringWriter{};
		HspObjectWriter{ objects(), string_writer }.write_table_form(**path_opt);
		auto text = string_writer.finish();

		send(KMTC_LIST_DETAILS_OK, object_id, int{}, text);
	}

private:
	auto objects() -> HspObjects& {
		return objects_;
	}

	void send(int kind, int wparam, int lparam, Utf8StringView text) {
		if (text.size() >= MEMORY_BUFFER_SIZE) {
			// FIXME: ログ出力
			assert(false);
			return;
		}

		if (!client_hwnd_opt_ || !server_buffer_view_opt_) {
			// 後で、接続が確立したときに送る。
			send_queue_.push_back(Msg{ kind, wparam, lparam, Utf8String{ text } });
			return;
		}

		auto data = (Utf8Char*)server_buffer_view_opt_->get();
		std::memcpy(data, text.data(), text.size());
		data[text.size()] = Utf8Char{};

		SendMessage(*client_hwnd_opt_, kind, (WPARAM)wparam, (LPARAM)lparam);
	}

	void send(int kind, int wparam, int lparam) {
		send(kind, (WPARAM)wparam, (LPARAM)lparam, Utf8StringView{});
	}

	void send(int kind) {
		send(kind, WPARAM{}, LPARAM{}, Utf8StringView{});
	}

	void send_location(int kind) {
		assert(kind == KMTC_STOPPED || kind == KMTC_LOCATION);

		objects().script_do_update_location();

		auto file_id = objects().script_to_current_file().value_or(0);
		auto line_index = objects().script_to_current_line();

		send(kind, (int)file_id, (int)line_index);
	}

	void touch_all_windows() {
		auto hwnd = (HWND)debug_->hspctx->wnd_parent;
		if (!hwnd) {
			hwnd = HWND_BROADCAST;
		}

		// HACK: HSP のウィンドウに無意味なメッセージを送信することで、デバッグモードの変更に気づかせる。
		PostMessage(hwnd, WM_NULL, WPARAM{}, LPARAM{});
	}
};

auto KnowbugServer::create(HSP3DEBUG* debug, HspObjects& objects, HINSTANCE instance, KnowbugStepController& step_controller)->std::shared_ptr<KnowbugServer> {
	auto server = std::make_shared<KnowbugServerImpl>(debug, objects, instance, step_controller);
	s_server = server;
	return server;
}

// -----------------------------------------------
// グローバル
// -----------------------------------------------

static auto WINAPI process_hidden_window(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
	switch (msg) {
	case WM_CREATE:
		return TRUE;

	case WM_CLOSE:
		return FALSE;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		if (KMTS_FIRST <= msg && msg <= KMTS_LAST) {
			if (auto server = s_server.lock()) {
				switch (msg) {
				case KMTS_HELLO:
					server->client_did_hello((HWND)lp);
					break;

				case KMTS_TERMINATE:
					server->client_did_terminate();
					break;

				case KMTS_STEP_CONTINUE:
					server->client_did_step_continue();
					break;

				case KMTS_STEP_PAUSE:
					server->client_did_step_pause();
					break;

				case KMTS_STEP_IN:
					server->client_did_step_in();
					break;

				case KMTS_STEP_OVER:
					server->client_did_step_over();
					break;

				case KMTS_STEP_OUT:
					server->client_did_step_out();
					break;

				case KMTS_LOCATION_UPDATE:
					server->client_did_location_update();
					break;

				case KMTS_SOURCE:
					server->client_did_source((std::size_t)wp);
					break;

				case KMTS_LIST_UPDATE:
					server->client_did_list_update();
					break;

				case KMTS_LIST_TOGGLE_EXPAND:
					server->client_did_list_toggle_expand((int)wp);
					break;

				case KMTS_LIST_DETAILS:
					server->client_did_list_details((int)wp);
					break;

				default:
					assert(false && u8"unknown msg from the client");
					break;
				}
			}
			return TRUE;
		}
		break;
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
