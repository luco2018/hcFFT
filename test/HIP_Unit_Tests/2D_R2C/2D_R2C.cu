#include <stdio.h>
#include <assert.h>
// CUDA runtime
#include <cuda_runtime.h>
#include <cufft.h>

#define NX 256 
#define NY 128 
#define NRANK 2 
#define BATCH 1

int main()
{
	cufftHandle plan;
	int Csize = ((NX/2) + 1) * NY;
	int Rsize = NX * NY;
	cufftReal *input = (cufftReal*)calloc(Rsize, sizeof(cufftReal));
	cufftComplex *output = (cufftComplex*)calloc(Csize, sizeof(cufftComplex));
	cufftReal *idata;
	cufftComplex *odata;
	
	int seed = 123456789;
	srand(seed);

	for(int i=0; i<Rsize; i++)
	{
		input[i] = rand();
	}

	cudaMalloc((void**)&idata, sizeof(cufftReal)*Rsize*BATCH);
	cudaMemcpy(idata, input, sizeof(cufftReal)*Rsize*BATCH, cudaMemcpyHostToDevice);
	cudaMalloc((void**)&odata, sizeof(cufftComplex)*Csize*BATCH);
	cudaMemcpy(odata, output, sizeof(cufftComplex)*Csize*BATCH, cudaMemcpyHostToDevice);	
 
	if (cudaGetLastError() != cudaSuccess)
	{ 
		fprintf(stderr, "Cuda error: Failed to allocate\n"); 
		return 0;
	} 

	/* Create a 2D FFT plan. */ 
	if (cufftPlan2d(&plan, NX, NY, CUFFT_R2C) != CUFFT_SUCCESS)
	{ 
		fprintf(stderr, "CUFFT Error: Unable to create plan\n"); 
		return 0;
	}


	if (cufftExecR2C(plan, (cufftReal*)idata, (cufftComplex*)odata) != CUFFT_SUCCESS)
	{ 
		fprintf(stderr, "CUFFT Error: Unable to execute plan\n"); 
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
