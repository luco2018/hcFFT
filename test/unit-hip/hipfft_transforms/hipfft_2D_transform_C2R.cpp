/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "include/hipfft.h"
#include "../gtest/gtest.h"
#include <fftw3.h>
#include "hip/hip_runtime.h"

TEST(hipfft_2D_transform_test, func_correct_2D_transform_C2R_RTT) {
  size_t N1, N2;
  N1 = my_argc > 1 ? atoi(my_argv[1]) : 8;
  N2 = my_argc > 2 ? atoi(my_argv[2]) : 8;

  // HIPFFT work flow
  hipfftHandle plan;
  hipfftResult status = hipfftPlan2d(&plan, N1, N2, HIPFFT_R2C);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  int Rsize = N1 * N2;
  int Csize = N1 * (1 + N2 / 2);
  hipfftReal* inputR2C = (hipfftReal*)calloc(Rsize, sizeof(hipfftReal));
  hipfftComplex* outputR2C =
      (hipfftComplex*)calloc(Csize, sizeof(hipfftComplex));

  // Populate the input
  for (int i = 0; i < Rsize; i++) {
    inputR2C[i] = i % 8;
  }

  hipfftReal* devIpR2C;
  hipfftComplex* devOpR2C;
  hipMalloc(&devIpR2C, Rsize * sizeof(hipfftReal));
  hipMemcpy(devIpR2C, inputR2C, sizeof(hipfftReal) * Rsize,
            hipMemcpyHostToDevice);
  hipMalloc(&devOpR2C, Csize * sizeof(hipfftComplex));
  hipMemcpy(devOpR2C, outputR2C, sizeof(hipfftComplex) * Csize,
            hipMemcpyHostToDevice);
  status = hipfftExecR2C(plan, devIpR2C, devOpR2C);
  EXPECT_EQ(status, HIPFFT_SUCCESS);
  hipMemcpy(outputR2C, devOpR2C, sizeof(hipfftComplex) * Csize,
            hipMemcpyDeviceToHost);
  status = hipfftDestroy(plan);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  status = hipfftPlan2d(&plan, N1, N2, HIPFFT_C2R);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  hipfftComplex* inputC2R =
      (hipfftComplex*)calloc(Csize, sizeof(hipfftComplex));
  hipfftReal* outputC2R = (hipfftReal*)calloc(Rsize, sizeof(hipfftReal));

  // Populate the input of C2R with output ofo R2C
  for (int i = 0; i < Csize; i++) {
    inputC2R[i].x = outputR2C[i].x;
    inputC2R[i].y = outputR2C[i].y;
  }

  hipfftComplex* devIpC2R;
  hipfftReal* devOpC2R;
  hipMalloc(&devIpC2R, Csize * sizeof(hipfftComplex));
  hipMemcpy(devIpC2R, inputC2R, sizeof(hipfftComplex) * Csize,
            hipMemcpyHostToDevice);
  hipMalloc(&devOpC2R, Rsize * sizeof(hipfftReal));
  hipMemcpy(devOpC2R, outputC2R, sizeof(hipfftReal) * Rsize,
            hipMemcpyHostToDevice);
  status = hipfftExecC2R(plan, devIpC2R, devOpC2R);
  EXPECT_EQ(status, HIPFFT_SUCCESS);
  hipMemcpy(outputC2R, devOpC2R, sizeof(hipfftReal) * Rsize,
            hipMemcpyDeviceToHost);
  status = hipfftDestroy(plan);
  EXPECT_EQ(status, HIPFFT_SUCCESS);

  // Check Real Outputs
  for (int i = 0; i < Rsize; i++) {
    EXPECT_NEAR(outputC2R[i] / (Rsize), inputR2C[i], 1);
  }

  // Free up resources
  free(inputR2C);
  free(outputR2C);
  free(inputC2R);
  free(outputC2R);
  hipFree(devOpC2R);
  hipFree(devOpR2C);
  hipFree(devIpC2R);
  hipFree(devIpR2C);
}

