/*
 * c11_thread_test.c - C11 Thread library & Message box library test module
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mailbox\mailbox.h"
#include "threads.h"
#include "timeSpecCalcLib.h"

void c11ThrdStartAndExitTest(void);
void startAndExitThreadEntry(void* arg);

void c11ThrdJoinTest(void);
void threadJoinTest_threadEntry(void* arg);

void c11CondTest(void);
void cndTimedwait_timedoutTest_threadEntry(void* arg);
void cndTimedwait_successTest_threadEntry(void* arg);
int cndBroadcast_successTest_threadEntry(void* arg);

void c11MtxTest(void);
void mtxTimedLockTest_threadEntry1(void* arg);
void mtxTryLockTest_threadEntry1(void* arg);
void userThreadEntry1(void* arg);
void userThreadEntry2(void* arg);

void mboxTest(void);

void timespecCalcLibTest(void);

static void sleepSec(int sleepSec);
static void sleepMs(int sleepMs);
static void timespecPrint(struct timespec* ts);

mtx_t gMtx;
MailBox gMailboxInst;
thrd_t gThread[2];
thrd_t gMtxTestThread[2];

cnd_t gCnd;
mtx_t gCndMtx;
thrd_t gCndTestThread[3];

void c11ThreadTest(void)
{
	thrd_lib_init();

	c11ThrdStartAndExitTest();
	c11ThrdJoinTest();
	timespecCalcLibTest();
	c11CondTest();
	c11MtxTest();
	mboxTest();

	printf("\n[Enter] key to exit.\n");
	int c = getchar();
}

/*!
 * c11ThrdStartAndExitTest
 *
 * スレッドの detatch と exit 時のリソース解放のテスト.
 * デバッガや ProcessExplorer でリソースリークがないことを確認する.
 */
void c11ThrdStartAndExitTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	int rc = thrd_create(&gThread[0], (thrd_start_t)startAndExitThreadEntry, (void*)0x01);
	if (rc == thrd_error) {
		printf("%s(): thrd_create() error: %d\n", __func__, GetLastError());
		exit(-1);
	}

	printf("=== %s(): exit.\n", __func__);
}

/*!
 * startAndExitThreadEntry
 *
 * detatch の 2 回目はエラーになることを確認.
 */
void startAndExitThreadEntry(void* arg)
{
	printf("%s(%p): start.\n", __func__, arg);

	thrd_t thr = thrd_current();
	printf("%s(): thr = %08x\n", __func__, thr);

	C11ThrdWin32* pThrdWin32 = (C11ThrdWin32*)thr;
	printf("%s(): win32 tid = %08x\n", __func__, pThrdWin32->id);

	int rc = thrd_detach(thr);
	assert(rc == thrd_success);

	rc = thrd_detach(thr);
	assert(rc == thrd_error);

	printf("%s(): exit.\n", __func__);

	thrd_exit(0x8086);
}

/*!
 * c11ThrdJoinTest
 *
 * join によるスレッドの終了待ちと終了コードの取得の確認,
 */
void c11ThrdJoinTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	int rc = thrd_create(&gThread[1], (thrd_start_t)threadJoinTest_threadEntry, (void*)0x02);
	if (rc == thrd_error) {
		printf("%s(): thrd_create() error: %d\n", __func__, GetLastError());
		exit(-1);
	}

	int thrdRc;
	rc = thrd_join(gThread[1], &thrdRc);
	assert(rc == thrd_success);
	printf("%s(): thread result code = %08x.\n", __func__, thrdRc);
	assert(thrdRc == 0x80186);

	printf("=== %s(): exit.\n", __func__);
}

/*!
 * threadJoinTest_threadEntry
 *
 * join で終了待ちするように 3 秒ウェイト.
 */
void threadJoinTest_threadEntry(void* arg)
{
	printf("%s(%p): start.\n", __func__, arg);

	thrd_t thr = thrd_current();
	printf("%s(): thr = %08x\n", __func__, thr);

	printf("%s(): sleep(3)\n", __func__);
	sleepSec(3);

	printf("%s(): exit(0x80186).\n", __func__);

	thrd_exit(0x80186);
//	return 0x80186;
}

/*!
 * c11CondTest - 条件変数のテスト
 */
void c11CondTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	int rc;
	rc = mtx_init(&gCndMtx, mtx_timed);
	printf("mtx_init(): rc = %d\n", rc);

	rc = cnd_init(&gCnd);
	printf("cnd_init(): rc = %d\n", rc);

	// cnd_timedwait() 受信: スレッド側で条件変数シグナルを受信する
	{
		printf("\n--- cnd_timedwait() recv test ---\n");
		rc = thrd_create(&gCndTestThread[1], (thrd_start_t)cndTimedwait_successTest_threadEntry, NULL);
		if (rc == thrd_error) {
			printf("%s(): thrd_create() error: %d\n", __func__, GetLastError());
			exit(-1);
		}

		sleepSec(3); // ３秒ウェイト

		rc = cnd_signal(&gCnd); // シグナル送信

		sleepSec(3); // ３秒ウェイト
	}

	// cnd_timedwait() タイムアウト: スレッド側で条件変数を 5 秒間待機、タイムアウトさせるテスト.
	{
		printf("\n--- cnd_timedwait() timeout test ---\n");
		rc = thrd_create(&gCndTestThread[0], (thrd_start_t)cndTimedwait_timedoutTest_threadEntry, NULL);

		sleepSec(6); // スレッド側がタイムアウトして終了するまでウェイト
	}

	// cnd_broadcast() 一斉送受信:
	{
		printf("\n--- cnd_broadcast() recv test ---\n");
		for (int i = 0; i < 3; i++) {
			rc = thrd_create(&gCndTestThread[i], (thrd_start_t)cndBroadcast_successTest_threadEntry, (void*)i);
			assert(rc == thrd_success);
		}

		sleepSec(3); // ３秒ウェイト

		rc = cnd_broadcast(&gCnd); // シグナル一斉送信
		assert(rc == thrd_success);

		for (int i = 0; i < 3; i++) {
			int thrdRc = thrd_error;
			rc = thrd_join(gCndTestThread[i], &thrdRc);
			assert(rc == thrd_success);
			assert(thrdRc == thrd_success);
		}
	}

	printf("=== %s(): exit.\n", __func__);
}

/*!
 * c11MtxTest - ミューテックスのテスト
 */
void c11MtxTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	int rc = mtx_init(&gMtx, mtx_timed);
	printf("mtx_init(): rc = %d\n", rc);

	// mtx_timedlock() テスト:
	// スレッド側が 5 秒間ミューテックスを取得するので、その間に 0.5 秒間
	// ミューテックスが獲得できないことを確認する.
	{
		printf("\n--- mtx_timedlock() timeout test ---\n");

		rc = thrd_create(&gMtxTestThread[0], (thrd_start_t)mtxTimedLockTest_threadEntry1, NULL);

		sleepSec(3); // スレッド側がミューテックスを取得するまでウェイト

		struct timespec startTs;
		struct timespec endTs;
		timespec_get(&startTs, TIME_UTC);
		struct timespec ts = startTs;
		timespecAddMs(&ts, 500);

		rc = mtx_timedlock(&gMtx, &ts);

		timespec_get(&endTs, TIME_UTC);
		printf("start  : ");
		timespecPrint(&startTs);
		printf("Timeout: ");
		timespecPrint(&ts);
		printf("End    : ");
		timespecPrint(&endTs);
		printf("mtx_timedlock(&gMtx): rc = %d\n", rc);
		assert(rc == thrd_timedout);

		sleepSec(3); // スレッド側が終了するまでウェイト
	}

	// mtx_trylock() テスト:
	// スレッド側が 5 秒間ミューテックスを取得するので、その間に
	// ミューテックスが獲得できないことを確認する.
	{
		printf("\n--- mtx_trylock() Test1 ---\n");

		rc = thrd_create(&gMtxTestThread[0], (thrd_start_t)mtxTryLockTest_threadEntry1, NULL);

		sleepSec(3); // スレッド側がミューテックスを取得するまでウェイト

		rc = mtx_trylock(&gMtx);
		printf("mtx_timedlock(&gMtx): rc = %d\n", rc);
		assert(rc == thrd_busy);

		printf("\n--- mtx_trylock() Test2 ---\n");
		while (1) {
			sleepMs(500);
			rc = mtx_trylock(&gMtx);
			printf("mtx_timedlock(&gMtx): rc = %d\n", rc);
			if (rc == thrd_success) {
				break;
			}
		}

		rc = mtx_unlock(&gMtx);
		assert(rc == thrd_success);
	}

	// mtx_destroy() テスト
	{
		printf("\n--- mtx_destroy() Test ---\n");
		mtx_destroy(&gMtx);
	}

	printf("=== %s(): exit.\n", __func__);
}

/*!
 * mboxTest - メールボックスのテスト
 */
void mboxTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	// msgBox テスト
	{
		int rc;

		printf("\n--- msgBox Test ---\n");

		MboxStatus mbox_status = mailboxCreate(&gMailboxInst, 8);

		rc = thrd_create(&gThread[0], (thrd_start_t)userThreadEntry1, (void*)0x1234);
		printf("thrd_create(&thread1): rc = %d, thr = %08x\n", rc, gThread[0]);

		rc = thrd_create(&gThread[1], (thrd_start_t)userThreadEntry2, (void*)0x5678);
		printf("thrd_create(&thread2): rc = %d, thr = %08x\n", rc, gThread[1]);
	}
}

/*!
 * cndTimedwait_timedoutTest_threadEntry
 *
 * 条件変数を 5 秒間待機する.
 *
 * @return なし
 */
void cndTimedwait_timedoutTest_threadEntry(void* arg)
{
	printf("%s(): Start\n", __func__);

	struct timespec startTs;
	struct timespec endTs;
	timespec_get(&startTs, TIME_UTC);
	struct timespec timeoutTs = startTs;
	timespecAddMs(&timeoutTs, 5000);

	mtx_lock(&gCndMtx);
	{
		int rc = cnd_timedwait(&gCnd, &gCndMtx, &timeoutTs);
		timespec_get(&endTs, TIME_UTC);

		printf("start  : ");
		timespecPrint(&startTs);
		printf("Timeout: ");
		timespecPrint(&timeoutTs);
		printf("End    : ");
		timespecPrint(&endTs);
		printf("cnd_timedwait(): rc = %d\n", rc);
		assert(rc == thrd_timedout);
	}
	mtx_unlock(&gCndMtx);

	thrd_t thr = thrd_current();
	thrd_detach(thr);

	printf("%s(): End\n", __func__);
}

/*!
 * cndTimedwait_successTest_threadEntry
 *
 * 条件変数を 5 秒間待機する.
 *
 * @return なし
 */
void cndTimedwait_successTest_threadEntry(void* arg)
{
	printf("%s(): Start\n", __func__);

	struct timespec startTs;
	struct timespec endTs;
	timespec_get(&startTs, TIME_UTC);
	struct timespec timeoutTs = startTs;
	timespecAddMs(&timeoutTs, 5000);

	mtx_lock(&gCndMtx);
	{
		int rc = cnd_timedwait(&gCnd, &gCndMtx, &timeoutTs);
		timespec_get(&endTs, TIME_UTC);

		printf("start  : ");
		timespecPrint(&startTs);
		printf("Timeout: ");
		timespecPrint(&timeoutTs);
		printf("End    : ");
		timespecPrint(&endTs);
		printf("cnd_timedwait(): rc = %d\n", rc);
		assert(rc == thrd_success);
	}
	mtx_unlock(&gCndMtx);

	thrd_t thr = thrd_current();
	thrd_detach(thr);

	printf("%s(): End\n", __func__);
}

/*!
 * cndBroadcast_successTest_threadEntry
 *
 * 条件変数を 5 秒間待機する.
 *
 * @return thrd_success or thrd_error
 */
int cndBroadcast_successTest_threadEntry(void* arg)
{
	int threadNum = (int)arg;
	int rc;

	printf("%s(%d): Start\n", __func__, threadNum);

	mtx_lock(&gCndMtx);
	{
		rc = cnd_wait(&gCnd, &gCndMtx);
		assert(rc == thrd_success);
	}
	mtx_unlock(&gCndMtx);

	printf("%s(%d): End\n", __func__, threadNum);

	return rc;
}

/*!
 * mtxTimedLockTest_threadEntry1
 *
 * ミューテックスをロックして 5 秒ウェイト後、アンロックする
 *
 * @return なし
 */
void mtxTimedLockTest_threadEntry1(void* arg)
{
	printf("%s(): Start\n", __func__);

	mtx_lock(&gMtx);

	sleepSec(5);

	mtx_unlock(&gMtx);

	thrd_t thr = thrd_current();
	thrd_detach(thr);

	printf("%s(): End\n", __func__);
}

/*!
 * mtxTryLockTest_threadEntry1
 *
 * ミューテックスをロックして 5 秒ウェイト後、アンロックする
 *
 * @return なし
 */
void mtxTryLockTest_threadEntry1(void* arg)
{
	printf("%s(): Start\n", __func__);

	mtx_lock(&gMtx);

	sleepSec(5);

	mtx_unlock(&gMtx);

	thrd_t thr = thrd_current();
	thrd_detach(thr);

	printf("%s(): End\n", __func__);
}

/*!
 * userThreadEntry1
 */
void userThreadEntry1(void* arg)
{
	printf("userThreadEntry1(%p):\n", arg);

#ifdef RTOS_SIMULATION_ENABLE
	thrd_priority_set(thrd_current(), thrd_priority_below_normal);
#endif

	thrd_t thr = thrd_current();
	printf("userThreadEntry1(): thr = %08x\n", thr);

	for (int i = 0; i < _countof(gThread); i++) {
		if (thrd_equal(gThread[i], thr)) {
			printf("userThreadEntry1(): gThread[%d] == thr\n", i);
		}
	}

	sleepSec(3);

	MboxStatus mbox_status;
	mbox_status = mailboxSend(&gMailboxInst, (void*)0x1111);

	mbox_status = mailboxSend(&gMailboxInst, (void*)0x2222);

	for (int i = 0; i < 8; i++) {
		mbox_status = mailboxSend(&gMailboxInst, (void*)i);
		if (mbox_status == MBOX_ERROR) {
			printf("mailboxSend(): mbox_status = %d\n", mbox_status);
		}
	}
	printf("userThreadEntry1(): exit\n");

	thrd_exit(1);
}

/*!
 * userThreadEntry2
 */
void userThreadEntry2(void* arg)
{
	printf("userThreadEntry2(%p):\n", arg);

	thrd_t thr = thrd_current();
	printf("userThreadEntry2(): thr = %08x\n", thr);

	for (int i = 0; i < _countof(gThread); i++) {
		if (thrd_equal(gThread[i], thr)) {
			printf("userThreadEntry2(): gThread[%d] == thr\n", i);
		}
	}

	MboxStatus mbox_status;
	void* pData;
	mbox_status = mailboxRecv(&gMailboxInst, &pData);
	printf("userThreadEntry2(): mailboxRecv(): mbox_status = %d, pData=%p\n", mbox_status, pData);

	sleepSec(3);

	while (1) {
		mbox_status = mailboxRecv(&gMailboxInst, &pData);
		printf("userThreadEntry2(): mailboxRecv(): mbox_status = %d, pData=%p\n", mbox_status, pData);
	}

	thrd_detach(thr);

	printf("userThreadEntry2(): exit\n");
}

/*!
 * timespecCalcLibTest
 */
void timespecCalcLibTest(void)
{
	printf("\n=== %s(): start.\n", __func__);

	struct timespec ts1 = { 0 };
	struct timespec ts2 = { 0 };
	time_t ms;

	// test0: timespecDiffMs() で正しい時間差が取得できるか目視で確認する。
	printf("--- test0 ---\n");
	(void)timespec_get(&ts1, TIME_UTC);
	struct timespec duration = { .tv_sec = 0, .tv_nsec = (1 * 1000000) };
	thrd_sleep(&duration, NULL);
	(void)timespec_get(&ts2, TIME_UTC);

	printf("ts1: ");
	timespecPrint(&ts1);
	printf("ts2: ");
	timespecPrint(&ts2);
	ms = timespecDiffMs(&ts2, &ts1);
	printf("diff msec = %lld\n", ms);

	// test1
	printf("--- test1 ---\n");
	ts1.tv_sec = 0xfffffffe;
	ts1.tv_nsec = 1;
	ts2.tv_sec = 0xffffffff;
	ts2.tv_nsec = 500000000;

	printf("ts1: ");
	timespecPrint(&ts1);
	printf("ts2: ");
	timespecPrint(&ts2);
	ms = timespecDiffMs(&ts2, &ts1);
	printf("test1 diff msec = %lld\n", ms);
	assert(ms == 1499);

	// test2
	printf("--- test2 ---\n");
	ts1.tv_sec = 123;
	ts1.tv_nsec = 999000000;
	ts2.tv_sec = 124;
	ts2.tv_nsec = 100000000;

	printf("ts1: ");
	timespecPrint(&ts1);
	printf("ts2: ");
	timespecPrint(&ts2);
	ms = timespecDiffMs(&ts2, &ts1);
	printf("test2: diff msec = %lld\n", ms);
	assert(ms == 101);

	// test2
	printf("--- test3 ---\n");
	ts1.tv_sec = 0;
	ts1.tv_nsec = 100000000;
	ts2.tv_sec = 1000000000;
	ts2.tv_nsec = 200000000;

	printf("ts1: ");
	timespecPrint(&ts1);
	printf("ts2: ");
	timespecPrint(&ts2);
	ms = timespecDiffMs(&ts2, &ts1);
	printf("test2: diff msec = %lld\n", ms);
	assert(ms == 1000000000100LL);

	printf("=== %s(): exit.\n", __func__);
}

/*!
 * sleepSec
 */
static void sleepSec(int sleepSec)
{
	struct timespec duration = { .tv_sec = sleepSec, .tv_nsec = 0 };
	thrd_sleep(&duration, NULL);
}

/*!
 * sleepMs
 */
static void sleepMs(int durationMs)
{
	struct timespec duration = { .tv_sec = 0, .tv_nsec = (durationMs * 1000000) };
	thrd_sleep(&duration, NULL);
}

/*!
 * timespecPrint
 */
static void timespecPrint(struct timespec* ts)
{
	char dateTimeStr[32];

	ctime_s(&dateTimeStr[0], sizeof(dateTimeStr), &ts->tv_sec);
	printf("%.24s, %09lu ns\n", &dateTimeStr[0], ts->tv_nsec);
}

