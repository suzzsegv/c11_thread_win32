/*
 * c11_mtx.c - C11 standard (ISO/IEC 9899:2011): Thread support library for Win32
 *             Mutex functions.
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

#include "c11_mtx.h"


/*
 * forward declarations
 */
int mtx_init(mtx_t* pMutex, int type);
int mtx_lock(mtx_t* pMutex);
int mtx_timedlock(mtx_t* pMutex, const struct timespec* time_point);
int mtx_trylock(mtx_t* pMutex);
int mtx_unlock(mtx_t* pMutex);
void mtx_destroy(mtx_t* pMutex);


/*!
 * mtx_init - ミューテックスの作成
 *
 * 引数 type で指定したミューテックスを作成する。pMutex が指すオブジェクトには、
 * 新たに生成されたミューテックスの ID が設定される。
 *
 * 引数 type は、以下のいずれかの値でなければならない。
 *     mtx_plain - 単純な非再帰的ミューテックス
 *     mtx_timed - タイムアウトをサポートする非再帰的ミューテックス
 *     mtx_plain | mtx_recursive - 再帰的ミューテックス
 *     mtx_timed | mtx_recursive - タイムアウトをサポートする再帰的ミューテックス
 *
 * @param pMutex ミューテックス ID の格納先オブジェクト
 * @param type 作成するミューテックスの種別
 *
 * @return thrd_success or thrd_error
 */
int mtx_init(mtx_t* pMutex, int type)
{
	InitializeCriticalSection(&pMutex->criticalSection);

	return thrd_success;
}

/*!
 * mtx_lock - ミューテックス ロック（取得）
 *
 * ミューテックスをロックするまで、呼び出し元スレッドをブロックする。
 *
 * 呼び出し元のスレッドが既にミューテックスをロックしており、かつ、ミューテックスが
 * 再帰的でない場合、動作は未定義である。
 *
 * @param pMutex ミューテックス オブジェクト
 *
 * @return thrd_success or thrd_error
 */
int mtx_lock(mtx_t* pMutex)
{
	EnterCriticalSection(&pMutex->criticalSection);

	return thrd_success;
}

/*!
 * mtx_timedlock - タイムアウト付き ミューテックス ロック
 *
 * ミューテックスをロックするか、pTimeoutTs が指す TIME_UTC ベースの絶対カレンダー時刻に達する
 * まで、呼び出し元スレッドをブロックする。
 *
 * この関数は絶対時刻を取るため、経過時間が必要な場合は、経過時間に相当する絶対カレンダー時刻
 * を手動で計算する必要がある。(現在時刻は timespec_get() を用いて取得する。)
 *
 * 呼び出し元のスレッドが既にミューテックスをロックしており、かつ、ミューテックスが再帰的で
 * ない場合、動作は未定義である。
 * ミューテックスがタイムアウトをサポートしていない場合、動作は未定義である。
 *
 * @param pMutex ミューテックス オブジェクト
 * @param pTimeoutTs タイムアウトを待つまでの絶対カレンダー時刻へのポインタ
 *
 * @retval thrd_success 正常終了
 * @retval thrd_timedout タイムアウト
 * @retval thrd_error エラー
 */
int mtx_timedlock(mtx_t* pMutex, const struct timespec* pTimeoutTs)
{
	BOOL success;

	success = TryEnterCriticalSection(&pMutex->criticalSection);
	while (success == FALSE) {
		struct timespec currentTs;

		if (timespec_get(&currentTs, TIME_UTC) == 0) {
			return thrd_error;
		}
		if ((currentTs.tv_sec > pTimeoutTs->tv_sec)
			|| ((currentTs.tv_sec == pTimeoutTs->tv_sec) && (currentTs.tv_nsec > pTimeoutTs->tv_nsec))) {
			return thrd_timedout;
		}
		Sleep(0);
		success = TryEnterCriticalSection(&pMutex->criticalSection);
	}

	return thrd_success;
}

/*!
 * mtx_trylock - ミューテックスのロックを試みる
 *
 * ミューテックス ロックの成功/失敗にかかわらず、呼び出し元スレッドをブロックせずに戻る。
 *
 * @param pMutex ミューテックス オブジェクト
 *
 * @retval thrd_success 正常終了
 * @retval thrd_busy ロック失敗
 * @retval thrd_error エラー
 *
 */
int mtx_trylock(mtx_t* pMutex)
{
	BOOL success;

	success = TryEnterCriticalSection(&pMutex->criticalSection);
	if (success == FALSE) {
		return thrd_busy;
	}

	return thrd_success;
}

/*!
 * mtx_unlock - ミューテックス アンロック(解除)
 *
 * ミューテックスをアンロックする。
 * 呼び出し元スレッドが指定されたミューテックスをロックしていない場合、動作は未定義である。
 *
 * @param pMutex ミューテックス オブジェクト
 *
 * @return thrd_success or thrd_error
 *
 */
int mtx_unlock(mtx_t* pMutex)
{
	LeaveCriticalSection(&pMutex->criticalSection);

	return thrd_success;
}

/*!
 * mtx_destroy - ミューテックスの破棄
 *
 * ミューテックスを破棄する。
 * ロック待ち中のスレッドがあるミューテックスを破棄した場合、動作は未定義である。
 *
 * @param pMutex ミューテックス オブジェクト
 *
 * @return なし
 */
void mtx_destroy(mtx_t* pMutex)
{
	DeleteCriticalSection(&pMutex->criticalSection);
}

