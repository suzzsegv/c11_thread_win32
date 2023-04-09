/*
 * c11_cnd_old.c - C11 standard (ISO/IEC 9899:2011): Thread support library for Win32(Old API)
 *                 Condition variable functions.
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

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "timeSpecCalcLib.h"

#include "c11_mtx.h"
#include "c11_cnd.h"


#ifndef WIN32_NATIVE_CONDITION_VARIABLE


/*
 * forward declarations
 */
int cnd_init(cnd_t* pCond);
int cnd_signal(cnd_t* pCond);
int cnd_broadcast(cnd_t* pCond);
int cnd_wait(cnd_t* pCond, mtx_t* pMutex);
int cnd_timedwait(cnd_t* pCond, mtx_t* pMutex, const struct timespec* pTimeSpec);
void cnd_destroy(cnd_t* pCond);


/*!
 * cnd_init - 条件変数の作成
 *
 * pCond が指すオブジェクトには、新たに生成された条件変数の ID が設定される。
 *
 * @param pCond 条件変数 ID の格納先オブジェクト
 *
 * @retval thrd_success 正常終了
 * @retval thrd_nomem メモリ不足
 * @retval thrd_error エラー
 */
int cnd_init(cnd_t* pCond)
{
	HANDLE hSem;
	mtx_t cnd_mtx;
	int rc;

	hSem = CreateSemaphore(
		NULL, // security
		0, // initial count
		C11_THREAD_WIN32__THREAD_NUM_MAX, // maximum count
		NULL); // name
	if (hSem == NULL) {
		printf("Win32: CreateSemaphore() error: %d\n", GetLastError());
		return thrd_error;
	}
	pCond->handle = hSem;

	rc = mtx_init(&cnd_mtx, mtx_plain);
	if (rc == thrd_error) {
		BOOL success = CloseHandle(pCond->handle);
		if (success == FALSE) {
			printf("%s(): Win32: CloseHandle() error: %d\n", __func__, GetLastError());
		}
		return thrd_error;
	}
	pCond->cnd_mtx = cnd_mtx;
	pCond->waiter_count = 0;
	pCond->lastSignalIsBroadcast = FALSE;

	return thrd_success;
}

/*!
 * cnd_signal - 条件シグナルの送信
 *
 * 条件変数で待機しているスレッドのブロックを解除する。
 * 待機しているスレッドがない場合は、何もせずに thrd_success を返す。
 *
 * @param pCond 条件変数 オブジェクト
 *
 * @return thrd_success or thrd_error
 */
int cnd_signal(cnd_t* pCond)
{
	int rc = thrd_success;

	mtx_lock(&pCond->cnd_mtx);
	do {
		if (pCond->waiter_count == 0) {
			break;
		}

		pCond->lastSignalIsBroadcast = FALSE;
		if (ReleaseSemaphore(pCond->handle, 1, NULL) == 0) {
			printf("Win32: ReleaseSemaphore() error: %d\n", GetLastError());
			rc = thrd_error;
			break;
		}
		pCond->waiter_count--;
	} while (0);
	mtx_unlock(&pCond->cnd_mtx);

	return rc;
}

/*!
 * cnd_broadcast - 条件シグナルの一斉送信
 *
 * 条件変数で待機しているすべてのスレッドのブロックを解除する。
 * 待機しているスレッドがない場合は、何もせずに、thrd_success を返す。
 *
 * @param pCond 条件変数 オブジェクト
 *
 * @return thrd_success or thrd_error
 */
int cnd_broadcast(cnd_t* pCond)
{
	int waiter_count;

	mtx_lock(&pCond->cnd_mtx);
	waiter_count = pCond->waiter_count;
	pCond->waiter_count = 0;
	mtx_unlock(&pCond->cnd_mtx);

	if (waiter_count == 0) {
		return thrd_success;
	}

	pCond->lastSignalIsBroadcast = TRUE;
	if (ReleaseSemaphore(pCond->handle, waiter_count, NULL) == 0) {
		printf("Win32: ReleaseSemaphore() error: %d\n", GetLastError());
		return thrd_error;
	}

	return thrd_success;
}

/*!
 * cnd_wait - 条件変数の待機
 *
 * cnd_wait() は、pMutex が指すミューテックスをアンロック(解除)し、cnd_signal() または、
 * cnd_broadcast() の呼び出しによって pCond が指す条件変数のシグナルが送信されるまで、
 * 呼び出し元スレッドをブロックする。
 * 条件変数によるスレッドのブロックが解除されると、戻る前に pMutex が指すミューテックスを
 * ロックする。
 * cnd_wait() は、pMutex が指すミューテックスが呼び出し元スレッドによってロックされている
 * 必要がある。
 *
 * @param pCond 条件変数 オブジェクト
 * @param pMutex ミューテックス オブジェクト
 *
 * @return thrd_success or thrd_error
 */
int cnd_wait(cnd_t* pCond, mtx_t* pMutex)
{
	int rc;

	mtx_unlock(pMutex);
	{
		mtx_lock(&pCond->cnd_mtx);
		pCond->waiter_count++;
		mtx_unlock(&pCond->cnd_mtx);

		DWORD waitResult = WaitForSingleObject(pCond->handle, INFINITE);

		// シグナルがブロードキャストの場合、他のスレッド用のセマフォを
		// 取得してしまわないように、他のスレッドに走行権を譲る.
		if (pCond->lastSignalIsBroadcast == TRUE) {
			Sleep(0);
		}
		if (waitResult == WAIT_OBJECT_0) {
			rc = thrd_success;
		} else {
			rc = thrd_error;
		}
	}
	mtx_lock(pMutex);

	return rc;
}

/*!
 * cnd_timedwait - タイムアウト付き 条件変数の待機
 * 
 * cnd_timedwait() は pMutex が指すミューテックスをアンロック(解除)し、
 * cnd_signal() または cnd_broadcast() の呼び出しによって pCond が指す条件変数のシグナルが
 * 送信されるか、pTimeoutTs が指す TIME_UTC ベースの絶対カレンダー時刻に達するまで、
 * 呼び出し元スレッドをブロックする。
 * 
 * スレッドのブロックが解除されると、関数が戻る前に pMutex が指すミューテックスを再ロックする。
 * cnd_timedwait() は、pMutex が指すミューテックスが呼び出し元スレッドによってロックされている
 * 必要がある。
 *
 * @param pCond 条件変数 オブジェクト
 * @param pMutex ミューテックス オブジェクト
 * @param pTimeoutTs タイムアウトを待つまでの絶対カレンダー時刻へのポインタ
 *
 * @retval thrd_success 正常終了
 * @retval thrd_timedout タイムアウト
 * @retval thrd_error エラー
 */
int cnd_timedwait(cnd_t* pCond, mtx_t* pMutex, const struct timespec* pTimeoutTs)
{
	struct timespec currentTs;

	if (timespec_get(&currentTs, TIME_UTC) == 0) {
		return thrd_error;
	}

	time_t timeoutMs = timespecDiffMs(pTimeoutTs, &currentTs);
	int rc;

	mtx_unlock(pMutex);
	{
		mtx_lock(&pCond->cnd_mtx);
		pCond->waiter_count++;
		mtx_unlock(&pCond->cnd_mtx);

		DWORD waitResult = WaitForSingleObject(pCond->handle, (DWORD)timeoutMs);
		switch (waitResult) {
		case WAIT_OBJECT_0:
			rc = thrd_success;
			break;
		case WAIT_TIMEOUT:
			rc = thrd_timedout;
			break;
		case WAIT_ABANDONED:
		case WAIT_FAILED:
		default:
			rc = thrd_error;
			break;
		}
	}
	mtx_lock(pMutex);

	return rc;
}

/*!
 * cnd_destroy - 条件変数の削除
 *
 * 条件変数を破棄する。
 * 待機しているスレッドがある条件変数を削除した場合、動作は未定義である。
 *
 * @param pCond 条件変数 オブジェクト
 *
 * @return なし
 */
void cnd_destroy(cnd_t* pCond)
{
	BOOL success;

	success = CloseHandle(pCond->handle);
	if (success == FALSE) {
		printf("%s(): Win32: CloseHandle() error: %d\n", __func__, GetLastError());
	}
}

#endif // WIN32_NATIVE_CONDITION_VARIABLE

