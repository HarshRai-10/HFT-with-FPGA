/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
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
 */

#include <stdio.h>
#include <string.h>
#include "sleep.h"
#include "xparameters.h"
#include "xaxidma.h"
#include <stdio.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

#define numofdata 100

char imgBuffer[numofdata*sizeof(int)];
char procImageBuffer[512*512];
int imageSize=numofdata*sizeof(int);
int processingDone;

#define buy 1
#define sell 0

XAxiDma myDma;
XAxiDma_Config *myDmaConfig;
int status;
int image_data_len = numofdata*sizeof(int);
int idk_data_len = numofdata*(sizeof(u64));
int state = 0;

//  int inputnum[20] = {4,2,3,444,5,6,7,8,9,10,99,12,13,14,15,16,17,18,19,0};
//	int inputnum[20] = {48,22,38,444,5,6,7,8,9,10,999,12,13,4,15,146,17,128,19,0};
//	int inputnum[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//	int inputnum[40] = {48,22,38,444,5,6,7,8,9,10,999,12,13,4,15,146,17,128,19,0,48,22,38,444,5,6,7,8,9,10,999,12,13,4,15,146,17,128,19,0};
//	int inputnum[40] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
u64 processedImage[200];

int transfer_data() {
	return 0;
}

err_t send_callback(void *arg, struct tcp_pcb *tpcb,struct pbuf *p, err_t err){
	static int sentSize = 0;
	int size;
	static char *bufferPntr=procImageBuffer;

	if(processingDone){
		if(sentSize != imageSize){
			if(imageSize-sentSize >= TCP_SND_BUF)
				size = TCP_SND_BUF;
			else
				size = imageSize-sentSize;
			err = tcp_write(tpcb, (void *)bufferPntr,size, 1);
			if(err == ERR_OK){
				//printf("Sent size %d\n",sentSize);
				bufferPntr = bufferPntr+size;
				sentSize += size;
			}
		}
		else{
			printf("Processed image sent..\n");
			processingDone=0;
			sentSize = 0;
			bufferPntr=procImageBuffer;
		}
	}
	return ERR_OK;
}


void print_app_header()
{
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
#else
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
#endif
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                               struct pbuf *p, err_t err)
{
	static int i=0;
	static char* buffPntr = imgBuffer;
	/* do not read the packet if we are not in ESTABLISHED state */
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		return ERR_OK;
	}

	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);

	memcpy((void *)buffPntr,(void *)(p->payload),p->len);
	i += p->len;
	buffPntr += p->len;

	if(i==imageSize){
		status = XAxiDma_SimpleTransfer(&myDma, (UINTPTR)imgBuffer, image_data_len, XAXIDMA_DMA_TO_DEVICE);

		xil_printf("Tx Status %d, ",status);
		
		status = XAxiDma_SimpleTransfer(&myDma, (UINTPTR)processedImage, idk_data_len, XAXIDMA_DEVICE_TO_DMA);

		xil_printf("Rx Status %d, ",status);
		print("Done....");

		while (XAxiDma_Busy(&myDma, XAXIDMA_DEVICE_TO_DMA) || XAxiDma_Busy(&myDma, XAXIDMA_DMA_TO_DEVICE)) {
//			xil_printf(".");
			sleep(1);
		}
		printf("\n");
		
		for(int i=0; i<numofdata; i++) {
			int macd = processedImage[i] & 0xffffffff;
			int rsi = processedImage[i] >> 32;
			printf("%d\t %d", i, macd);
			printf("\t %d \n", rsi);

			if ((macd > 0) && (rsi <= 30) && (state == sell)) {
				printf("\t BUY!\n");
				state = buy;
			} else if ((macd < 0) && (rsi >= 70) && (state == buy)) {
				printf("\t SELL!\n");
				state = sell;
			}
		}
		
		fflush(stdout);
		i=0;
		processingDone=1;
	} else if (i < imageSize)
		xil_printf("%d %d\n\r", sizeof(int), sizeof(u64));




	//printf((char *)p->payload);

	/* echo back the payload */
	/* in this case, we assume that the payload is < TCP_SND_BUF */
	//if (tcp_sndbuf(tpcb) > p->len) {
	//	err = tcp_write(tpcb, p->payload, p->len, 1);
	//} else
	//	xil_printf("no space in tcp_sndbuf\n\r");

	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);
	tcp_sent(newpcb, send_callback);


	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);

	myDmaConfig = XAxiDma_LookupConfigBaseAddr(XPAR_AXI_DMA_0_BASEADDR);
	status = XAxiDma_CfgInitialize(&myDma, myDmaConfig);
	if(status != XST_SUCCESS){
		print("DMA initialization failed\n");
		return -1;
	}
	print("DMA initialization success..\n\r");

	return 0;
}
