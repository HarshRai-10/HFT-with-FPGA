#include "xaxidma.h"
#include "xparameters.h"
#include "sleep.h"
#include "xil_cache.h"

#define __cachePatch

XAxiDma myDma;

int main(){
#ifdef __cachePatch
	Xil_DCacheDisable();
#endif
	XAxiDma_Config *myDmaConfig;
	int status;
	int image_data_len = 20*sizeof(int);

//	int inputnum[20] = {4,2,3,444,5,6,7,8,9,10,99,12,13,14,15,16,17,18,19,0};
	int inputnum[20] = {48,22,38,444,5,6,7,8,9,10,999,12,13,4,15,146,17,128,19,0};
//	int inputnum[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int processedImage[200];

	myDmaConfig = XAxiDma_LookupConfigBaseAddr(XPAR_AXI_DMA_0_BASEADDR);
	status = XAxiDma_CfgInitialize(&myDma, myDmaConfig);
	if(status != XST_SUCCESS){
		print("DMA initialization failed\n");
		return -1;
	}
	print("DMA initialization success..\n\r");
	xil_printf("numero start address %0x\n\r",inputnum);

#ifndef __cachePatch
	Xil_DCacheFlushRange((INTPTR)inputnum, 20*sizeof(int));
#endif

	status = XAxiDma_SimpleTransfer(&myDma, (UINTPTR)processedImage, image_data_len,XAXIDMA_DEVICE_TO_DMA);

	xil_printf("Rx Status %d",status);

	status = XAxiDma_SimpleTransfer(&myDma, (UINTPTR)inputnum, image_data_len, XAXIDMA_DMA_TO_DEVICE);

	xil_printf("Tx Status %d",status);
	print("Done....\n\r");

	while (XAxiDma_Busy(&myDma, XAXIDMA_DEVICE_TO_DMA) || XAxiDma_Busy(&myDma, XAXIDMA_DMA_TO_DEVICE)) {
		xil_printf(".");
		sleep(1);
	}

#ifndef __cachePatch
	Xil_DCacheInvalidateRange((INTPTR)processedImage - 5*sizeof(int), 25*sizeof(int));
#endif

	for(int i=0; i<20; i++)
		xil_printf("%d  %d\n\r", i, processedImage[i]);
	return 0;
}
