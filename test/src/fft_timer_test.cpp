#include <stdlib.h>
#include <iostream>
#include <cstdlib>
#include "hcfftlib.h"
#include <dlfcn.h>
#include <map>
#include <hc.hpp>
#include <cfloat>
#include <stdio.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <thread>

#define PRINT 0
#define COUNT 100

using namespace hc;
using namespace std;

template <typename T>
  T average(const std::vector<std::chrono::duration<T>> &data) {
  T avg_duration = 0;
  for(auto &i : data)
    avg_duration += i.count();
  return avg_duration/data.size();
}

int main(int argc, char* argv[]) {
  std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
  std::vector<std::chrono::duration<double>> elapsed_pfe;  

  FFTPlan plan;
  const hcfftDim dimension = HCFFT_2D;
  hcfftDirection dir = HCFFT_FORWARD;
  hcfftResLocation location = HCFFT_OUTOFPLACE;
  hcfftResTransposed transposeType = HCFFT_NOTRANSPOSE;
  size_t* length = (size_t*)malloc(sizeof(size_t) * dimension);
  size_t *ipStrides = (size_t*)malloc(sizeof(size_t) * dimension);
  size_t *opStrides = (size_t*)malloc(sizeof(size_t) * dimension);

  hcfftPlanHandle planhandle;
  hcfftPrecision precision = HCFFT_SINGLE;
  size_t N1, N2;
  N1 = atoi(argv[1]);
  N2 = atoi(argv[2]);
  length[0] = N1;
  length[1] = N2;

  ipStrides[0] = 1;
  ipStrides[1] = length[0];

  opStrides[0] = 1;
  opStrides[1] = 1 + length[0]/2;

  size_t ipDistance = length[1] * length[0];
  size_t opDistance = length[1] * (1 + length[0]/2);

  int realsize, cmplexsize;
  realsize = length[0] * length[1];
  cmplexsize = length[1] * (1 + (length[0] / 2)) * 2;

  std::vector<accelerator> accs = accelerator::get_all();

#if PRINT
  std::wcout << "Size: " << accs.size() << std::endl;
  if(accs.size() == 0) {
    std::wcout << "There is no acclerator!\n";
    // Since this case is to test on GPU device, skip if there is CPU only
    return 0;
  }
  assert(accs.size() && "Number of Accelerators == 0!");
  std::for_each(accs.begin(), accs.end(), [&] (accelerator acc)
  {
    std::wcout << "New accelerator: " << acc.get_description() << std::endl;
    std::wcout << "device_path = " << acc.get_device_path() << std::endl;
    std::wcout << "version = " << (acc.get_version() >> 16) << '.' << (acc.get_version() & 0xFFFF) << std::endl;
    std::wcout << "dedicated_memory = " << acc.get_dedicated_memory() << " KB" << std::endl;
    std::wcout << "doubles = " << ((acc.get_supports_double_precision()) ? "true" : "false") << std::endl;
    std::wcout << "limited_doubles = " << ((acc.get_supports_limited_double_precision()) ? "true" : "false") << std::endl;
    std::wcout << "has_display = " << ((acc.get_has_display()) ? "true" : "false") << std::endl;
    std::wcout << "is_emulated = " << ((acc.get_is_emulated()) ? "true" : "false") << std::endl;
    std::wcout << "is_debug = " << ((acc.get_is_debug()) ? "true" : "false") << std::endl;
    std::cout << std::endl;
  });
#endif

  // Initialize host variables ----------------------------------------------
  float* ipHost = (float*)calloc(realsize, sizeof(float));
  float* ipzHost = (float*)calloc(realsize, sizeof(float));
  float* opHost = (float*)calloc(cmplexsize, sizeof(float));

  for(int  i = 0; i < N2 ; i++) {
    for(int  j = 0; j < N1 ; j++) {
      ipHost[i * N1 + j] = i * N1 + j + 1;
    }
  }

  float* ipDev = (float*)am_alloc(realsize * sizeof(float), accs[1], 0);
  float* ipzDev = (float*)am_alloc(realsize * sizeof(float), accs[1], 0);
  float* opDev = (float*)am_alloc(cmplexsize * sizeof(float), accs[1], 0);

  // Copy input contents to device from host
  hc::am_copy(ipDev, ipHost, realsize * sizeof(float));
  hc::am_copy(ipzDev, ipzHost, realsize * sizeof(float));
  hc::am_copy(opDev, opHost, cmplexsize * sizeof(float));

  hcfftLibType libtype = HCFFT_R2CD2Z;

  hcfftStatus status = plan.hcfftCreateDefaultPlan (&planhandle, dimension, length, dir, precision, libtype);
  if(status != HCFFT_SUCCEEDS) {
    cout << " Create plan error " << endl;
  }

  status = plan.hcfftSetAcclView(planhandle, accs[1].create_view());
  if(status != HCFFT_SUCCEEDS) {
    cout << " set accleration view error " << endl;
  }

  status = plan.hcfftSetPlanPrecision(planhandle, precision);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set plan error " << endl;
  }

  status = plan.hcfftSetPlanTransposeResult(planhandle, transposeType);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set hcfftSetPlanTransposeResult error " << endl;
  }

  status = plan.hcfftSetResultLocation(planhandle, HCFFT_OUTOFPLACE);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set result error " << endl;
  }

  status = plan.hcfftSetPlanInStride(planhandle, dimension, ipStrides );

  if(status != HCFFT_SUCCEEDS) {
    cout<<" hcfftSetPlanInStride error "<<endl;
  }

  status = plan.hcfftSetPlanOutStride(planhandle, dimension, opStrides );

  if(status != HCFFT_SUCCEEDS) {
    cout<<"hcfftSetPlanOutStride error "<<endl;
  }

  status = plan.hcfftSetPlanDistance(planhandle, ipDistance, opDistance );

  if(status != HCFFT_SUCCEEDS) {
    cout<<"hcfftSetPlanDistance error "<<endl;
  }

  /*---------------------R2C--------------------------------------*/
  status = plan.hcfftSetLayout(planhandle, HCFFT_REAL, HCFFT_HERMITIAN_INTERLEAVED);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set layout error " << endl;
  }

  status = plan.hcfftBakePlan(planhandle);

  if(status != HCFFT_SUCCEEDS) {
    cout << " bake plan error " << endl;
  }

  for(int i = 0; i < COUNT; i++)
  {
  start = std::chrono::high_resolution_clock::now();

  status = plan.hcfftEnqueueTransform(planhandle, dir, ipDev, opDev, NULL);
  if(status != HCFFT_SUCCEEDS) {
    cout << " Transform error " << endl;
  }

  end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = end - start;
  if(i != 0)
    elapsed_pfe.push_back(dur);
  }

#if PRINT
  std::cout << " R2C Done " << std::endl;
#endif
  double Avg_time = average(elapsed_pfe);
  double time_in_ms = Avg_time * 1e3;
  cout << "FFT Kernel execution time R2C Transform for size "<< N1 << "x" << N2 << " in <ms>:" << time_in_ms <<endl;

  // Copy Device output  contents back to host
  hc::am_copy(opHost, opDev, cmplexsize * sizeof(float));

  status = plan.hcfftDestroyPlan(&planhandle);
  if(status != HCFFT_SUCCEEDS) {
    cout << " destroy plan error " << endl;
  }

#if PRINT

  printf("r2c\n");

  /* Print Output */
    for(int  i = 0; i < N2 ; i++) {
      for(int  j = 0; j < (N1 / 2 + 1) * 2; j++) {
        printf("%lf\n", opHost[i * (N1 / 2 + 1) * 2 + j]);
      }
    }

#endif
  /*---------------------C2R---------------------------------------*/

  FFTPlan plan1;
  libtype = HCFFT_C2RZ2D;
  dir = HCFFT_BACKWARD;

  ipStrides[0] = 1;
  ipStrides[1] = 1 + length[0]/2;

  opStrides[0] = 1;
  opStrides[1] = length[0];

  ipDistance = length[1] * (1 + length[0]/2);
  opDistance = length[0] * length[1];

  status = plan1.hcfftCreateDefaultPlan (&planhandle, dimension, length, dir, precision, libtype);
  if(status != HCFFT_SUCCEEDS) {
    cout << " Create plan error " << endl;
  }

  status = plan1.hcfftSetAcclView(planhandle, accs[1].create_view());
  if(status != HCFFT_SUCCEEDS) {
    cout << " set accleration view error " << endl;
  }

  status = plan1.hcfftSetPlanPrecision(planhandle, precision);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set plan error " << endl;
  }

  status = plan1.hcfftSetPlanTransposeResult(planhandle, transposeType);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set hcfftSetPlanTransposeResult error " << endl;
  }

  status = plan1.hcfftSetResultLocation(planhandle, HCFFT_OUTOFPLACE);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set result error " << endl;
  }

  status = plan1.hcfftSetLayout(planhandle, HCFFT_HERMITIAN_INTERLEAVED, HCFFT_REAL);

  if(status != HCFFT_SUCCEEDS) {
    cout << " set layout error " << endl;
  }

  status = plan1.hcfftSetPlanInStride(planhandle, dimension, ipStrides );

  if(status != HCFFT_SUCCEEDS) {
    cout<<" hcfftSetPlanInStride error "<<endl;
  }

  status = plan1.hcfftSetPlanOutStride(planhandle, dimension, opStrides );

  if(status != HCFFT_SUCCEEDS) {
    cout<<"hcfftSetPlanOutStride error "<<endl;
  }

  status = plan1.hcfftSetPlanDistance(planhandle, ipDistance, opDistance );

  if(status != HCFFT_SUCCEEDS) {
    cout<<"hcfftSetPlanDistance error "<<endl;
  }

  status = plan1.hcfftSetPlanScale(planhandle, dir, 1.0 );
  if( status != HCFFT_SUCCEEDS) {
    cout << " setplan scale error " << endl;;
  }

  status = plan1.hcfftBakePlan(planhandle);
  if(status != HCFFT_SUCCEEDS) {
    cout << " bake plan error " << endl;
  }

  elapsed_pfe.clear();

  for(int i = 0; i < COUNT; i++)
  {
  start = std::chrono::high_resolution_clock::now();

  status = plan1.hcfftEnqueueTransform(planhandle, dir, opDev, ipzDev, NULL);
  if(status != HCFFT_SUCCEEDS) {
    cout << " Transform error " << endl;
  }

  end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> dur = end - start;
  if(i != 0)
    elapsed_pfe.push_back(dur);
  }

#if PRINT
  std::cout << " C2R done " << std::endl;
#endif

  Avg_time = average(elapsed_pfe);
  time_in_ms = Avg_time * 1e3;
  cout << "FFT Kernel execution time C2R Transform for size "<< N1 << "x" << N2 << " in <ms>:" << time_in_ms <<endl;

  // Copy Device output  contents back to host
  hc::am_copy(ipzHost, ipzDev, realsize * sizeof(float));

#if PRINT

  /* Print Output */
  for(int  i = 0; i < N2 ; i++) {
    for(int  j = 0; j < N1 ; j++) {
      std::cout << " ipzHost[" << i * N1 + j << "] " << ipzHost[i * N1 + j] << std::endl;
    }
  }

  std::cout <<  " Comparing results " << std::endl;
  for(int  i = 0; i < N2 ; i++) {
    for(int  j = 0; j < N1 ; j++) {
    if((round(ipzHost[i * N1 + j]) != (ipHost[i * N1 + j]) * N1 * N2) || isnan(ipzHost[i * N1 + j])) {
      cout << " Mismatch at  " << i * N1 + j << " input " << ipHost[i * N1 + j] << " amp " << round(ipzHost[i * N1 + j]) << endl;
      cout << " TEST FAILED " << std::endl;
      exit(0);
    }
    }
  }

  cout << " TEST PASSED " << std::endl;
#endif

  status = plan1.hcfftDestroyPlan(&planhandle);
  if(status != HCFFT_SUCCEEDS) {
    cout << " destroy plan error " << endl;
  }

  hc::am_free(ipDev);
  hc::am_free(ipzDev);
  hc::am_free(opDev);

  free(ipHost);
  free(ipzHost);
  free(opHost);
}