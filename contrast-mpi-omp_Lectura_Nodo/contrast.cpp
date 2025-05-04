#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <cmath>
#include "hist-equ.h"

using namespace std;

void run_cpu_color_test(PPM_IMG img_in);
void run_cpu_gray_test(PGM_IMG img_in);


int main(){
    PGM_IMG img_ibuf_g, img_obuf;
    PPM_IMG img_ibuf_c, img_obuf_hsl, img_obuf_yuv;
    
    // Posiblemente paralelizable con ficheros MPI
    MPI_Init(NULL,NULL);
    double start, total_local, total;
    int equipos, rango;

    MPI_Comm_size(MPI_COMM_WORLD,&equipos);
    MPI_Comm_rank(MPI_COMM_WORLD,&rango);

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    img_ibuf_g = read_pgm("in.pgm", equipos, rango);
    img_ibuf_c = read_ppm("in.ppm", equipos, rango);

    if(rango==0)
    	printf("Starting CPU processing...\n");

    // Seccion paralelizable
    if(rango==0)
    	printf("Running contrast enhancement for gray-scale images.\n");

    img_obuf = contrast_enhancement_g(img_ibuf_g);
    
    if(rango==0)
    	printf("Running contrast enhancement for color images.\n");

    img_obuf_hsl = contrast_enhancement_c_hsl(img_ibuf_c);
    img_obuf_yuv = contrast_enhancement_c_yuv(img_ibuf_c);

    // Posiblemente paralelizable con ficheros MPI
    write_pgm(img_obuf,"out.pgm", rango);
    write_ppm(img_obuf_hsl,"out_hsl.ppm", rango);
    write_ppm(img_obuf_yuv,"out_yuv.ppm", rango);

    
    MPI_Barrier(MPI_COMM_WORLD);
    total_local = MPI_Wtime() - start;
    MPI_Reduce(&total_local,&total,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);

    if(rango==0) {
	printf("Equipos: %d\nTotal processing time: %.6f(ms)\n", equipos, total*1e3);
    }

    MPI_Finalize();

    // Seccion no paralelizable
    free_pgm(img_ibuf_g);
    free_ppm(img_ibuf_c);
    
    free_pgm(img_obuf);
    free_ppm(img_obuf_hsl);
    free_ppm(img_obuf_yuv);
    return 0;
}

PPM_IMG read_ppm(const char * path, int equipos, int rango){
	PPM_IMG result;
	int size, i;
	unsigned char *ibuf_glob;
	char *ibuf;

	if(rango == 0) {
		FILE * in_file;
		char sbuf[256];
		int v_max;
		in_file = fopen(path, "r");
		if (in_file == NULL) {
			printf("Input file not found!\n");
			exit(1);
		}

		fscanf(in_file, "%s", sbuf);
		fscanf(in_file, "%d", &result.w);
		fscanf(in_file, "%d", &result.h);
		fscanf(in_file, "%d\n", &v_max);

		ibuf_glob = (unsigned char *) malloc(3*result.w*result.h * sizeof(unsigned char));
		fread(ibuf_glob, sizeof(unsigned char), 3*result.w*result.h, in_file);
		fclose(in_file);
	}
	else {
		ibuf_glob = NULL;
	}
	MPI_Bcast(&result.w, 2, MPI_INT, 0, MPI_COMM_WORLD);

	size = result.w * result.h;

	if (rango==0) {
		printf("Image size: %d x %d\n", result.w, result.h);
	}

	double max = double(size) / double(equipos);
	int max_sect = (int) ceil(max);
	int offst = rango*max_sect;

	if (offst + max_sect < size)
		result.size_sect = max_sect;
	else
		result.size_sect = size - offst;

	result.img_r = (unsigned char *)malloc(max_sect*sizeof(unsigned char));
	result.img_g = (unsigned char *)malloc(max_sect*sizeof(unsigned char));
	result.img_b = (unsigned char *)malloc(max_sect*sizeof(unsigned char));
	ibuf = (char *)malloc(3* max_sect*sizeof(unsigned char));

	MPI_Scatter(ibuf_glob, 3*max_sect, MPI_UNSIGNED_CHAR, ibuf, 3*max_sect, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

	for(i = 0; i < result.size_sect; i++) {
		result.img_r[i] = ibuf[3*i + 0];
		result.img_g[i] = ibuf[3*i + 1];
		result.img_b[i] = ibuf[3*i + 2];
	}

	free(ibuf);
	if(rango==0)
		free(ibuf_glob);

	return result;
}

void write_ppm(PPM_IMG img, const char * path, int rango){
	unsigned char *glob_obuf = (unsigned char *) malloc(3*img.w*img.h * sizeof(unsigned char));
	int i, equipos;
	MPI_Comm_size(MPI_COMM_WORLD, &equipos);

	double max = double(img.w*img.h) / double(equipos);
	int max_sect = (int) ceil(max);

	char * obuf = (char *) malloc(3*max_sect *sizeof(char));
	for(i = 0; i<img.size_sect; i++) {
		obuf[3*i + 0] = img.img_r[i];
		obuf[3*i + 1] = img.img_g[i];
		obuf[3*i + 2] = img.img_b[i];
	}

	MPI_Gather(obuf, 3*img.size_sect, MPI_UNSIGNED_CHAR, glob_obuf, 3*max_sect, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

	if(rango == 0) {
		char cab_buf[256];
		sprintf(cab_buf, "P6\n%d %d\n255\n", img.w, img.h);
		FILE * out_file;
		out_file = fopen(path, "wb");
		fprintf(out_file, "%s", cab_buf);
		fwrite(glob_obuf, sizeof(unsigned char), 3*img.w*img.h, out_file);
		fclose(out_file);
	}

	free(obuf);
	free(glob_obuf);
}

void free_ppm(PPM_IMG img)
{
    free(img.img_r);
    free(img.img_g);
    free(img.img_b);
}

PGM_IMG read_pgm(const char * path, int equipos, int rango){
    PGM_IMG result;
    int size;
    unsigned char *ibuf;
    if(rango == 0) {
    	FILE * in_file;
    	char sbuf[256];
    	int v_max;//, i;
    	in_file = fopen(path, "r");
    	if (in_file == NULL){
		printf("Input file not found!\n");
        	exit(1);
    	}
    	fscanf(in_file, "%s", sbuf); /*Skip the magic number*/ // Los primeros 256 bytes
    	fscanf(in_file, "%d",&result.w); // La anchura de la imagen
    	fscanf(in_file, "%d",&result.h); // La altura de la imagen
    	fscanf(in_file, "%d\n",&v_max);
    	
	ibuf = (unsigned char *) malloc(result.w*result.h * sizeof(unsigned char));
    	fread(ibuf,sizeof(unsigned char), result.w*result.h, in_file);    
    	fclose(in_file);
    }
    else {
	    ibuf = NULL;
    }
    MPI_Bcast(&result.w, 2, MPI_INT, 0, MPI_COMM_WORLD);

    size = result.w * result.h;

    if(rango == 0) {
    	printf("Image size: %d x %d\n", result.w, result.h);
    }


    double max = double(size) / double(equipos);
    int max_sect = (int) ceil(max);
    int init_offst = rango*max_sect;

    if(init_offst + max_sect < size)
	    result.size_sect = max_sect;
    else
	    result.size_sect = size - init_offst;

    result.img = (unsigned char *)malloc(max_sect*sizeof(unsigned char));

    MPI_Scatter(ibuf, max_sect, MPI_UNSIGNED_CHAR, result.img, max_sect, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if(rango==0);
    	free(ibuf);

    return result;
}

void write_pgm(PGM_IMG img, const char * path, int rango){
    unsigned char *obuf = (unsigned char *) malloc(img.w*img.h * sizeof(unsigned char));
    int equipos;
    MPI_Comm_size(MPI_COMM_WORLD, &equipos);
    double max = double(img.w*img.h) / double(equipos);
    int max_sect = (int) ceil(max);

    MPI_Gather(img.img, img.size_sect, MPI_UNSIGNED_CHAR, obuf, max_sect, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    if(rango == 0) {
    	char cab_buf[256];
    	sprintf(cab_buf, "P5\n%d %d\n255\n", img.w, img.h);
	FILE * out_file;
	out_file = fopen(path, "wb");
	fprintf(out_file, "%s", cab_buf);
	fwrite(obuf, sizeof(unsigned char), img.w*img.h, out_file);
	fclose(out_file);
    }

    free(obuf);
}

void free_pgm(PGM_IMG img)
{
    free(img.img);
}

