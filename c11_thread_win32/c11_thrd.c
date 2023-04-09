/*
 * c11_threads.c - C11 standard (ISO/IEC 9899:2011): Thread support library for Win32
 *                 Thread functions.
 */

/*
 * Copyright (c) 2021-2023 Suzuki Satoshi
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it freely,
 * subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *        wrote the original software. If you use this software in a product, an acknowledgment
 *        in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be 
 *        misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
 */

/*
 * Copyright (c) 2021-2023 鈴木 聡
 *
 * 本ソフトウェアは「現状のまま」で、明示であるか暗黙であるかを問わず、何らの保証もなく
 * 提供されます。 本ソフトウェアの使用によって生じるいかなる損害についても、作者は一切の責任を
 * 負わないものとします。
 *
 * 以下の制限に従う限り、商用アプリケーションを含めて、本ソフトウェアを任意の目的に使用し、
 * 自由に改変して再頒布することをすべての人に許可します。
 *
 *     1. 本ソフトウェアの出自について虚偽の表示をしてはなりません。あなたがオリジナルの
 *        ソフトウェアを作成したと主張してはなりません。 あなたが本ソフトウェアを製品内で使用
 *        する場合、製品の文書に謝辞を入れていただければ幸いですが、必須ではありません。
 *
 *     2. ソースを変更した場合は、そのことを明示しなければなりません。オリジナルのソフトウェア
 *        であるという虚偽の表示をしてはなりません。
 *
 *     3. ソースの頒布物から、この表示を削除したり、表示の内容を変更したりしてはなりません。
 */

#include <windows.h>
#include <process.h>
#include <processthreadsapi.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "timeSpecCalcLib.h"
#include "c11_mtx.h"
#include "c11_thrd.h"


/*
 * static variables
 */
static DWORD c11thrdLib_tlsIndex = TLS_OUT_OF_INDEXES;


/*
 * forward declarations
 */
int thrd_lib_init(void);
int thrd_create(thrd_t* pThr, thrd_start_t pFunc, void* pArg);
thrd_t thrd_current(void);
int thrd_equal(thrd_t thr0, thrd_t thr1);
int thrd_sleep(const struct timespec* pDuration, struct timespec* pRemaining);
void thrd_yield(void);
void thrd_exit(int rc);
int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int* rc);

#ifdef RTOS_SIMULATION_ENABLE
	int thrd_priority_set(thrd_t thr, int priority);
#endif

static DWORD __stdcall threadEntryFunc(void* pArg);
static void threadExitPreproc(thrd_t thr);
static void threadResourceFree(thrd_t thr);


/*!
 * c11ThrdLib_init - C11 スレッド ライブラリの初期化
 *
 * 本スレッドライブラリを使用する場合には、ライブラリの使用前に一度だけ
 * 本関数を呼び出してください。
 *
 * @return thrd_success or thrd_error
 */
int thrd_lib_init(void)
{
	DWORD tlsIndex;

	tlsIndex = TlsAlloc(); // TLS: Thread Local Storage
	if (tlsIndex == TLS_OUT_OF_INDEXES) {
		printf("%s(): TlsAlloc() error: %d\n", __func__, GetLastError());
		return thrd_error;
	}

	c11thrdLib_tlsIndex = tlsIndex;

	return thrd_success;
}

/*!
 * thrd_create - 新しいスレッドの作成
 *
 * @param pThr 作成したスレッドのスレッド ID の格納先
 * @param pFunc スレッドのエントリ関数
 * @param pArg エントリ関数に渡される引数
 *
 * @return thrd_success or thrd_error
 *
 * @note 生成したスレッドの ID を確実に取得するため、_beginthread() ではなく
 *       _beginthreadex() を使用している。
 */
int thrd_create(thrd_t* pThr, thrd_start_t pFunc, void* pArg)
{
	HANDLE threadHandle;
	DWORD threadId;

	if (c11thrdLib_tlsIndex == TLS_OUT_OF_INDEXES) {
		printf("%s(): TLS not initialized.\n", __func__);
		return thrd_error;
	}

	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)malloc(sizeof(C11ThrdWin32));
	if (pThrdWin32 == NULL) {
		return thrd_nomem;
	}

	pThrdWin32->handle = (HANDLE)-1;
	pThrdWin32->id = -1;
	pThrdWin32->detached = false;
	pThrdWin32->pFunc = pFunc;
	pThrdWin32->pArg = pArg;

	threadHandle = (HANDLE)_beginthreadex(
		NULL, // security
		0, // stack size
		threadEntryFunc, // thread function
		(void*)pThrdWin32, // argument list
		0, // initial state
		&threadId
	);

	if (threadHandle == (HANDLE)-1) {
		printf("%s(): _beginthreadex() error: %d\n", __func__, GetLastError());
		return thrd_error;
	}

#ifdef RTOS_SIMULATION_ENABLE
	SetThreadAffinityMask(threadHandle, 1);
#endif

	pThrdWin32->handle = threadHandle;
	pThrdWin32->id = threadId;

	*pThr = (thrd_t)pThrdWin32;

	return thrd_success;
}

/*!
 * thrd_current - 現在のスレッド ID を返す
 *
 * 呼び出されたスレッドの ID を返す。
 *
 * @return スレッド ID
 *
 * @note 本実装では、スレッドローカル変数から自身のスレッド ID を取得して返す。
 */
thrd_t thrd_current(void)
{
	thrd_t thr = (thrd_t)TlsGetValue(c11thrdLib_tlsIndex);

	return thr;
}

/*!
 * thrd_equal - 2 つのスレッド ID が等しいかチェックする
 *
 * 引数で指定された 2 つのスレッド ID が、同一スレッドを示している場合は
 * 非 0 (本実装では true) を、異なるスレッドを示している場合は 0(本実装では false) を返す。
 *
 * @param thr0 比較するスレッド ID
 * @param thr1 比較するスレッド ID
 *
 * @return true or false
 */
int thrd_equal(thrd_t thr0, thrd_t thr1)
{
	if (thr0 == thr1) {
		return true;
	}

	return false;
}

/*!
 * thrd_sleep - 呼び出しスレッドを指定時間サスペンドする
 *
 * pDuration が指す TIME_UTC に基づく時間が経過するまで、呼び出し元スレッドの実行を
 * ブロックする。
 *
 * 無視できないシグナルを受信した場合、スリープは指定時間より早く復帰する。
 * その場合、pRemaining が NULL でない時には、pRemaining が指すオブジェクトに
 * 残り時間を格納する。
 *
 * @param pDuration スリープさせる時間
 * @param pRemaining シグナルにより中断された場合に、残り時間を格納する
 *                   オブジェクトへのポインタ。
 *                   NULL を指定された場合は格納しない。
 *
 * @retval 0: 指定時間満了
 * @retval -1: シグナルによる中断
 * @retval その他の負の値:エラー
 *
 * @note pDuration と pRemaining に同じオブジェクトを指定すると、シグナル後に関数を再実行する
 *       事が容易になります。
 *       実際のスリープ時間は要求された時間よりも長くなる可能性があります。
 *       これは、スリープ時間がタイマ粒度に切り上げられることと、スケジューリングと
 *       コンテキストスイッチのオーバーヘッドがあるためです。
 *       この関数は POSIX の nanosleep と同等です。
 * 
 * @attention 本実装ではシグナル受信時の残り時間の格納は未サポートです。
 */
int thrd_sleep(const struct timespec* pDuration, struct timespec* pRemaining)
{
	if (pRemaining != NULL) {
		// Not supported.
		return -2;
	}

	uint64_t msec;

	msec = pDuration->tv_sec * 1000 + pDuration->tv_nsec / 1000000;
	Sleep((DWORD)msec);

	return 0;
}

/*!
 * thrd_yield - スレッドの実行権を放棄する
 *
 * 他のスレッドを実行できるよう OS のスケジューラに対して通知を行う。
 *
 * @return なし
 */
void thrd_yield(void)
{
	Sleep(0);
}

/*!
 * thrd_exit - スレッドを終了する
 *
 * 呼び出し元スレッドの実行を終了させ、その終了コードを rc に設定する。
 *
 * @param rc スレッドの終了コード
 *
 * @return なし
 *
 * @attention 本実装では tss_create() によるデストラクタの呼び出しは
 *            未サポートです。
 */
void thrd_exit(int rc)
{
	thrd_t thr = (thrd_t)TlsGetValue(c11thrdLib_tlsIndex);

	threadExitPreproc(thr);

	_endthreadex((unsigned int)rc);
}


/*!
 * thrd_detach - スレッドを切り離す
 *
 * thr で指定されたスレッドの終了時に、そのスレッドに割り当てられたリソースを解放するように
 * 指示する。
 *
 * thrd_detach() で切り離されたスレッドに対しては、 thrd_join() を用いてスレッドの終了待ちを
 * 行うことはできない。また、同一スレッドに対して thrd_detach() を複数回呼び出すことは
 * できない。
 *
 * @param thr 切り離すスレッドのスレッド ID
 *
 * @return thrd_success or thrd_error
 */
int thrd_detach(thrd_t thr)
{
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;

	if (pThrdWin32->detached == true) {
		return thrd_error;
	}

	pThrdWin32->detached = true;

	return thrd_success;
}

/*!
 * thrd_join - スレッドの終了を待つ
 *
 * thr で指定されたスレッドが終了するまで、呼び出し元スレッドの実行をブロックする。
 * pRetCode が NULL ポインタでない場合、スレッドの終了コードを pRetCode が指す変数に格納する。
 *
 * thrd_detach() で切り離されたスレッドに対しては、 thrd_join() を用いてスレッドの終了待ちを
 * 行うことはできない。また、同一スレッドに対して thrd_join() を複数回呼び出すことはできない。
 *
 * @param thr 終了を待つスレッドのスレッド ID
 * @param pRetCode 終了したスレッドの終了コードを格納する変数へのポインタ。
 *                 NULL を指定された場合は格納しない。
 *
 * @return thrd_success or thrd_error
 *
 */
int thrd_join(thrd_t thr, int* pRetCode)
{
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;
	DWORD exitCode;
	BOOL success;

	DWORD waitResult = WaitForSingleObject(pThrdWin32->handle, INFINITE); // スレッド終了待ち
	if (waitResult != WAIT_OBJECT_0) {
		return thrd_error;
	}

	success = GetExitCodeThread(pThrdWin32->handle, &exitCode);
	if (success == FALSE) {
		printf("%s(): GetExitCodeThread() error: %d\n", __func__, GetLastError());
		return thrd_error;
	}

	threadResourceFree(thr);

	if (pRetCode != NULL) {
		*pRetCode = (int)exitCode;
	}

	return thrd_success;
}

#ifdef RTOS_SIMULATION_ENABLE
/*!
 * thrd_priority_set - スレッド優先順位の設定
 *
 * thr で指定したスレッドのスケジューリング優先順位を変更する。
 *
 * @param thr 優先順位を変更するスレッドのスレッド ID
 * @param priority スレッド優先順位(thrd_priority_* を指定)
 *
 * @return thrd_success or thrd_error
 */
int thrd_priority_set(thrd_t thr, int priority)
{
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;
	BOOL success;

	success = SetThreadPriority(pThrdWin32->handle, priority);
	if (success == FALSE) {
		printf("%s(): SetThreadPriority() error: %d\n", __func__, GetLastError());
		return thrd_error;
	}

	return thrd_success;
}
#endif

/*!
 * threadEntryFunc - スレッドのエントリー関数
 *
 * CRT _beginthreadex() に渡すスレッドのエントリ関数は __stdcall (pascal 呼出) であるため、
 * 通常の C 言語関数(cdecl 呼出)であるユーザーのスレッドエントリ関数をラップする。
 * また、ユーザースレッドの実行に対する前処理と後処理を行う。
 *
 * @param pArg スレッド ID
 *
 * @return ユーザーのスレッドエントリ関数の戻り値
 */
static DWORD __stdcall threadEntryFunc(void* pArg)
{
	thrd_t thr = (thrd_t)pArg;
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;

	TlsSetValue(c11thrdLib_tlsIndex, (LPVOID)thr);

	int rc = pThrdWin32->pFunc(pThrdWin32->pArg);

	threadExitPreproc(thr);

	return rc;
}

/*
 * threadExitPreproc - スレッドの終了前処理
 *
 * thrd_detach() により、スレッド終了時に OS リソースの開放が指示されていた場合には、
 * OS リソースの開放を行う。
 *
 * @return なし
 */
static void threadExitPreproc(thrd_t thr)
{
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;

	if (pThrdWin32->detached == true) {
		threadResourceFree(thr);
	}
}

/*
 * threadResourceFree - スレッドに関するリソースの開放
 *
 * @return なし
 */
static void threadResourceFree(thrd_t thr)
{
	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;

	BOOL success = CloseHandle(pThrdWin32->handle);
	if (success == FALSE) {
		printf("%s(): Win32: CloseHandle() error: %d\n", __func__, GetLastError());
	}
	free((void*)pThrdWin32);
}


