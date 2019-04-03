/* Example sobel code for ECE574 -- Spring 2019 */
/* By Vince Weaver <vincent.weaver@maine.edu> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

#include <jpeglib.h>

#include <cuda.h>

#include <papi.h>

/* Filters */

/* Structure describing the image */
struct image_t {
	int x;
	int y;
	int depth;	/* bytes */
	unsigned char *pixels;
};

struct convolve_data_t {
	struct image_t *old;
	struct image_t *newt;
	int (*filter)[3][3];
	int ystart;
	int yend;
};

//global for CUDA function
__global__
void combine(unsigned char *x, unsigned char *y, unsigned char *dev_new, int imageSize) {
	int i = blockIdx.x * blockDim.x+threadIdx.x; //ditermins the pixels to be operated on
	int out;

	if (i < imageSize) {
		out=sqrt((double)((x[i]*x[i]) + (y[i]*y[i]))); //double to make nvcc happy
		if (out>255) out=255;
		if (out<0) out=0;
		dev_new[i]=out;
	}

	return; 
}

static int load_jpeg(char *filename, struct image_t *image) {

	FILE *fff;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW output_data;
	unsigned int scanline_len;
	int scanline_count=0;

	fff=fopen(filename,"rb");
	if (fff==NULL) {
		fprintf(stderr, "Could not load %s: %s\n",
			filename, strerror(errno));
		return -1;
	}

	/* set up jpeg error routines */
	cinfo.err = jpeg_std_error(&jerr);

	/* Initialize cinfo */
	jpeg_create_decompress(&cinfo);

	/* Set input file */
	jpeg_stdio_src(&cinfo, fff);

	/* read header */
	jpeg_read_header(&cinfo, TRUE);

	/* Start decompressor */
	jpeg_start_decompress(&cinfo);

	printf("output_width=%d, output_height=%d, output_components=%d\n",
		cinfo.output_width,
		cinfo.output_height,
		cinfo.output_components);

	image->x=cinfo.output_width;
	image->y=cinfo.output_height;
	image->depth=cinfo.output_components;

	scanline_len = cinfo.output_width * cinfo.output_components;
	image->pixels=(unsigned char *)malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);

	while (scanline_count < cinfo.output_height) {
		output_data = (image->pixels + (scanline_count * scanline_len));
		jpeg_read_scanlines(&cinfo, &output_data, 1);
		scanline_count++;
	}

	/* Finish decompressing */
	jpeg_finish_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	fclose(fff);

	return 0;
}

static int store_jpeg(const char *filename, struct image_t *image) {

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int quality=90; /* % */
	int i;

	FILE *fff;

	JSAMPROW row_pointer[1];
	int row_stride;

	/* setup error handler */
	cinfo.err = jpeg_std_error(&jerr);

	/* initialize jpeg compression object */
	jpeg_create_compress(&cinfo);

	/* Open file */
	fff = fopen(filename, "wb");
	if (fff==NULL) {
		fprintf(stderr, "can't open %s: %s\n",
			filename,strerror(errno));
		return -1;
	}

	jpeg_stdio_dest(&cinfo, fff);

	/* Set compression parameters */
	cinfo.image_width = image->x;
	cinfo.image_height = image->y;
	cinfo.input_components = image->depth;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	/* start compressing */
	jpeg_start_compress(&cinfo, TRUE);

	row_stride=image->x*image->depth;

	for(i=0;i<image->y;i++) {
		row_pointer[0] = & image->pixels[i * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	/* finish compressing */
	jpeg_finish_compress(&cinfo);

	/* close file */
	fclose(fff);

	/* clean up */
	jpeg_destroy_compress(&cinfo);

	return 0;
}

__global__
void cuda_generic_convolve (int imageSize, unsigned char *in, int *matrix, unsigned char *out, int xsize, int depth) {
	int i = blockIdx.x*blockDim.x+threadIdx.x;
	int sum = 0;
	if((i%(xsize*depth) >= depth) && (i%(xsize*depth) <= (xsize*depth-depth-1)) && (i >= (xsize*depth+depth)) && (i <= (imageSize-xsize*depth-depth-1)) && (i < imageSize)) {
		sum+=in[i-3-(xsize*depth)]*matrix[0];
		sum+=in[i-(xsize*depth)]*matrix[1];
		sum+=in[i+3-(xsize*depth)]*matrix[2];
		sum+=in[i-3]*matrix[3];
		sum+=in[i]*matrix[4];
		sum+=in[i+3]*matrix[5];
		sum+=in[i-3+(xsize*depth)]*matrix[6];
		sum+=in[i+(xsize*depth)]*matrix[7];
		sum+=in[i+3+(xsize*depth)]*matrix[8];

		if (sum<0) sum=0;
		if (sum>255) sum=255;

		out[i] = sum;
	}
		return;
}

int main(int argc, char **argv) {

	struct image_t image,sobel_x,sobel_y,new_image; 
	long long start_time,load_time,convolve_time;
	long long combine_after=0,combine_before=0;
	long long copy_before=0,copy_after=0,copy2_before=0,copy2_after=0;
	long long store_after,store_before;

	/* Check command line usage */
	if (argc<2) {
		fprintf(stderr,"Usage: %s image_file\n",argv[0]);
		return -1;
	}

	PAPI_library_init(PAPI_VER_CURRENT);

	start_time=PAPI_get_real_usec();

	/* Load an image */
	load_jpeg(argv[1],&image);

	load_time=PAPI_get_real_usec();

	/* Allocate space for output image */
	new_image.x=image.x;
	new_image.y=image.y;
	new_image.depth=image.depth;
	new_image.pixels=(unsigned char *)calloc(image.x*image.y*image.depth,sizeof(char));

	/* Allocate space for output image */
	sobel_x.x=image.x;
	sobel_x.y=image.y;
	sobel_x.depth=image.depth;
	sobel_x.pixels=(unsigned char *)calloc(image.x*image.y*image.depth,sizeof(char));

	/* Allocate space for output image */
	sobel_y.x=image.x;
	sobel_y.y=image.y;
	sobel_y.depth=image.depth;
	sobel_y.pixels=(unsigned char *)calloc(image.x*image.y*image.depth,sizeof(char));

	//Allocating GPU memory
	unsigned char *dev_in_y, *dev_out_x, *dev_out_y, *dev_in_x;
	int imageSize = image.y * image.x * image.depth, *dev_xFilter, *dev_yFilter;
	int xFilter[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
	int yFilter[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
	cudaMalloc((void **)&dev_in_y,(imageSize*sizeof(char)));
	cudaMalloc((void **)&dev_out_y,(imageSize*sizeof(char)));
	cudaMalloc((void **)&dev_in_x,(imageSize*sizeof(char)));
	cudaMalloc((void **)&dev_out_x,(imageSize*sizeof(char)));
	cudaMalloc((void **)&dev_xFilter,(9*sizeof(int)));
	cudaMalloc((void **)&dev_yFilter,(9*sizeof(int)));

	//copying memory to GPU
	cudaMemcpy(dev_in_y, image.pixels, (imageSize*sizeof(char)),cudaMemcpyHostToDevice);
	cudaMemcpy(dev_in_x, image.pixels, (imageSize*sizeof(char)),cudaMemcpyHostToDevice);
	cudaMemcpy(dev_yFilter, yFilter, (9*sizeof(int)),cudaMemcpyHostToDevice);
	cudaMemcpy(dev_xFilter, xFilter, (9*sizeof(int)),cudaMemcpyHostToDevice);

	//convolving
	cuda_generic_convolve<<<(imageSize+255)/256,256>>>(imageSize, dev_in_y, dev_yFilter, dev_out_y, image.x, image.depth);
	cudaDeviceSynchronize();
	cuda_generic_convolve<<<(imageSize+255)/256,256>>>(imageSize, dev_in_x, dev_xFilter, dev_out_x, image.x, image.depth);

	// wait for convolves to finish
	cudaDeviceSynchronize();
	//check for errors
	cudaError_t error = cudaGetLastError();
	if( error != cudaSuccess) {
		printf("%s Big oof!\n", cudaGetErrorString(error));
	}

	cudaMemcpy(sobel_y.pixels, dev_out_y, (imageSize*sizeof(char)),cudaMemcpyDeviceToHost);
	cudaMemcpy(sobel_x.pixels, dev_out_x, (imageSize*sizeof(char)),cudaMemcpyDeviceToHost);

	convolve_time=PAPI_get_real_usec();

	/* Combine to form output */
	//Allocating memory on GPU
	unsigned char *dev_sobelx, *dev_sobely, *dev_new;

	cudaMalloc((void **)&dev_sobelx,(image.x*image.y*image.depth*sizeof(char)));
	cudaMalloc((void **)&dev_sobely,(image.x*image.y*image.depth*sizeof(char)));
	cudaMalloc((void **)&dev_new,(image.x*image.y*image.depth*sizeof(char)));

	//copying memory to GPU
	copy_before = PAPI_get_real_usec();
	cudaMemcpy(dev_sobelx, sobel_x.pixels, (imageSize*sizeof(char)),cudaMemcpyHostToDevice);
	cudaMemcpy(dev_sobely, sobel_y.pixels, (imageSize*sizeof(char)),cudaMemcpyHostToDevice);
	copy_after= PAPI_get_real_usec();

	combine_before = PAPI_get_real_usec();
	combine<<<(imageSize+255)/256,256>>>(dev_sobelx, dev_sobely, dev_new, imageSize);

	// wait for combixe to finish
	cudaDeviceSynchronize();
	combine_after= PAPI_get_real_usec();

	/* REPLACE THE ABOVE WITH YOUR CODE */
	/* IT SHOULD ALLOCATE SPACE ON DEVICE */
	/* COPY SOBEL_X and SOBEL_Y data to device */
	/* RUN THE KERNEL */
	/* THEN COPY THE RESULTS BACK */

	copy2_before= PAPI_get_real_usec();
	cudaMemcpy(new_image.pixels, dev_new, (imageSize*sizeof(char)),cudaMemcpyDeviceToHost);
	copy2_after= PAPI_get_real_usec();

	store_before=PAPI_get_real_usec();

	/* Write data back out to disk */
	store_jpeg("out.jpg",&new_image);

	store_after=PAPI_get_real_usec();

	/* Print timing results */
	printf("Load time: %lld\n",load_time-start_time);
        printf("Convolve time: %lld\n",convolve_time-load_time);
	printf("Copy host to device: %lld\n",(copy_after-copy_before));
        printf("Combine time: %lld\n",combine_after-combine_before);
	printf("Copy device to host: %lld\n",(copy2_after-copy2_before));
        printf("Store time: %lld\n",store_after-store_before);
	printf("Total time = %lld\n",store_after-start_time);

	return 0;
}
