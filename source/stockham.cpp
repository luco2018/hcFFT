#include <math.h>
#include "stockham.h"
#include <list>
// FFT Stockham Autosort Method
//
//   Each pass does one digit reverse in essence. Hence by the time all passes are done, complete
//   digit reversal is done and output FFT is in correct order. Intermediate FFTs are stored in natural order,
//   which is not the case with basic Cooley-Tukey algorithm. Natural order in intermediate data makes it
//   convenient for stitching together passes with different radices.
//
//  Basic FFT algorithm:
//
//        Pass loop
//        {
//            Outer loop
//            {
//                Inner loop
//                {
//                }
//            }
//        }
//
//  The sweeps of the outer and inner loop resemble matrix indexing, this matrix changes shape with every pass as noted below
//
//   FFT pass diagram (radix 2)
//
//                k            k+R                                    k
//            * * * * * * * * * * * * * * * *                     * * * * * * * *
//            *   |             |           *                     *   |         *
//            *   |             |           *                     *   |         *
//            *   |             |           * LS        -->       *   |         *
//            *   |             |           *                     *   |         *
//            *   |             |           *                     *   |         *
//            * * * * * * * * * * * * * * * *                     *   |         *
//                         RS                                     *   |         * L
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                *   |         *
//                                                                * * * * * * * *
//                                                                       R
//
//
//    With every pass, the matrix doubles in height and halves in length
//
//
//  N = 2^T = Length of FFT
//  q = pass loop index
//  k = outer loop index = (0 ... R-1)
//  j = inner loop index = (0 ... LS-1)
//
//  Tables shows how values change as we go through the passes
//
//    q | LS   |  R   |  L  | RS
//   ___|______|______|_____|___
//    0 |  1   | N/2  |  2  | N
//    1 |  2   | N/4  |  4  | N/2
//    2 |  4   | N/8  |  8  | N/4
//    . |  .   | .    |  .  | .
//  T-1 |  N/2 | 1    |  N  | 2
//
//
//   Data Read Order
//     Radix 2: k*LS + j, (k+R)*LS + j
//     Radix 3: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j
//     Radix 4: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j, (k+3R)*LS + j
//     Radix 5: k*LS + j, (k+R)*LS + j, (k+2R)*LS + j, (k+3R)*LS + j, (k+4R)*LS + j
//
//   Data Write Order
//       Radix 2: k*L + j, k*L + j + LS
//       Radix 3: k*L + j, k*L + j + LS, k*L + j + 2*LS
//       Radix 4: k*L + j, k*L + j + LS, k*L + j + 2*LS, k*L + j + 3*LS
//       Radix 5: k*L + j, k*L + j + LS, k*L + j + 2*LS, k*L + j + 3*LS, k*L + j + 4*LS
//

namespace StockhamGenerator
{
// Experimental End ===========================================

#define RADIX_TABLE_COMMON 	{     2048,           256,             1,         4,     8, 8, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{      512,            64,             1,         3,     8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{      256,            64,             1,         4,     4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{       64,            64,             4,         3,     4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{       32,            64,            16,         2,     8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{       16,            64,            16,         2,     4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{        4,            64,            32,         2,     2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	\
							{        2,            64,            64,         1,     2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

    template <Precision PR>
	class KernelCoreSpecs
	{
		struct SpecRecord
		{
			size_t length;
			size_t workGroupSize;
			size_t numTransforms;
			size_t numPasses;
			size_t radices[12]; // Setting upper limit of number of passes to 12
		};

		typedef typename std::map<size_t, SpecRecord> SpecTable;
		SpecTable specTable;

	public:
		KernelCoreSpecs()
		{
			switch(PR)
			{
			case P_SINGLE:
				{
					SpecRecord specRecord[] = {

					RADIX_TABLE_COMMON

					//  Length, WorkGroupSize, NumTransforms, NumPasses,  Radices
					{     4096,           256,             1,         4,     8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0 },
					{     1024,           128,             1,         4,     8, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 },
					{      128,            64,             4,         3,     8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
					{        8,            64,            32,         2,     4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

					};

					size_t tableLength = sizeof(specRecord)/sizeof(specRecord[0]);
					for(size_t i=0; i<tableLength; i++) specTable[specRecord[i].length] = specRecord[i];

				} break;

			case P_DOUBLE:
				{
					SpecRecord specRecord[] = {

					RADIX_TABLE_COMMON

					//  Length, WorkGroupSize, NumTransforms, NumPasses,  Radices
					{     1024,           128,             1,         4,     8, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0 },
					//{      128,            64,             1,         7,     2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 },
					{      128,            64,             4,         3,     8, 8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
					{        8,            64,            16,         3,     2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

					};

					size_t tableLength = sizeof(specRecord)/sizeof(specRecord[0]);
					for(size_t i=0; i<tableLength; i++) specTable[specRecord[i].length] = specRecord[i];
				} break;

			default:
				assert(false);
			}
		}

		void GetRadices(size_t length, size_t &numPasses, const size_t * &pRadices) const
		{
			pRadices = NULL;
			numPasses = 0;

			typename SpecTable::const_iterator it = specTable.find(length);
			if(it != specTable.end())
			{
				pRadices = it->second.radices;
				numPasses = it->second.numPasses;
			}
		}

		void GetWGSAndNT(size_t length, size_t &workGroupSize, size_t &numTransforms) const
		{
			workGroupSize = 0;
			numTransforms = 0;

			typename SpecTable::const_iterator it = specTable.find(length);
			if(it != specTable.end())
			{
				workGroupSize = it->second.workGroupSize;
				numTransforms = it->second.numTransforms;
			}
		}
	};
}

template<>
ampfftStatus FFTPlan::GetMax1DLengthPvt<Stockham> (size_t * longest) const
{
	// TODO  The caller has already acquired the lock on *this
	//	However, we shouldn't depend on it.

	//	Query the devices in this context for their local memory sizes
	//	How large a kernel we can generate depends on the *minimum* LDS
	//	size for all devices.
	//
	const FFTEnvelope * pEnvelope = NULL;
	this->GetEnvelope (& pEnvelope);
	BUG_CHECK (NULL != pEnvelope);

	ARG_CHECK (NULL != longest)
	size_t LdsperElement = this->ElementSize();
	size_t result = pEnvelope->limit_LocalMemSize /
		(1 * LdsperElement);
	result = FloorPo2 (result);
	*longest = result;
	return AMPFFT_SUCCESS;
}

