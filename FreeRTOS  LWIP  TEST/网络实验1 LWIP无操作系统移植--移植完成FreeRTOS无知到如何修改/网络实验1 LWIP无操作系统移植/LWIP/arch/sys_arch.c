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

//为LWIP提供计时
extern uint32_t lwip_localtime;//lwip本地时间计数器,单位:ms
u32_t sys_now(void){
	return lwip_localtime;
}


//创建一个消息邮箱
//*mbox:消息邮箱
//size:邮箱大小
//返回值:ERR_OK,创建成功
// 其他,创建失败
err_t sys_mbox_new( sys_mbox_t *mbox, int size)
{
(*mbox)=mymalloc(SRAMIN,sizeof(TQ_DESCR)); //为消息邮箱申请内存
mymemset((*mbox),0,sizeof(TQ_DESCR)); //清除 mbox 的内存
if(*mbox)//内存分配成功
{
//消息队列最多容纳 MAX_QUEUE_ENTRIES 消息数目
if(size>MAX_QUEUE_ENTRIES)size=MAX_QUEUE_ENTRIES;
//使用 UCOS 创建一个消息队列
(*mbox)->pQ=OSQCreate(&((*mbox)->pvQEntries[0]),size);
LWIP_ASSERT("OSQCreate",(*mbox)->pQ!=NULL);
//返回 ERR_OK,表示消息队列创建成功 ERR_OK=0
if((*mbox)->pQ!=NULL)return ERR_OK;
else
{
myfree(SRAMIN,(*mbox));
return ERR_MEM; //消息队列创建错误
}
}else return ERR_MEM; //消息队列创建错误
}
//释放并删除一个消息邮箱
//*mbox:要删除的消息邮箱
void sys_mbox_free(sys_mbox_t * mbox)
{
u8_t ucErr;
sys_mbox_t m_box=*mbox;
(void)OSQDel(m_box->pQ,OS_DEL_ALWAYS,&ucErr);
LWIP_ASSERT( "OSQDel ",ucErr == OS_ERR_NONE );
myfree(SRAMIN,m_box);
*mbox=NULL;
}
//向消息邮箱中发送一条消息(必须发送成功)
//*mbox:消息邮箱
//*msg:要发送的消息
void sys_mbox_post(sys_mbox_t *mbox,void *msg)
{
//当 msg 为空时 msg 等于 pvNullPointer 指向的值
if(msg==NULL)msg=(void*)&pvNullPointer;
while(OSQPost((*mbox)->pQ,msg)!=OS_ERR_NONE);//死循环等待消息发送成功
}
//尝试向一个消息邮箱发送消息
//此函数相对于 sys_mbox_post 函数只发送一次消息，
//发送失败后不会尝试第二次发送
//*mbox:消息邮箱
//*msg:要发送的消息
//返回值:ERR_OK,发送 OK
// ERR_MEM,发送失败
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
//当 msg 为空时 msg 等于 pvNullPointer 指向的值
if(msg==NULL)msg=(void*)&pvNullPointer;
if((OSQPost((*mbox)->pQ, msg))!=OS_ERR_NONE)return ERR_MEM;
return ERR_OK;
}
//等待邮箱中的消息
//*mbox:消息邮箱
//*msg:消息
//timeout:超时时间，如果 timeout 为 0 的话,就一直等待
//返回值:当 timeout 不为 0 时如果成功的话就返回等待的时间，
// 失败的话就返回超时 SYS_ARCH_TIMEOUT，
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
u8_t ucErr;
u32_t ucos_timeout,timeout_new;
void *temp;
sys_mbox_t m_box=*mbox;
if(timeout!=0)
{
//转换为节拍数,因为 UCOS 延时使用的是节拍数,而 LWIP 是用 ms
ucos_timeout=(timeout*OS_TICKS_PER_SEC)/1000;
if(ucos_timeout<1)ucos_timeout=1;//至少 1 个节拍
}else ucos_timeout = 0;
timeout = OSTimeGet(); //获取系统时间
//请求消息队列,等待时限为 ucos_timeout
temp=OSQPend(m_box->pQ,(u16_t)ucos_timeout,&ucErr);
if(msg!=NULL)
{
//因为 lwip 发送空消息的时候我们使用了 pvNullPointer 指针,
//所以判断 pvNullPointer 指向的值就可知道请求到的消息是否有效
if(temp==(void*)&pvNullPointer)*msg = NULL;
else *msg=temp;
}
if(ucErr==OS_ERR_TIMEOUT)timeout=SYS_ARCH_TIMEOUT; //请求超时
else
{
LWIP_ASSERT("OSQPend ",ucErr==OS_ERR_NONE);
timeout_new=OSTimeGet();
//算出请求消息或使用的时间
if (timeout_new>timeout) timeout_new = timeout_new - timeout;
else timeout_new = 0xffffffff - timeout + timeout_new;
timeout=timeout_new*1000/OS_TICKS_PER_SEC + 1;
}
return timeout;
}

//尝试获取消息
//*mbox:消息邮箱
//*msg:消息
//返回值:等待消息所用的时间/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
return sys_arch_mbox_fetch(mbox,msg,1);//尝试获取一个消息
}
//检查一个消息邮箱是否有效
//*mbox:消息邮箱
//返回值:1,有效.
// 0,无效
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
//设置一个消息邮箱为无效
//*mbox:消息邮箱
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
*mbox=NULL;
}



	

