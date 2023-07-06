# Parallel Image Filtering

The final project for a university Parallel Programming course.

## Usage

Clone the repository into Visual Studio and make sure MPI is installed and included in the Visual Studio project.

Download the [sample images]() and put them in a folder named `Input` next to the compiled executable. Create an empty `Output` folder in the same location.

Open PowerShell in the directory of the executable and run the following
```PowerShell

```

## How it works

Each image is read and stored as a 1D array. The program receives an image, divides it into equal chunks, and distributes those chunks to the processors. The remainder of the image is always given to the last processor, making it the processing bottleneck for the convolution operation. Each processor performs convolution on its assigned chunk, and the results are combined by the master processor, which returns the final output image.

To perform a valid convolution operation, a processor needs to have the rows above and below its assigned chunk that are required to process the border regions of that chunk. Therefore, every processor communicates with the ones before and after it to exchange the required regions. The only exception is the first processor using zero padding for the upper border, and the last processor using zero padding for the lower border.
