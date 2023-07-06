# Parallel Image Filtering

The final project for a university Parallel Programming course.

## Usage

Clone the repository into Visual Studio and make sure MPI is installed and included in the Visual Studio project.

Download the [sample images](https://drive.google.com/file/d/1WWgfGED7QGlYf5isGks2_Eblu8HOZPi8/view?usp=sharing) and put them in a folder named `Input` next to the compiled executable. Create an empty `Output` folder in the same location.

Open PowerShell in the directory of the executable and run the following (replace `<compiled-executable>` with the name of your executable)
```PowerShell
mpiexec.exe <compiled-executable>
```
This will prompt you to enter a custom kernel, but you may choose a preset kernel using the `-k` option, with value `H` for the high-pass filter and value `L` for the low-pass filter.

Also the option `-n` can determine the number of images processed from the `Input` folder, given that there are at least that number of images all named as numbers starting from 1 and all are PNGs. The following is an example command
```PowerShell
mpiexec.exe <compiled-executable> -k H -n 10
```

The `mpiexec` program has a `-n` option that specifies the number of processors used
```PowerShell
mpiexec.exe -n 4 <compiled-executable> -k H -n 10
```

## How it works

Each image is read and stored as a 1D array. The program receives an image, divides it into equal chunks, and distributes those chunks to the processors. The remainder of the image is always given to the last processor, making it the processing bottleneck for the convolution operation. Each processor performs convolution on its assigned chunk, and the results are combined by the master processor, which returns the final output image.

To perform a valid convolution operation, a processor needs to have the rows above and below its assigned chunk that are required to process the border regions of that chunk. Therefore, every processor communicates with the ones before and after it to exchange the required regions. The only exception is the first processor using zero padding for the upper border, and the last processor using zero padding for the lower border.
