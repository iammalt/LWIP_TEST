/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/lwip_sys.h"
#include "lwip/mem.h"
#include "timer.h"

//ΪLWIP�ṩ��ʱ
extern uint32_t lwip_localtime;//lwip����ʱ�������,��λ:ms
u32_t sys_now(void){
	return lwip_localtime;
}


//����һ����Ϣ����
//*mbox:��Ϣ����
//size:�����С
//����ֵ:ERR_OK,�����ɹ�
// ����,����ʧ��
err_t sys_mbox_new( sys_mbox_t *mbox, int size)
{
(*mbox)=mymalloc(SRAMIN,sizeof(TQ_DESCR)); //Ϊ��Ϣ���������ڴ�
mymemset((*mbox),0,sizeof(TQ_DESCR)); //��� mbox ���ڴ�
if(*mbox)//�ڴ����ɹ�
{
//��Ϣ����������� MAX_QUEUE_ENTRIES ��Ϣ��Ŀ
if(size>MAX_QUEUE_ENTRIES)size=MAX_QUEUE_ENTRIES;
//ʹ�� UCOS ����һ����Ϣ����
(*mbox)->pQ=OSQCreate(&((*mbox)->pvQEntries[0]),size);
LWIP_ASSERT("OSQCreate",(*mbox)->pQ!=NULL);
//���� ERR_OK,��ʾ��Ϣ���д����ɹ� ERR_OK=0
if((*mbox)->pQ!=NULL)return ERR_OK;
else
{
myfree(SRAMIN,(*mbox));
return ERR_MEM; //��Ϣ���д�������
}
}else return ERR_MEM; //��Ϣ���д�������
}
//�ͷŲ�ɾ��һ����Ϣ����
//*mbox:Ҫɾ������Ϣ����
void sys_mbox_free(sys_mbox_t * mbox)
{
u8_t ucErr;
sys_mbox_t m_box=*mbox;
(void)OSQDel(m_box->pQ,OS_DEL_ALWAYS,&ucErr);
LWIP_ASSERT( "OSQDel ",ucErr == OS_ERR_NONE );
myfree(SRAMIN,m_box);
*mbox=NULL;
}
//����Ϣ�����з���һ����Ϣ(���뷢�ͳɹ�)
//*mbox:��Ϣ����
//*msg:Ҫ���͵���Ϣ
void sys_mbox_post(sys_mbox_t *mbox,void *msg)
{
//�� msg Ϊ��ʱ msg ���� pvNullPointer ָ���ֵ
if(msg==NULL)msg=(void*)&pvNullPointer;
while(OSQPost((*mbox)->pQ,msg)!=OS_ERR_NONE);//��ѭ���ȴ���Ϣ���ͳɹ�
}
//������һ����Ϣ���䷢����Ϣ
//�˺�������� sys_mbox_post ����ֻ����һ����Ϣ��
//����ʧ�ܺ󲻻᳢�Եڶ��η���
//*mbox:��Ϣ����
//*msg:Ҫ���͵���Ϣ
//����ֵ:ERR_OK,���� OK
// ERR_MEM,����ʧ��
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
//�� msg Ϊ��ʱ msg ���� pvNullPointer ָ���ֵ
if(msg==NULL)msg=(void*)&pvNullPointer;
if((OSQPost((*mbox)->pQ, msg))!=OS_ERR_NONE)return ERR_MEM;
return ERR_OK;
}
//�ȴ������е���Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//timeout:��ʱʱ�䣬��� timeout Ϊ 0 �Ļ�,��һֱ�ȴ�
//����ֵ:�� timeout ��Ϊ 0 ʱ����ɹ��Ļ��ͷ��صȴ���ʱ�䣬
// ʧ�ܵĻ��ͷ��س�ʱ SYS_ARCH_TIMEOUT��
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
u8_t ucErr;
u32_t ucos_timeout,timeout_new;
void *temp;
sys_mbox_t m_box=*mbox;
if(timeout!=0)
{
//ת��Ϊ������,��Ϊ UCOS ��ʱʹ�õ��ǽ�����,�� LWIP ���� ms
ucos_timeout=(timeout*OS_TICKS_PER_SEC)/1000;
if(ucos_timeout<1)ucos_timeout=1;//���� 1 ������
}else ucos_timeout = 0;
timeout = OSTimeGet(); //��ȡϵͳʱ��
//������Ϣ����,�ȴ�ʱ��Ϊ ucos_timeout
temp=OSQPend(m_box->pQ,(u16_t)ucos_timeout,&ucErr);
if(msg!=NULL)
{
//��Ϊ lwip ���Ϳ���Ϣ��ʱ������ʹ���� pvNullPointer ָ��,
//�����ж� pvNullPointer ָ���ֵ�Ϳ�֪�����󵽵���Ϣ�Ƿ���Ч
if(temp==(void*)&pvNullPointer)*msg = NULL;
else *msg=temp;
}
if(ucErr==OS_ERR_TIMEOUT)timeout=SYS_ARCH_TIMEOUT; //����ʱ
else
{
LWIP_ASSERT("OSQPend ",ucErr==OS_ERR_NONE);
timeout_new=OSTimeGet();
//���������Ϣ��ʹ�õ�ʱ��
if (timeout_new>timeout) timeout_new = timeout_new - timeout;
else timeout_new = 0xffffffff - timeout + timeout_new;
timeout=timeout_new*1000/OS_TICKS_PER_SEC + 1;
}
return timeout;
}

//���Ի�ȡ��Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//����ֵ:�ȴ���Ϣ���õ�ʱ��/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
return sys_arch_mbox_fetch(mbox,msg,1);//���Ի�ȡһ����Ϣ
}
//���һ����Ϣ�����Ƿ���Ч
//*mbox:��Ϣ����
//����ֵ:1,��Ч.
// 0,��Ч
int sys_mbox_valid(sys_mbox_t *mbox)
{
sys_mbox_t m_box=*mbox;
u8_t ucErr;
int ret;
OS_Q_DATA q_data;
memset(&q_data,0,sizeof(OS_Q_DATA));
ucErr=OSQQuery (m_box->pQ,&q_data);
ret=(ucErr<2&&(q_data.OSNMsgs<q_data.OSQSize))?1:0;
return ret;
}
//����һ����Ϣ����Ϊ��Ч
//*mbox:��Ϣ����
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
*mbox=NULL;
}



	

