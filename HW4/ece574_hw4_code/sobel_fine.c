//Jason Millette
//2/16/19

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <jpeglib.h>

#include "papi.h"

static int sobel_x_filter[3][3]={{-1,0,+1},{-2,0,+2},{-1,0,+1}};
static int sobel_y_filter[3][3]={{-1,-2,-1},{0,0,0},{1,2,+1}};

struct image_t {
	int x;
	int y;
	int depth;	/* bytes */
	unsigned char *pixels;
};

struct convolve_data_t {
	struct image_t *old;
	struct image_t *new;
	int (*filter)[3][3];
	int start;
	int stop;
};


static void *generic_convolve(void *argument) {



	/******************/
	/* Your code here */
	/******************/
	int sum;
	printf("Convolve\n");

	//handling void pointer for struct
	struct image_t *old;
	struct image_t *new;
	int (*filter)[3][3];
	struct convolve_data_t *data;

	data = (struct convolve_data_t *)argument;
	old = data->old;
	new = data->new;
	filter = data->filter;
	printf("Start: %d\n", data->start);
	printf("Stop: %d\n", data->stop);

	for (int x=data->start; x<data->stop-1; x++) {
		for (int y=data->start; y<data->stop-1; y++) {
			for (int color=0; color<3; color++) {
				sum = 0;

				//applying filter to image
				sum += (*filter)[0][0]*old->pixels[((y-1)*old->x*3) + ((x-1)*3) + color];
				sum += (*filter)[1][0]*old->pixels[((y-1)*old->x*3) + (x*3) + color];
				sum += (*filter)[2][0]*old->pixels[((y-1)*old->x*3) + ((x+1)*3) + color];
				sum += (*filter)[0][1]*old->pixels[(y*old->x*3) + ((x-1)*3) + color];
				sum += (*filter)[1][1]*old->pixels[(y*old->x*3) + (x*3) + color];
				sum += (*filter)[2][1]*old->pixels[(y*old->x*3) + ((x+1)*3) + color];
				sum += (*filter)[0][2]*old->pixels[((y+1)*old->x*3) + ((x-1)*3) + color];
				sum += (*filter)[1][2]*old->pixels[((y+1)*old->x*3) + (x*3) + color];
				sum += (*filter)[2][2]*old->pixels[((y+1)*old->x*3) + ((x+1)*3) + color];

				//Saturating
				if (sum < 0) 
					sum = 0;
				if (sum > 255) 
					sum = 255;

				//setting the new value
				new->pixels[(y*old->x*3) + (x*3) + color] = sum;
			}
		}
	}
	pthread_exit(NULL);
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
	image->pixels=malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);

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

static int store_jpeg(char *filename, struct image_t *image) {

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

static int combine(struct image_t *sobel_x, struct image_t *sobel_y,
		struct image_t *new_image) {

	/******************/
	/* your code here */
	/******************/
	int sum;
	for (int x=1; x<new_image->x-1; x++) {
		for (int y=1; y<new_image->y-1; y++) {
			for (int color=0; color<3; color++) {
				sum = 0;

				// combining the two sobel filters
				sum = sqrt((sobel_x->pixels[(y*sobel_x->x*3) + (x*3) + color])*(sobel_x->pixels[(y*sobel_x->x*3) + (x*3) + color]) + (sobel_y->pixels[(y*sobel_y->x*3) + (x*3) + color])*(sobel_y->pixels[(y*sobel_y->x*3) +(x*3) + color]));
				//Saturating
				if (sum < 0) 
					sum = 0;
				if (sum > 255) 
					sum = 255;

				//setting the new value
				new_image->pixels[(y*new_image->x*3) + (x*3) + color] = sum;
			}
		}
	}


	return 0;
}

int main(int argc, char **argv) {

	int numThreads = 8;
	printf("Threads: %d\n", numThreads); //debugging
	pthread_t threads[numThreads]; //inits number of threads from command line argument
	pthread_attr_t attr; //pthread attribute for return status to use with join

	struct image_t image,sobel_x,sobel_y,new_image;
	struct convolve_data_t sobel_data[2*numThreads];
	int result;
	void *status; //for pthread status

	/* Check command line usage */
	if (argc<2) {
		fprintf(stderr,"Usage: %s image_file\n",argv[0]);
		return -1;
	}

	result=PAPI_library_init(PAPI_VER_CURRENT);
	if (result!=PAPI_VER_CURRENT) {
		fprintf(stderr,"Warning!  PAPI error %s\n",
			PAPI_strerror(result));
	}

	//Initialize and set thread detached attribute
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


	//Creating variables for papi
	long long value[2];

	/* Load an image */
	value[0] = PAPI_get_real_usec(); //first time stamp
	load_jpeg(argv[1],&image);
	value[1] = PAPI_get_real_usec(); //second time stamp
	printf("Load time: %lld\n", value[1] - value[0]); //printing time

	/* Allocate space for output image */
	new_image.x=image.x;
	new_image.y=image.y;
	new_image.depth=image.depth;
	new_image.pixels=malloc(image.x*image.y*image.depth*sizeof(char));

	/* Allocate space for output image */
	sobel_x.x=image.x;
	sobel_x.y=image.y;
	sobel_x.depth=image.depth;
	sobel_x.pixels=malloc(image.x*image.y*image.depth*sizeof(char));

	/* Allocate space for output image */
	sobel_y.x=image.x;
	sobel_y.y=image.y;
	sobel_y.depth=image.depth;
	sobel_y.pixels=malloc(image.x*image.y*image.depth*sizeof(char));

	/* convolution */
	//setting up structs
	
	for (int i = 0; i < (numThreads); i++) { //handle remainder of threads
		//printf("structs loop: %d\n", i);
		sobel_data[i].old=&image;
		sobel_data[i].new=&sobel_x;
		sobel_data[i].filter=&sobel_x_filter;
		sobel_data[i].start=i * ((image.x)/(numThreads));
		sobel_data[i].stop=(i+1) * ((image.x)/(numThreads));

		sobel_data[(numThreads) + i].old=&image;
		sobel_data[(numThreads) + i].new=&sobel_y;
		sobel_data[(numThreads) + i].filter=&sobel_y_filter;
		sobel_data[(numThreads) + i].start=i * ((image.y)/(numThreads));
		sobel_data[(numThreads) + i].stop= (i+1) * ((image.y)/(numThreads));
	}


	//using threads to convolve
	value[0] = PAPI_get_real_usec(); //first time stamp
	for (int i = 0; i < (2*numThreads); i+=2) {
		printf("Thread loop: %d\n", i);
		pthread_create(&threads[i], &attr, generic_convolve, (void *)&sobel_data[i]);
		pthread_create(&threads[(numThreads) + i], &attr, generic_convolve, (void *)&sobel_data[(numThreads) + i]);
	}

	//waits for threads to finish to run the combine function
	for (int i = 0; i < numThreads; i++) {
		pthread_join(threads[i], &status);
	}
	value[1] = PAPI_get_real_usec(); //second time stamp
	printf("Convolve time: %lld\n", value[1] - value[0]); //printing time

	value[0] = PAPI_get_real_usec(); //first time stamp
	combine(&sobel_x, &sobel_y, &new_image);
	value[1] = PAPI_get_real_usec(); //second time stamp
	printf("Combine time: %lld\n", value[1] - value[0]); //printing time

	value[0] = PAPI_get_real_usec(); //first time stamp
	store_jpeg("out.jpg",&new_image);
	value[1] = PAPI_get_real_usec(); //second time stamp
	printf("Store time: %lld\n", value[1] - value[0]); //printing time

	PAPI_shutdown();

	return 0;
}
