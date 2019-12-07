// サーバーとクライアントの通信仕様 (仮)
// <https://github.com/vain0x/knowbug/issues/81>

#ifndef WM_USER
#define WM_USER 0x400
#endif

// 1 MB
#define MEMORY_BUFFER_SIZE          0x1000000

// Knowbug window Message To the Server
#define KMTS_FIRST                  (WM_USER + 1)
// クライアントが起動したことをサーバーに伝える。
// KMTC_HELLO_OK が返ってくる。
#define KMTS_HELLO                  (WM_USER + 1)
// デバッグの終了をサーバーに要求する。
#define KMTS_TERMINATE              (WM_USER + 2)
// 実行
#define KMTS_STEP_CONTINUE          (WM_USER + 3)
// 停止
#define KMTS_STEP_PAUSE             (WM_USER + 4)
// 次へ
#define KMTS_STEP_IN                (WM_USER + 5)
// ステップオーバー
#define KMTS_STEP_OVER              (WM_USER + 6)
// ステップアウト
#define KMTS_STEP_OUT               (WM_USER + 7)
// 現在の実行位置を要求する。
#define KMTS_LOCATION_UPDATE        (WM_USER + 11)
// ソースファイルの詳細を要求する。
// KMTC_SOURCE_PATH, KMTC_SOURCE_CODE が返ってくる。
// wparam: ソースファイルID
#define KMTS_SOURCE                 (WM_USER + 21)
// オブジェクトリストを要求する。
// KMTC_LIST_UPDATE_OK が返ってくる。
#define KMTS_LIST_UPDATE            (WM_USER + 31)
// オブジェクトリストのノードを開閉する。
// KMTC_LIST_UPDATE_OK が返ってくる。
// wparam: オブジェクトID
#define KMTS_LIST_TOGGLE_EXPAND     (WM_USER + 32)
// オブジェクトリストのノードの詳細情報を要求する。
// KMTC_LIST_DETAILS_OK が返ってくる。
// wparam: オブジェクトID
#define KMTS_LIST_DETAILS           (WM_USER + 33)
#define KMTS_LAST                   (WM_USER + 999)

// Knowbug window Message To the Client
#define KMTC_FIRST                  (WM_USER + 1001)
// クライアントの起動をサーバーが確認したことを伝える。
// text: バージョンを表す文字列 (UTF-8)
#define KMTC_HELLO_OK               (WM_USER + 1001)
// クライアントを終了させる。
#define KMTC_SHUTDOWN               (WM_USER + 1002)
// logmes 命令が実行されたことを伝える。
// text: logmes の引数。(UTF-8。改行は追加されていない。)
#define KMTC_LOGMES                 (WM_USER + 1003)
// assert や stop により停止したことを伝える。
// wparam: ソースファイルID
// lparam: 行番号 (0-indexed)
#define KMTC_STOPPED                (WM_USER + 1004)
// 現在の実行位置を伝える。
// wparam: ソースファイルID
// lparam: 行番号 (0-indexed)
#define KMTC_LOCATION               (WM_USER + 1005)
// ソースファイルの絶対パスを渡す。
// wparam: ソースファイルID
// text: ソースパス (UTF-8)
#define KMTC_SOURCE_PATH            (WM_USER + 1021)
// ソースコードを渡す。
// wparam: ソースファイルID
// text: ソースコード (UTF-8)
#define KMTC_SOURCE_CODE            (WM_USER + 1022)
// オブジェクトリストの更新を返す。
// text: オブジェクトリストの差分リスト (UTF-8、改行区切り)
// 詳細はサーバー側の HspObjectListDelta を参照。
#define KMTC_LIST_UPDATE_OK         (WM_USER + 1031)
// オブジェクトリストのノードの詳細データを返す。
// wparam: オブジェクトID
// text: 詳細 (UTF-8)
#define KMTC_LIST_DETAILS_OK        (WM_USER + 1032)
#define KMTC_LAST                   (WM_USER + 1999)
