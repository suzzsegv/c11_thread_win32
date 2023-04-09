/*
 * mailbox.c - Mailbox library
 */

/*
 * Copyright (c) 2022-2023 Suzuki Satoshi
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
 * Copyright (c) 2022-2023 鈴木 聡
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

#include <stdint.h>
#include <stdio.h>
#include "threads.h"
#include "mailboxPrivate.h"


/*
 * forward declarations
 */
MboxStatus mailboxCreate(MailBox* pMbox, uint32_t maxMsgNum);
MboxStatus mailboxSend(MailBox* pMbox, void* pMsg);
MboxStatus mailboxRecv(MailBox* pMbox, void** ppMsg);


/*!
 * mailboxCreate - メッセージボックスの作成
 *
 * @param pMbox メッセージボックス インスタンス
 * @param maxMsgNum 格納可能なメッセージ数
 *
 * @return MBOX_OK or MBOX_ERROR
 */
MboxStatus mailboxCreate(MailBox* pMbox, uint32_t maxMsgNum)
{
	pMbox->ppMsgData = malloc(sizeof(void*) * maxMsgNum);
	pMbox->num = 0;
	pMbox->maxMsgNum = maxMsgNum;
	pMbox->head = 0;
	pMbox->tail = 0;
	mtx_init(&pMbox->mutex, mtx_plain);
	cnd_init(&pMbox->condRecv);

	return MBOX_OK;
}

/*!
 * mailboxSend - メッセージ送信
 *
 * @param pMbox メッセージボックス インスタンス
 * @param pMsg 送信メッセージ
 *
 * @return MBOX_OK or MBOX_ERROR
 */
MboxStatus mailboxSend(MailBox* pMbox, void* pMsg)
{
	mtx_lock(&pMbox->mutex);
	{
		if (pMbox->num >= pMbox->maxMsgNum) {
			mtx_unlock(&pMbox->mutex);
			return MBOX_ERROR;
		}
		pMbox->ppMsgData[pMbox->head] = pMsg;
		pMbox->head = (pMbox->head + 1) % pMbox->maxMsgNum;
		pMbox->num++;
	}
	mtx_unlock(&pMbox->mutex);
	cnd_signal(&pMbox->condRecv);

	return MBOX_OK;
}

/*!
 * mailboxRecv - メッセージ受信
 *
 * @param[in] pMbox メッセージボックス インスタンス
 * @param[out] ppMsg 受信メッセージ
 *
 * @return MBOX_OK or MBOX_ERROR
 */
MboxStatus mailboxRecv(MailBox* pMbox, void** ppMsg)
{
	mtx_lock(&pMbox->mutex);
	{
		while (pMbox->num == 0) {
			int rc = cnd_wait(&pMbox->condRecv, &pMbox->mutex);
			if (rc == thrd_error) {
				mtx_unlock(&pMbox->mutex);
				return MBOX_ERROR;
			}
		}
		*ppMsg = pMbox->ppMsgData[pMbox->tail];
		pMbox->tail = (pMbox->tail + 1) % pMbox->maxMsgNum;
		pMbox->num--;
	}
	mtx_unlock(&pMbox->mutex);

	return MBOX_OK;
}

