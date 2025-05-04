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
    FILE * in_file;
    char sbuf[256];
    
    char *ibuf;
    PPM_IMG result;
    int v_max, i, offst;
    in_file = fopen(path, "r");
    if (in_file == NULL){
        printf("Input file not found!\n");
        exit(1);
    }
    /*Skip the magic number*/
    fscanf(in_file, "%s", sbuf);


    //result = malloc(sizeof(PPM_IMG));
    fscanf(in_file, "%d",&result.w);
    fscanf(in_file, "%d",&result.h);
    fscanf(in_file, "%d\n",&v_max);
    offst = ftell(in_file);
    fclose(in_file);
    if (rango == 0) 
    	printf("Image size: %d x %d\n", result.w, result.h);

    int size = result.w * result.h;
    double max = double(size) / double(equipos);
    int max_sect = (int) ceil(max);
    result.init_offst = rango * max_sect;

    if(result.init_offst + max_sect < size)
	    result.size_sect = max_sect;
    else
	    result.size_sect = size - result.init_offst;

    result.img_r = (unsigned char *)malloc(result.size_sect*sizeof(unsigned char));
    result.img_g = (unsigned char *)malloc(result.size_sect*sizeof(unsigned char));
    result.img_b = (unsigned char *)malloc(result.size_sect*sizeof(unsigned char));
    ibuf         = (char *)malloc(3 * result.size_sect*sizeof(unsigned char));

    
    //fread(ibuf,sizeof(unsigned char), 3 * result.w*result.h, in_file);
    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
    MPI_File_read_at_all(fh, (3*result.init_offst)+offst, ibuf, 3*result.size_sect, MPI_UNSIGNED_CHAR, NULL);
    MPI_File_close(&fh);

    for(i = 0; i < result.size_sect; i ++){
        result.img_r[i] = ibuf[3*i + 0];
        result.img_g[i] = ibuf[3*i + 1];
        result.img_b[i] = ibuf[3*i + 2];
    }
    
    free(ibuf);
    
    return result;
}

void write_ppm(PPM_IMG img, const char * path, int rango){
    int i, offst;

    char * obuf = (char *)malloc(3 * img.size_sect);
    for(i = 0; i < img.size_sect; i ++){
       	obuf[3*i + 0] = img.img_r[i];
        obuf[3*i + 1] = img.img_g[i];
       	obuf[3*i + 2] = img.img_b[i];
    }

    char cab_buf[256];
    sprintf(cab_buf, "P6\n%d %d\n255\n", img.w, img.h);
    offst = strlen(cab_buf);

    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &fh);
    if(rango==0) {
    	//FILE * out_file;
    
    	//out_file = fopen(path, "wb");
    	//fprintf(out_file, "%s", cab_buf);
    	//fwrite(obuf,sizeof(unsigned char), 3*img.w*img.h, out_file);
    	//fclose(out_file);	
	MPI_File_write(fh, cab_buf, offst, MPI_CHAR, NULL);
    }

    //MPI_Bcast(&offst, 1, MPI_INT, 0, MPI_COMM_WORLD);

    //MPI_File fh;
    //MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDWR, MPI_INFO_NULL, &fh);
    MPI_File_write_at_all(fh, (3*img.init_offst)+offst, obuf, 3*img.size_sect, MPI_UNSIGNED_CHAR, NULL);

    MPI_File_close(&fh);

    free(obuf);
}

void free_ppm(PPM_IMG img)
{
    free(img.img_r);
    free(img.img_g);
    free(img.img_b);
}

PGM_IMG read_pgm(const char * path, int equipos, int rango){
    FILE * in_file;
    char sbuf[256];
    
    // Esta maravilla de formato escribe la anchura y la altura de la imagen en ASCII, lo que hace imposible leer un entero que sea dichos valores, habría que leer caracter por caracter hasta encontrar un espacio en blanco y entonces parsear el número
    PGM_IMG result;
    int v_max;//, i;
    in_file = fopen(path, "r");
    if (in_file == NULL){
        printf("Input file not found!\n");
        exit(1);
    }
    
    /*
     * Imagen PGM
     * n bytes - MAGIC - "P5"
     * 4 bytes - Anchura -> w
     * 4 bytes - Altura -> h
     * w*h bytes - Imagen
     *
     * Estructura
     * "P5 Anchura Altura ValorGrisMaximo\nImagen"
     * El valor de gris maximo es 255 en la salida
     */ 
    fscanf(in_file, "%s", sbuf); /*Skip the magic number*/ // Los primeros 256 bytes
    fscanf(in_file, "%d",&result.w); // La anchura de la imagen
    fscanf(in_file, "%d",&result.h); // La altura de la imagen
    fscanf(in_file, "%d\n",&v_max);
    if(rango == 0)
    	printf("Image size: %d x %d\n", result.w, result.h);
    
    int init_offst = ftell(in_file);
    fclose(in_file);

    int size = result.w * result.h;
    double max = double(size) / double(equipos);
    int max_sect = (int) ceil(max);
    result.init_offst = rango*max_sect;

    if(result.init_offst + max_sect < size)
	    result.size_sect = max_sect;
    else
	    result.size_sect = size - (result.init_offst);

    result.img = (unsigned char *)malloc(result.size_sect*sizeof(unsigned char));
     
    //fread(result.img,sizeof(unsigned char), result.w*result.h, in_file);    
    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
    //MPI_File_seek(fh, result.init_offst + init_offst, MPI_SEEK_SET);
    MPI_File_read_at_all(fh, result.init_offst+init_offst, result.img, result.size_sect, MPI_UNSIGNED_CHAR, NULL);

    MPI_File_close(&fh);
    
    return result;
}

void write_pgm(PGM_IMG img, const char * path, int rango){
    int offst;

    char cab_buf[256];
    sprintf(cab_buf, "P5\n%d %d\n255\n", img.w, img.h);
    offst = strlen(cab_buf);

    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_CREATE | MPI_MODE_RDWR, MPI_INFO_NULL, &fh);
    if(rango == 0) {
	    //FILE * out_file;
	    //out_file = fopen(path, "wb");
	    //fprintf(out_file, "%s", cab_buf);
	    //fclose(out_file);
	    MPI_File_write(fh, cab_buf, offst, MPI_CHAR, NULL);
    }
    
    //fwrite(img.img,sizeof(unsigned char), img.w*img.h, out_file);
    //MPI_Bcast(&offst, 1, MPI_INT, 0, MPI_COMM_WORLD);

    //MPI_File fh;
    //MPI_File_open(MPI_COMM_WORLD, path, MPI_MODE_RDWR, MPI_INFO_NULL, &fh);
    //MPI_File_seek(fh, img.init_offst + offst, MPI_SEEK_SET);
    MPI_File_write_at_all(fh, img.init_offst+offst, img.img, img.size_sect, MPI_UNSIGNED_CHAR, NULL);

    MPI_File_close(&fh);
}

void free_pgm(PGM_IMG img)
{
    free(img.img);
}

