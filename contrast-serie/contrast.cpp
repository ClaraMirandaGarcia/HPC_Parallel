#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hist-equ.h"
#include <chrono>

using namespace std;
using namespace chrono;

void run_cpu_color_test(PPM_IMG img_in);
void run_cpu_gray_test(PGM_IMG img_in);


int main(){
    PGM_IMG img_ibuf_g;
    PPM_IMG img_ibuf_c;
    
    high_resolution_clock::time_point start = high_resolution_clock::now();
    printf("Running contrast enhancement for gray-scale images.\n");
    img_ibuf_g = read_pgm("in.pgm");
    run_cpu_gray_test(img_ibuf_g);
    
    printf("Running contrast enhancement for color images.\n");
    img_ibuf_c = read_ppm("in.ppm");
    run_cpu_color_test(img_ibuf_c);

    std::chrono::duration<double> total_time = high_resolution_clock::now() - start;
    printf("Processing time: %f (ms)\n", std::chrono::duration_cast<std::chrono::nanoseconds>(total_time).count()*1e-6);

    free_pgm(img_ibuf_g);
    free_ppm(img_ibuf_c);
    
    return 0;
}

void run_cpu_color_test(PPM_IMG img_in)
{
    PPM_IMG img_obuf_hsl, img_obuf_yuv;
    
    img_obuf_hsl = contrast_enhancement_c_hsl(img_in);
    
    write_ppm(img_obuf_hsl, "out_hsl.ppm");

    img_obuf_yuv = contrast_enhancement_c_yuv(img_in);
    
    write_ppm(img_obuf_yuv, "out_yuv.ppm");
    
    free_ppm(img_obuf_hsl);
    free_ppm(img_obuf_yuv);
}




void run_cpu_gray_test(PGM_IMG img_in)
{
    PGM_IMG img_obuf;
    
    img_obuf = contrast_enhancement_g(img_in);
    
    write_pgm(img_obuf, "out.pgm");
    free_pgm(img_obuf);
}



PPM_IMG read_ppm(const char * path){
    high_resolution_clock::time_point start_proc = high_resolution_clock::now();
    FILE * in_file;
    char sbuf[256];
    
    char *ibuf;
    PPM_IMG result;
    int v_max, i;
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
    printf("Image size: %d x %d\n", result.w, result.h);
    

    result.img_r = (unsigned char *)malloc(result.w * result.h * sizeof(unsigned char));
    result.img_g = (unsigned char *)malloc(result.w * result.h * sizeof(unsigned char));
    result.img_b = (unsigned char *)malloc(result.w * result.h * sizeof(unsigned char));
    ibuf         = (char *)malloc(3 * result.w * result.h * sizeof(char));

    
    high_resolution_clock::time_point start_paral = high_resolution_clock::now();
    fread(ibuf,sizeof(unsigned char), 3 * result.w*result.h, in_file);
    std::chrono::duration<double> time_paral = high_resolution_clock::now() - start_paral;

    start_paral = high_resolution_clock::now();
    for(i = 0; i < result.w*result.h; i ++){
        result.img_r[i] = ibuf[3*i + 0];
        result.img_g[i] = ibuf[3*i + 1];
        result.img_b[i] = ibuf[3*i + 2];
    }
    time_paral += high_resolution_clock::now() - start_paral;
    
    fclose(in_file);
    free(ibuf);

    std::chrono::duration<double> time_proc = high_resolution_clock::now() - start_proc;
    printf("\tReading time (paral): %f (ms)\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_paral).count()*1e-6);
    printf("\tReading time (img proc): %f (ms)\n\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_proc).count()*1e-6);
    
    return result;
}

void write_ppm(PPM_IMG img, const char * path){
    high_resolution_clock::time_point start_proc = high_resolution_clock::now();
    FILE * out_file;
    int i;
    
    char * obuf = (char *)malloc(3 * img.w * img.h * sizeof(char));

    high_resolution_clock::time_point start_paral = high_resolution_clock::now();
    for(i = 0; i < img.w*img.h; i ++){
        obuf[3*i + 0] = img.img_r[i];
        obuf[3*i + 1] = img.img_g[i];
        obuf[3*i + 2] = img.img_b[i];
    }
    std::chrono::duration<double> time_paral = high_resolution_clock::now() - start_paral;

    out_file = fopen(path, "wb");
    fprintf(out_file, "P6\n");
    fprintf(out_file, "%d %d\n255\n",img.w, img.h);

    start_paral = high_resolution_clock::now();
    fwrite(obuf,sizeof(unsigned char), 3*img.w*img.h, out_file);
    time_paral += high_resolution_clock::now() - start_paral;

    fclose(out_file);
    free(obuf);

    std::chrono::duration<double> time_proc = high_resolution_clock::now() - start_proc;
    printf("\tWriting time (paral): %f (ms)\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_paral).count()*1e-6);
    printf("\tWriting time (img proc): %f (ms)\n\n\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_proc).count()*1e-6);
}

void free_ppm(PPM_IMG img)
{
    free(img.img_r);
    free(img.img_g);
    free(img.img_b);
}

PGM_IMG read_pgm(const char * path){
    high_resolution_clock::time_point start_proc = high_resolution_clock::now();
    FILE * in_file;
    char sbuf[256];
    
    
    PGM_IMG result;
    int v_max;//, i;
    in_file = fopen(path, "r");
    if (in_file == NULL){
        printf("Input file not found!\n");
        exit(1);
    }
    
    fscanf(in_file, "%s", sbuf); /*Skip the magic number*/
    fscanf(in_file, "%d",&result.w);
    fscanf(in_file, "%d",&result.h);
    fscanf(in_file, "%d\n",&v_max);
    printf("Image size: %d x %d\n", result.w, result.h);
    

    result.img = (unsigned char *)malloc(result.w * result.h * sizeof(unsigned char));

        
    high_resolution_clock::time_point start_paral = high_resolution_clock::now();
    fread(result.img,sizeof(unsigned char), result.w*result.h, in_file);    
    std::chrono::duration<double> time_paral = high_resolution_clock::now() - start_paral;

    fclose(in_file);
    std::chrono::duration<double> time_proc = high_resolution_clock::now() - start_proc;
    printf("\tReading time (paral): %f (ms)\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_paral).count()*1e-6);
    printf("\tReading time (img proc): %f (ms)\n\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_proc).count()*1e-6);
    
    return result;
}

void write_pgm(PGM_IMG img, const char * path){
    high_resolution_clock::time_point start_proc = high_resolution_clock::now();
    FILE * out_file;
    out_file = fopen(path, "wb");
    fprintf(out_file, "P5\n");
    fprintf(out_file, "%d %d\n255\n",img.w, img.h);

    high_resolution_clock::time_point start_paral = high_resolution_clock::now();
    fwrite(img.img,sizeof(unsigned char), img.w*img.h, out_file);
    std::chrono::duration<double> time_paral = high_resolution_clock::now() - start_paral;

    fclose(out_file);
    std::chrono::duration<double> time_proc = high_resolution_clock::now() - start_proc;
    printf("\tWriting time (paral): %f (ms)\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_paral).count()*1e-6);
    printf("\tWriting time (img proc): %f (ms)\n\n\n", std::chrono::duration_cast<std::chrono::nanoseconds>(time_proc).count()*1e-6);
}

void free_pgm(PGM_IMG img)
{
    free(img.img);
}

