// Created by Boris Vidolov on 02/15/2014
// Published under Apache 2.0 licence.
#pragma once
#include "Layer.h"
#include "File.h"

namespace FastNets
{
//This class implements a deep network through stacking layers.
//Example: 
//	Net<5, Net<3, Net<1>>> n;
//The line above creates a network with 3 layers, 5 input neurons, 3 hidden ones and 1 output.
//It is possible to stack this way to arbitrary depth:
//	Net<10, Net<9, Net<8, Net<7, Net<6, Net<5>>>>>> n5;

template<unsigned INPUT, class UpperNet = double>
class Net
{
/* Public constants */
public:
	const static unsigned Input = INPUT;
	const static unsigned Output = UpperNet::Output;
	const static bool	  Last = false;

	typedef typename UpperNet::FloatingPointType FloatingPointType;
protected:
	//Unfortunately, the stack size is limited and insufficient for really deep
	//networks. So we dynamically allocate the large data here:
	Layer<INPUT, UpperNet::Input, FloatingPointType>		mInputLayer;
	UpperNet									  			mNext;
private:
	Net(const Net&){}//No copy

/*Constructors and destructors */
public:
	Net(){}

	Net(const char* szFile)
	{
		File f(szFile, "rb");
		ReadFromFile(f);
	}

	Net(const Net& first, const Net& second, Randomizer<>& rand)
		:mInputLayer(first.mInputLayer, second.mInputLayer, rand),
		mNext(first.mNext, second.mNext, rand)
	{
	}

	void ReadFromFile(FILE* fp)
	{
		mInputLayer.ReadFromFile(fp);
		mNext.ReadFromFile(fp);
	}

	//Writes to the file. Assumes that the file pointer
	//current location is already set to the right place:
	void WriteToFile(const char* szFile)
	{
		File f(szFile, "wb");
		WriteToFile(f);
	}

	void WriteToFile(FILE* fp)
	{
		mInputLayer.WriteToFile(fp);
		mNext.WriteToFile(fp);
	}

	bool IsSame(const Net& other) const
	{
		return mInputLayer.IsSame(other.mInputLayer) && mNext.IsSame(other.mNext);
	}

	/* Processes a matrix, which rows are inputs to a matrix, which rows are the outputs. 
	"count" specifies the number of rows. */
	void BatchProcessInputSlow(FloatingPointType* input, FloatingPointType* output, unsigned count)
	{
		unsigned alignedInputSize = AVXAlign(INPUT);
		for (unsigned i = 0; i < count; ++i)
		{
			ProcessInputSlow(input + alignedInputSize*i, output + UpperNet::Output*i);
		}
	}

	double CalculateError(FloatingPointType* output, FloatingPointType* expected, unsigned count)
	{
		std::cout << "Calculating errors: ";
		Timer t;
		//TODO consider optimizing this code, if it takes considerable time
		double accum = 0;
		for (unsigned i = 0; i < count; ++i)
		{
			double localSquares = 0;
			unsigned baseIndex = i*Output;
			for (unsigned j = 0; j < Output; ++j)
			{
				unsigned index = baseIndex + j;
				double delta = expected[index] - output[index];
				localSquares += delta*delta;
			}
			localSquares /= Output;
			accum += localSquares;
		}
		accum /= count;
		return accum;
	}

	/* Forward calculation of the network. The method uses the first INPUT elements
	   of the "input" array, so the array will need to have at least as much elements.
	   The same applies to the "output" array, where the last layer of the net will
	   put its data there.*/
	void ProcessInputSlow(FloatingPointType* input, FloatingPointType* output)
	{
		if (UpperNet::Last)//Should be constant expression
		{
			//The next layer is dummy, it doesn't process. Just put the result in the "output"
			mInputLayer.ProcessInputSlow(input, output);
		}
		else
		{
			FloatingPointType intermediate[UpperNet::Input];
			mInputLayer.ProcessInputSlow(input, intermediate);
			mNext.ProcessInputSlow(intermediate, output);
		}
	}

	/* Processes a matrix, which rows are inputs to a matrix, which rows are the outputs. 
	"count" specifies the number of rows. */
	void BatchProcessInputFast(FloatingPointType* input, FloatingPointType* output, unsigned count)
	{
		unsigned alignedInputSize = AVXAlign(INPUT);
		Timer t;
		#pragma omp parallel for
		for (int i = 0; i < (int)count; ++i)
		{
			ProcessInputFast(input + alignedInputSize*i, output + UpperNet::Output*i);
		}
	}

	/* Forward calculation of the network. The method uses the first INPUT elements
	   of the "input" array, so the array will need to have at least as much elements.
	   The same applies to the "output" array, where the last layer of the net will
	   put its data there.*/
	void ProcessInputFast(FloatingPointType* input, FloatingPointType* output)
	{
		if (UpperNet::Last)//Should be constant expression
		{
			//The next layer is dummy, it doesn't process. Just put the result in the "output"
			mInputLayer.ProcessInputFast(input, output);
		}
		else
		{
			_CRT_ALIGN(32) FloatingPointType intermediate[UpperNet::Input];
			mInputLayer.ProcessInputFast(input, intermediate);
			mNext.ProcessInputFast(intermediate, output);
		}
	}

	void Mutate(double rate)
	{
		Randomizer<> rand;
		Mutate(rate, rand);
	}

	void Mutate(double rate, Randomizer<>& rand)
	{
		mInputLayer.Mutate(rate, rand);
		mNext.Mutate(rate, rand);
	}
};//Net class

//Specialization for the ending, all implementation is empty
template<unsigned INPUT>
class Net<INPUT, double>
{
/* Public constants */
public:
	const static unsigned Input = INPUT;
	const static unsigned Output = INPUT;
	const static bool Last = true;//Identifies the last (dummy) layer.

	typedef double FloatingPointType;
public:
	Net(){}
	Net(const char* szFile){}      
	Net(const Net& first, const Net& second, Randomizer<>& rand){}
	void WriteToFile(const char* szFile){}
	void WriteToFile(FILE* fp){}
	void ReadFromFile(FILE* fp){}
	bool IsSame(const Net& other) const { return true; }
	double ProcessInputSlow(FloatingPointType* input, FloatingPointType* output){ throw std::string("Execution Flow error"); }
	double ProcessInputFast(FloatingPointType* input, FloatingPointType* output){ throw std::string("Execution Flow error"); }
	void Mutate(double rate, Randomizer<>& rand){}
};

}//FastNets namespace
