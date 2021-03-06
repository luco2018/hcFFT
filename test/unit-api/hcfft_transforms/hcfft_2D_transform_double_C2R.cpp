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

#include "include/hcfft.h"
#include "../gtest/gtest.h"
#include <fftw3.h>
#include <hc_am.hpp>
#include "include/hcfftlib.h"

TEST(hcfft_2D_transform_test, func_correct_2D_transform_Z2D_RTT) {
  size_t N1, N2;
  N1 = my_argc > 1 ? atoi(my_argv[1]) : 8;
  N2 = my_argc > 2 ? atoi(my_argv[2]) : 8;
  hcfftHandle plan;
  // First Real to Complex tranformation
  hcfftResult status = hcfftPlan2d(&plan, N1, N2, HCFFT_D2Z);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  int Rsize = N2 * N1;
  int Csize = N2 * (1 + N1 / 2);
  hcfftDoubleReal* inputD2Z =
      (hcfftDoubleReal*)calloc(Rsize, sizeof(hcfftDoubleReal));
  hcfftDoubleComplex* outputD2Z =
      (hcfftDoubleComplex*)calloc(Csize, sizeof(hcfftDoubleComplex));

  // Populate the input
  for (int i = 0; i < Rsize; i++) {
    inputD2Z[i] = i % 8;
  }

  std::vector<hc::accelerator> accs = hc::accelerator::get_all();
  assert(accs.size() && "Number of Accelerators == 0!");
  hc::accelerator_view accl_view = accs[1].get_default_view();
  hcfftDoubleReal* devIpD2Z =
      hc::am_alloc(Rsize * sizeof(hcfftDoubleReal), accs[1], 0);
  accl_view.copy(inputD2Z, devIpD2Z, sizeof(hcfftDoubleReal) * Rsize);
  hcfftDoubleComplex* devOpD2Z =
      hc::am_alloc(Csize * sizeof(hcfftDoubleComplex), accs[1], 0);
  accl_view.copy(outputD2Z, devOpD2Z, sizeof(hcfftDoubleComplex) * Csize);
  status = hcfftExecD2Z(plan, devIpD2Z, devOpD2Z);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  accl_view.copy(devOpD2Z, outputD2Z, sizeof(hcfftDoubleComplex) * Csize);
  status = hcfftDestroy(plan);
  EXPECT_EQ(status, HCFFT_SUCCESS);

  // Next Complex to Real transformation
  // plan = NULL;
  status = hcfftPlan2d(&plan, N1, N2, HCFFT_Z2D);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  hcfftDoubleComplex* inputZ2D =
      (hcfftDoubleComplex*)calloc(Csize, sizeof(hcfftDoubleComplex));
  hcfftDoubleReal* outputZ2D =
      (hcfftDoubleReal*)calloc(Rsize, sizeof(hcfftDoubleReal));

  // Populate the input to Z2D with output of D2Z
  for (int i = 0; i < Csize; i++) {
    inputZ2D[i].x = outputD2Z[i].x;
    inputZ2D[i].y = outputD2Z[i].y;
  }

  hcfftDoubleComplex* devIpZ2D =
      hc::am_alloc(Csize * sizeof(hcfftDoubleComplex), accs[1], 0);
  accl_view.copy(inputZ2D, devIpZ2D, sizeof(hcfftDoubleComplex) * Csize);
  hcfftDoubleReal* devOpZ2D =
      hc::am_alloc(Rsize * sizeof(hcfftDoubleReal), accs[1], 0);
  accl_view.copy(outputZ2D, devOpZ2D, sizeof(hcfftDoubleReal) * Rsize);
  status = hcfftExecZ2D(plan, devIpZ2D, devOpZ2D);
  EXPECT_EQ(status, HCFFT_SUCCESS);
  accl_view.copy(devOpZ2D, outputZ2D, sizeof(hcfftDoubleReal) * Rsize);
  status = hcfftDestroy(plan);
  // Check Real Inputs and Outputs
  for (int i = 0; i < Rsize; i++) {
    EXPECT_NEAR(inputD2Z[i], outputZ2D[i] / Rsize, 0.1);
  }
  // Free up resources
  free(inputD2Z);
  free(outputD2Z);
  free(inputZ2D);
  free(outputZ2D);
  hc::am_free(devIpZ2D);
  hc::am_free(devOpZ2D);
  hc::am_free(devIpD2Z);
  hc::am_free(devOpD2Z);
}
