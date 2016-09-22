#include <stdio.h>
#include <assert.h>
// CUDA runtime
#include <cuda_runtime.h>
#include <cufft.h>

#define NX 128 
#define NY 128 
#define NZ 128 
#define BATCH 1

int main()
{
	cufftHandle plan;
	int Csize = ((NX/2) + 1) * NY * NZ;
	cufftComplex *input = (cufftComplex*)calloc(Csize, sizeof(cufftComplex));
	cufftComplex *output = (cufftComplex*)calloc(Csize, sizeof(cufftComplex));
	cufftComplex *idata;
	cufftComplex *odata;

	int seed = 123456789;
	srand(seed);

	for(int i=0; i<Csize; i++)
	{
		input[i].x = rand();
		input[i].y = rand();
	}

	cudaMalloc((void**)&idata, sizeof(cufftComplex)*Csize*BATCH);
	cudaMemcpy(idata, input, sizeof(cufftComplex)*Csize*BATCH, cudaMemcpyHostToDevice);
	cudaMalloc((void**)&odata, sizeof(cufftComplex)*Csize*BATCH);
	cudaMemcpy(odata, output, sizeof(cufftComplex)*Csize*BATCH, cudaMemcpyHostToDevice);

	if (cudaGetLastError() != cudaSuccess)
	{ 
		fprintf(stderr, "Cuda error: Failed to allocate\n"); 
		return 0;	
	} 

	/* Create a 3D FFT plan. */ 
	if (cufftPlan3d(&plan, NX, NY, NZ, CUFFT_C2C) != CUFFT_SUCCESS) 
	{ 
		fprintf(stderr, "CUFFT error: Plan creation failed"); 
		return 0;	
	}	

	/* Use the CUFFT plan to transform the signal in place. */ 
	if (cufftExecC2C(plan, (cufftComplex*)idata, (cufftComplex*)odata, CUFFT_FORWARD) != CUFFT_SUCCESS)
	{ 
		fprintf(stderr, "CUFFT error: ExecC2C Forward failed"); 
		return 0;	
	}
	if (cufftExecC2C(plan, (cufftComplex*)idata, (cufftComplex*)odata, CUFFT_INVERSE) != CUFFT_SUCCESS)
	{ 
		fprintf(stderr, "CUFFT error: ExecC2C Inverse failed"); 
		return 0;	
	} 
	if (cudaDeviceSynchronize() != cudaSuccess)
	{ 
		fprintf(stderr, "Cuda error: Failed to synchronize\n"); 
		return 0;	
	}	
	
	cufftDestroy(plan); 

	free(input);
	free(output);

	cudaFree(idata);
	cudaFree(odata);

	return 0;
}
