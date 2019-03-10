/* Example sobel code for ECE574 -- Fall 2015 */
/* By Vince Weaver <vincent.weaver@maine.edu> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

#include <jpeglib.h>

#include <omp.h>

#include "papi.h"

/* Filters */
static int sobel_x_filter[3][3]={{-1,0,+1},{-2,0,+2},{-1,0,+1}};
static int sobel_y_filter[3][3]={{-1,-2,-1},{0,0,0},{1,2,+1}};

/* Structure describing the image */
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
};


/* very inefficient convolve code */
static void *generic_convolve(void *argument) {

	int x,y,k,l,d;
	uint32_t color;
	int sum,depth,width;

	struct image_t *old;
	struct image_t *new;
	int (*filter)[3][3];
	struct convolve_data_t *data;

	/* Convert from void pointer to the actual data type */
	data=(struct convolve_data_t *)argument;
	old=data->old;
	new=data->new;
	filter=data->filter;

	depth=old->depth;
	width=old->x*old->depth;


	for(d=0;d<3;d++) {
	   for(x=1;x<old->x-1;x++) {
	     for(y=1;y<old->y-1;y++) {
		sum=0;
		for(k=-1;k<2;k++) {
		   for(l=-1;l<2;l++) {
			color=old->pixels[((y+l)*width)+(x*depth+d+k*depth)];
			sum+=color * (*filter)[k+1][l+1];
		   }
		}

		if (sum<0) sum=0;
		if (sum>255) sum=255;

		new->pixels[(y*width)+x*depth+d]=sum;
	     }
	   }
	}

	return NULL;
}

static int combine(struct image_t *s_x,
			struct image_t *s_y,
			struct image_t *new) {
	int i;
	int out;

	for(i=0;i<( s_x->depth * s_x->x * s_x->y );i++) {

		out=sqrt(
			(s_x->pixels[i]*s_x->pixels[i])+
			(s_y->pixels[i]*s_y->pixels[i])
			);
		if (out>255) out=255;
		if (out<0) out=0;
		new->pixels[i]=out;
	}

	return 0;
}

int main(int argc, char **argv) {

	struct image_t image,sobel_x,sobel_y,new_image;
	struct convolve_data_t sobel_data[2];

	/* Check command line usage */
	if (argc<2) {
		fprintf(stderr,"Usage: %s image_file\n",argv[0]);
		return -1;
	}

	PAPI_library_init(PAPI_VER_CURRENT);



	/* Load an image */
	load_jpeg(argv[1],&image);

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
	sobel_data[0].old=&image;
	sobel_data[0].new=&sobel_x;
	sobel_data[0].filter=&sobel_x_filter;
	generic_convolve((void *)&sobel_data[0]);

	sobel_data[1].old=&image;
	sobel_data[1].new=&sobel_y;
	sobel_data[1].filter=&sobel_y_filter;
	generic_convolve((void *)&sobel_data[1]);

	/* Combine to form output */
	combine(&sobel_x,&sobel_y,&new_image);

	/* Write data back out to disk */
	store_jpeg("out.jpg",&new_image);

	PAPI_shutdown();

	return 0;
}
