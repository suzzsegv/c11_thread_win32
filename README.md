# C11 thread for Win32

## 概要
C言語の規格である C11(ISO/IEC 9899:2011) で追加された、マルチスレッド
ライブラリの一部を Win32 環境向けに実装したものです。  

オープンソースでいくつか同様の実装はありましたが、  

 - 商用利用にあたって条件の緩いライセンスを適用している
 - Win32 環境向けで thrd_current() や thrd_join() が実装されている

という条件にマッチした実装が見つけられなかったため、新たに実装を行いました。  


## 実装済の機能
C11 マルチスレッド ライブラリのうち、以下の機能を実装しています。

* スレッド管理(Thread functions)
	| 関数           |
	|----------------|
	| thrd_create()  |
	| thrd_current() |
	| thrd_detach()  |
	| thrd_equal()   |
	| thrd_exit()    |
	| thrd_join()    |
	| thrd_sleep()   |
	| thrd_yield()   |

* ミューテックス(Mutex functions)
	| 関数             |
	|------------------|
	| mtx_init()       |
	| mtx_lock()       |
	| mtx_timedlock()  |
	| mtx_trylock()    |
	| mtx_unlock()     |
	| mtx_destroy()    |

* 条件変数(Condition variable functions)
	| 関数             |
	|------------------|
	| cnd_init()       |
	| cnd_signal()     |
	| cnd_broadcast()  |
	| cnd_wait()       |
	| cnd_timedwait()  |
	| cnd_destroy()    |

## 未実装の機能
* 初期化関数(Initialization functions)
	|関数           |
	|---------------|
	|call_once()    |

* スレッド専用記憶領域(Thread-specific storage functions)
	|関数           |
	|---------------|
	|tss_create()   |
	|tss_set()      |
	|tss_get()      |
	|tss_delete()   |

---
## 使用方法
ライブラリ関数を使用する前に、ライブラリ初期化関数である thrd_lib_init() を、
シングルスレッドで動作している状態で１回だけ実行してください。  
使用例はテストプログラムである c11_thread_test.c を参照してください。  
また、mailbox フォルダ内に ITRON のメールボックスっぽいメッセージキューを
実装していますので、こちらも参考にしてください。  

## ビルド

Visual Studio Community 2022 用のソリューションファイル
c11_thread_win32.sln を含んでいます。
このソリューションを開き、x86 プラットフォームでビルドしてください。

---
## ソースコード ライセンス

「zlib / libpng ライセンス」を適用しています。  
本ソフトウェアの使用によって生じるいかなる損害についても、
作者は一切の責任を負いませんが、
商用アプリケーションを含めて、自由に改変して利用してください。  

---
## 変更点

### 2023/12/24

 * threads.h 内で Windows に依存するヘッダをインクルードしないように変更  
   ユーザアプリの定義と Windows の定義が衝突しないように、
   threads.h 内では Windows に依存する定義や構造体を使用しない実装に変更。

### 2023/10/05

 * Windows XP 環境向けの処理を削除
 * スレッド名の設定を行う thrd_name_set() を追加  
   独自拡張の機能で、C11 標準の機能でありません。
 * VisualStudio Community のバージョンを 2019 から 2022 に変更

### 2023/04/09

 * 1st リリース

