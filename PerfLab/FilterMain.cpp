#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"


Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;

    
    
//**************************************************** 
    
	//1. made local variables out of function calls and kept them out the loops
	// so that the computations would be done less frequently
	int width = (input -> width) - 1;
	int height = (input -> height) - 1;
	int filterdivisor = filter -> getDivisor();
	int result, result1, result2;
	
	//2. made local array of filter->get, so that less time would be spent going into memory to retrieve values
	int i, j;
	int filter1[3][3];
	for (i = 0; i < 3; i++){
		for (j = 0; j < 3; j++){
			filter1[i][j] = filter -> get(i, j);
		}
	}
/* OpenMP, short for Open Multi-Processing, is an Application Program Interface (API) that can be used to direct explicitly multithreaded, shared memory parallelism in applications. It is a set of standard directives and tools that are inserted into code to help guide the compiler in parallelizing the application.*/
#pragma omp parallel for  
	//3. reordered loops so that they would have better spatial locality. so it can
    //   be traversed in a more linear one demensional way
for(int plane = 0; plane < 3; plane++) {
	for(int row = 1; row < height; row++) {
		for(int col = 1; col < width; col++) {


	
		//4. urolled two loops so that there would be less overhead over iterations
            //reduces iterations
			
	     result = (input -> color[plane][row-1][col-1] * filter1[0][0] );
		 result1 = (input -> color[plane][row-1][col] * filter1[0][1] );
		 result2 = (input -> color[plane][row-1][col+1] * filter1[0][2] );
		
		 result += (input -> color[plane][row][col-1] * filter1[1][0] );
		 result1 += (input -> color[plane][row][col] * filter1[1][1] );
		 result2 += (input -> color[plane][row][col+1] * filter1[1][2] );
		
		 result += (input -> color[plane][row+1][col-1] * filter1[2][0] );
		 result1 += (input -> color[plane][row+1][col] * filter1[2][1] );
		 result2 +=(input -> color[plane][row+1][col+1] * filter1[2][2] );
		 
		 //5. used three accumulators to hold data and then combined them at the end so computations 
		 // can be done in parallel and there would be less dependency
		
	output -> color[plane][row][col] = result + result1 + result2;
	
		//6. made a for loop for divisor so division will not be done or done less frequently if the divisor is 1
        //   avoids more iterations than you have to
            
	if ( filterdivisor > 1){
	output -> color[plane][row][col] = 	
	  output -> color[plane][row][col] / filterdivisor;
	}
	
	// 7. made an if else and tried to order so that branch prediciton would be more accurate, 
	// the branch most taken comes first
	
	if ( output -> color[plane][row][col] > 0 && output -> color[plane][row][col] < 255)
		continue;
	else if ( output -> color[plane][row][col]  > 255 )
	  output -> color[plane][row][col] = 255;
	else if ( output -> color[plane][row][col]  < 0 )
	  output -> color[plane][row][col] = 0;
      }
    }
  }
//*********************************************

    
    /* 1. istead of this, we made them into local variables outside of function so that it would not have to do the computation over and over again//
/*
  for(int col = 1; col < (input -> width) - 1; col = col + 1) {
    for(int row = 1; row < (input -> height) - 1 ; row = row + 1) {
      for(int plane = 0; plane < 3; plane++) {

	output -> color[plane][row][col] = 0;



/* 2. this is where we made a local array for filter->get, so less time would be spent
going into memory to retrieve values //
//
------------------------------------------------------------------- 
// 4. unrolled these loops so there would be less overhead over iterations
	for (int j = 0; j < filter -> getSize(); j++) {
	  for (int i = 0; i < filter -> getSize(); i++) {	
	    output -> color[plane][row][col]
	      = output -> color[plane][row][col]
	      + (input -> color[plane][row + i - 1][col + j - 1] 
		 * filter -> get(i, j) );
	  }
	}
    
    
/* 7. made if elses so that the branch most taken comes first so you dont have to go through the other //
//
	output -> color[plane][row][col] = 	
	  output -> color[plane][row][col] / filter -> getDivisor();

	if ( output -> color[plane][row][col]  < 0 ) {
	  output -> color[plane][row][col] = 0;
	}

	if ( output -> color[plane][row][col]  > 255 ) { 
	  output -> color[plane][row][col] = 255;
	}
      }
    }
-------------------------------------------------------------------
  }*/

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
