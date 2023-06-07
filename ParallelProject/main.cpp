#include <iostream>
#include <math.h>
#include <stdio.h>
#include <cassert>
#include <sstream>
#include <mpi.h>
#include <stdlib.h>
#include <string.h>
#include <msclr\marshal_cppstd.h>
#include <ctime>
#include <filesystem>
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;


int* inputImage(int* w, int* h, System::String^ imagePath)
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3);

		}

	}

	delete[] Red;
	delete[] Green;
	delete[] Blue;

	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(
				image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]
			);
			MyNewImage.SetPixel(j, i, c);
		}
	}

	MyNewImage.Save("./Output/" + index + ".png");
	
	cout << "Result Image Saved " << index << endl;
}

int* convolve(int* image, int width, int height, double* kernel, int kernel_size, int* upper_padding, int* lower_padding)
{
	assert((kernel_size % 2 == 1) && "The kernel size must be odd!");

	int* result = new int[height * width]{ 0 };

	int radius = (kernel_size - 1) / 2;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			for (int i_hat = 0; i_hat < kernel_size; i_hat++)
			{
				int y = (i - radius) + i_hat;
				int* source = image;

				if (y < 0)
				{
					source = upper_padding;
					y += radius;
				}
				else if (y >= height)
				{
					source = lower_padding;
					y -= height;
				}

				for (int j_hat = 0; j_hat < kernel_size; j_hat++)
				{
					int x = (j - radius) + j_hat;

					if (x < 0 || x >= width)
						continue;

					result[i * width + j] += round(source[y * width + x] * kernel[i_hat * kernel_size + j_hat]);
				}
			}
		}
	}

	return result;
}

int seq(string img, double* kernel, int kernel_size, int index)
{
	int start_s, stop_s;

	int* input;
	int width = 0, height = 0;

	System::String^ imagePath;
	imagePath = marshal_as<System::String^>(img);

	input = inputImage(&width, &height, imagePath);

	start_s = clock();

	int radius = (kernel_size - 1) / 2;

	int* upper_padding = new int[radius * width]{ 0 };
	int* lower_padding = new int[radius * width]{ 0 };

	int* result = convolve(input, width, height, kernel, kernel_size, upper_padding, lower_padding);

	stop_s = clock();

	createImage(result, width, height, index);

	delete[] input;
	delete[] result;

	return (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
}

int par(string img, double* kernel, int kernel_size, int index)
{
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int* input;
	int width = 0, height = 0;

	int radius = (kernel_size - 1) / 2;

	int* displacements = new int[world_size];
	int* send_counts = new int[world_size];

	int start_s = 0, stop_s = 0;

	if (rank == 0)
	{
		System::String^ imagePath;
		imagePath = marshal_as<System::String^>(img);

		input = inputImage(&width, &height, imagePath);

		start_s = clock();

		int chunck_height = height / world_size;
		int chunck_remainder = height % world_size;

		for (int i = 0; i < world_size; i++)
		{
			displacements[i] = i * chunck_height * width;

			if (i == world_size - 1)
				send_counts[i] = (chunck_height + chunck_remainder) * width;
			else
				send_counts[i] = chunck_height * width;
		}
	}

	int* chunck, * result, chunck_height;

	MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);

	int* upper_padding = new int[radius * width]{ 0 };
	int* lower_padding = new int[radius * width]{ 0 };

	if (rank == 0)
	{
		chunck_height = height / world_size;

		chunck = new int[chunck_height * width];

		MPI_Scatterv(
			input, send_counts, displacements, MPI_INT, chunck, chunck_height * width, MPI_INT, 0, MPI_COMM_WORLD
		);

		MPI_Sendrecv(
			&chunck[(chunck_height - radius) * width], radius * width, MPI_INT, rank + 1, 0,
			lower_padding, radius * width, MPI_INT, rank + 1, 1,
			MPI_COMM_WORLD, MPI_STATUSES_IGNORE
		);
	}
	else if (rank == world_size - 1)
	{
		chunck_height = (height / world_size) + (height % world_size);

		chunck = new int[chunck_height * width];

		MPI_Scatterv(
			NULL, NULL, NULL, MPI_INT, chunck, chunck_height * width, MPI_INT, 0, MPI_COMM_WORLD
		);

		MPI_Sendrecv(
			chunck, radius * width, MPI_INT, rank - 1, 1,
			upper_padding, radius * width, MPI_INT, rank - 1, 0,
			MPI_COMM_WORLD, MPI_STATUSES_IGNORE
		);
	}
	else
	{
		chunck_height = height / world_size;

		chunck = new int[chunck_height * width];

		MPI_Scatterv(
			NULL, NULL, NULL, MPI_INT, chunck, chunck_height * width, MPI_INT, 0, MPI_COMM_WORLD
		);

		MPI_Sendrecv(
			&chunck[(chunck_height - radius) * width], radius * width, MPI_INT, rank + 1, 0,
			lower_padding, radius * width, MPI_INT, rank + 1, 1,
			MPI_COMM_WORLD, MPI_STATUSES_IGNORE
		);

		MPI_Sendrecv(
			chunck, radius * width, MPI_INT, rank - 1, 1,
			upper_padding, radius * width, MPI_INT, rank - 1, 0,
			MPI_COMM_WORLD, MPI_STATUSES_IGNORE
		);
	}

	result = convolve(chunck, width, chunck_height, kernel, kernel_size, upper_padding, lower_padding);

	delete[] lower_padding;
	delete[] upper_padding;

	int* output;
	if (rank == 0)
		output = new int[height * width];

	if (rank == world_size - 1)
	{
		int chunck_height = (height / world_size) + (height % world_size);

		MPI_Gatherv(
			result, chunck_height * width, MPI_INT, output, send_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD
		);
	}
	else
	{
		int chunck_height = height / world_size;

		MPI_Gatherv(
			result, chunck_height * width, MPI_INT, output, send_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD
		);
	}

	delete[] result;
	delete[] send_counts;
	delete[] displacements;

	if (rank == 0)
	{
		stop_s = clock();

		createImage(output, width, height, index);
		delete[]  input;
		delete[] output;
	}

	return (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
}

double* prompt_kernel(int& kernel_size)
{
	cout << "Enter kernel size (whole number N means kernel is NxN): ";
	cin >> kernel_size;

	double* kernel = new double[kernel_size * kernel_size];

	for (int i = 0; i < kernel_size; i++)
	{
		cout << "Enter the values of row " << i + 1 << " (seperated by spaces): ";
		for (int j = 0; j < kernel_size; j++)
			cin >> kernel[i * kernel_size + j];
	}

	return kernel;
}

void parse_args(int argc, char* argv[], int& test_num, string& k)
{
	for (int i = 1; i < argc; i += 2)
	{
		if (argv[i] == "-n"s)
			test_num = atoi(argv[i + 1]);

		if (argv[i] == "-k"s)
			k = (string)argv[i + 1];
	}
}

int main(int argc, char* argv[])
{
	MPI_Init(&argc, &argv);

	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	int TotalTime = 0, kernel_size = 3;
	int test_num = 1;
	double* kernel;

	if (rank == 0)
	{
		string k = "0";
		parse_args(argc, argv, test_num, k);

		double* low_pass = new double[kernel_size * kernel_size]{
			1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0,
			1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0,
			1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0
		};

		double* high_pass = new double[kernel_size * kernel_size]{
			0, -1, 0,
			-1, 4, -1,
			0, -1, 0
		};

		if (k == "L")
		{
			cout << "Low pass filter selected.\n";
			kernel = low_pass;
		}
		else if (k == "H")
		{
			cout << "High pass filter selected.\n";
			kernel = high_pass;
		}
		else
		{
			cout << "Custom filter selected.\n";
			kernel = prompt_kernel(kernel_size);
		}

		cout << endl;
	}

	MPI_Bcast(&test_num, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&kernel_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank != 0)
		kernel = new double[kernel_size * kernel_size];

	MPI_Bcast(kernel, kernel_size * kernel_size, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	string img;
	for (int i = 1; i <= test_num; i++)
	{
		ostringstream ss;
		ss << "./Input/" << i << ".png";
		img = ss.str();

		if (world_size == 1)
			TotalTime += seq(img, kernel, kernel_size, i);
		else
			TotalTime += par(img, kernel, kernel_size, i);
	}

	if (rank == 0)
		cout << "\nTime: " << TotalTime << endl;

	MPI_Finalize();

	return 0;
}
