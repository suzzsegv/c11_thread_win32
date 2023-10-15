/*
 * c11_cnd.c - C11 standard (ISO/IEC 9899:2011): Thread support library for Win32
 *             Condition variable functions.
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

#include <stdint.h>
#include <time.h>

#include "timeSpecCalcLib.h"

#include "c11_cnd.h"


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
	InitializeConditionVariable(&pCond->conditionVariable);

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
	WakeConditionVariable(&pCond->conditionVariable);

	return thrd_success;
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
	WakeAllConditionVariable(&pCond->conditionVariable);

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
	BOOL success;

	success = SleepConditionVariableCS(
		&pCond->conditionVariable,
		&pMutex->criticalSection,
		INFINITE);

	if (success == FALSE) {
		return thrd_error;
	}

	return thrd_success;
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
	if (timeoutMs < 0) {
		timeoutMs = 0;
	}
	BOOL success = SleepConditionVariableCS(
		&pCond->conditionVariable,
		&pMutex->criticalSection,
		(DWORD)timeoutMs);

	if (success == TRUE) {
		return thrd_success;
	}

	DWORD err = GetLastError();
	if (err == ERROR_TIMEOUT) {
		return thrd_timedout;
	}

	return thrd_error;
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
	// 何も行う必要はない.
}

