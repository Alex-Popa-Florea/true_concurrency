#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"
#include <time.h>

#define BLUR_REGION_SIZE 9
#define BILLION  1000000000L;
#define THREADLIMIT 15;
// ---------- MAIN PROGRAM ---------- \\

  struct task_args {
    struct picture *pic;
    struct picture tmp;
    int i_start;
    int i_end;
    int j_start;
    int j_end;
  };    


  void *blur_chunk(void *args_ptr){
    struct task_args *args = (struct task_args *) args_ptr;
    for(int i = args->i_start; i < args->i_end + 1; i++){
      for(int j = args->j_start; j < args->j_end + 1; j++){
        struct pixel rgb;  
        int sum_red = 0;
        int sum_green = 0;
        int sum_blue = 0;
      
        for(int n = -1; n <= 1; n++){
          for(int m = -1; m <= 1; m++){
            rgb = get_pixel(&args->tmp, i+n, j+m);
            sum_red += rgb.red;
            sum_green += rgb.green;
            sum_blue += rgb.blue;
          }
        }
      
        rgb.red = sum_red / BLUR_REGION_SIZE;
        rgb.green = sum_green / BLUR_REGION_SIZE;
        rgb.blue = sum_blue / BLUR_REGION_SIZE;
      
        set_pixel(args->pic, i, j, &rgb);
      }
    }
  }


  void blur_column_by_column(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t threads[tmp.width - 2];
    struct task_args args[tmp.width - 2];
    for(int i = 1 ; i < tmp.width - 1; i++){
      args[i - 1].pic = pic;
      args[i - 1].tmp = tmp;
      args[i - 1].i_start = i;
      args[i - 1].i_end = i;
      args[i - 1].j_start = 1;
      args[i - 1].j_end = tmp.height - 2;

      pthread_create(&threads[i - 1], NULL, blur_chunk, &args[i - 1]);
    }
    for(int i = 1; i < tmp.width - 1; i++){
      pthread_join(threads[i - 1], NULL);
    }

    clear_picture(&tmp);
  }

  void blur_row_by_row(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t threads[tmp.height - 2];
    struct task_args args[tmp.height - 2];
    for(int j = 1 ; j < tmp.height - 1; j++){
      args[j - 1].pic = pic;
      args[j - 1].tmp = tmp;
      args[j - 1].i_start = 1;
      args[j - 1].i_end = tmp.width - 2;
      args[j - 1].j_start = j;
      args[j - 1].j_end = j;


      pthread_create(&threads[j - 1], NULL, blur_chunk, &args[j - 1]);
    }
    for(int j = 1; j < tmp.height - 1; j++){
      pthread_join(threads[j - 1], NULL);
    }

    clear_picture(&tmp);
  }

  void blur_pixel_by_pixel(struct picture *pic){
    struct picture tmp;
    int m = 0;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = malloc((tmp.width - 2) * (tmp.height - 2) * sizeof(pthread_t));
    struct task_args *args = malloc((tmp.width - 2) * (tmp.height - 2) * sizeof(struct task_args));

    for(int i = 1; i < tmp.width - 1; i++){
      for(int j = 1; j < tmp.height - 1; j++){
        args[(i - 1) * (tmp.height - 2) + j - 1].pic = pic;
        args[(i - 1) * (tmp.height - 2) + j - 1].tmp = tmp;
        args[(i - 1) * (tmp.height - 2) + j - 1].i_start = i;
        args[(i - 1) * (tmp.height - 2) + j - 1].i_end = i;
        args[(i - 1) * (tmp.height - 2) + j - 1].j_start = j;
        args[(i - 1) * (tmp.height - 2) + j - 1].j_end = j;

        pthread_create(&threads[(i - 1) * (tmp.height - 2) + j - 1], NULL, blur_chunk, &args[(i - 1) * (tmp.height - 2) + j - 1]);
        
      }
    }
    for(int i = 1; i < tmp.width - 1; i++){
      for(int j = 1; j < tmp.height - 1; j++){
        pthread_join(threads[(i - 1) * (tmp.height - 2) + j - 1], NULL);
      }
    }
    free(threads);
    free(args);

    clear_picture(&tmp);
  }


  void blur_sector_by_sector(struct picture *pic, int split){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;
    pthread_t *threads = malloc(split * split * sizeof(pthread_t));
    if (threads == NULL) {
      return;
    }
    struct task_args *args = malloc(split * split * sizeof(struct task_args));
    if (args == NULL) {
      return;
    }

    int i_start = 1;
    int j_start = 1;
    int i_end = tmp.width / split;
    int j_end = tmp.height / split;
    int x = 0;
    int y = 0;

    while (i_end < tmp.width - 1) {
      while (j_end < tmp.height - 1) {
        args[x * split + y].pic = pic;
        args[x * split + y].tmp = tmp;
        args[x * split + y].i_start = i_start;
        args[x * split + y].i_end = i_end;
        args[x * split + y].j_start = j_start;
        args[x * split + y].j_end = j_end;

        pthread_create(&threads[x * split + y], NULL, blur_chunk, &args[x * split + y]);

        j_start = j_end + 1;
        j_end = j_end + (tmp.height / split) + 1;
        y++;
      }
      
      args[x * split + y].pic = pic;
      args[x * split + y].tmp = tmp;
      args[x * split + y].i_start = i_start;
      args[x * split + y].i_end = i_end;
      args[x * split + y].j_start = j_start;
      args[x * split + y].j_end = tmp.height - 2;

      pthread_create(&threads[x * split + y], NULL, blur_chunk, &args[x * split + y]);

      y = 0;
      j_start = 1;
      j_end = tmp.height / split;

      i_start = i_end + 1;
      i_end = i_end + (tmp.width / split) + 1;
      x++;
    }
    while (j_end < tmp.height - 1) {
      args[x * split + y].pic = pic;
      args[x * split + y].tmp = tmp;
      args[x * split + y].i_start = i_start;
      args[x * split + y].i_end = tmp.width - 2;
      args[x * split + y].j_start = j_start;
      args[x * split + y].j_end = j_end;

      pthread_create(&threads[x * split + y], NULL, blur_chunk, &args[x * split + y]);

      j_start = j_end + 1;
      j_end = j_end + (tmp.height / split) + 1;
      y++;
    }
    
    args[x * split + y].pic = pic;
    args[x * split + y].tmp = tmp;
    args[x * split + y].i_start = i_start;
    args[x * split + y].i_end = tmp.width - 2;
    args[x * split + y].j_start = j_start;
    args[x * split + y].j_end = tmp.height - 2;

    pthread_create(&threads[x * split + y], NULL, blur_chunk, &args[x * split + y]);
    for(int x = 0; x < split; x++){
      for(int y = 0; y < split; y++){
        pthread_join(threads[x * split + y], NULL);
      }
    }

    clear_picture(&tmp);
    free(threads);
    free(args);
  }

  int main(int argc, char **argv){

    struct picture *pic = malloc(sizeof(struct picture));
    struct timespec start;
    struct timespec end;
    double accum;
    
    init_picture_from_file(pic, argv[1]);
    clock_gettime(CLOCK_REALTIME, &start);
    blur_picture(pic);
    clock_gettime(CLOCK_REALTIME, &end);
    accum = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / (1000000000L);
    printf( "base time: %lf seconds\n", accum );
    
    init_picture_from_file(pic, argv[1]);
    clock_gettime(CLOCK_REALTIME, &start);
    blur_row_by_row (pic);
    clock_gettime(CLOCK_REALTIME, &end);
    accum = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
    printf("row time: %lf seconds\n", accum );

    init_picture_from_file(pic, argv[1]);
    clock_gettime(CLOCK_REALTIME, &start);
    blur_column_by_column (pic);
    clock_gettime(CLOCK_REALTIME, &end);
    accum = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
    printf( "column time: %lf seconds\n", accum );

    init_picture_from_file(pic, argv[1]);
    clock_gettime(CLOCK_REALTIME, &start);
    blur_sector_by_sector (pic, 2);
    clock_gettime(CLOCK_REALTIME, &end);
    accum = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
    printf( "sector time: %lf seconds\n", accum );

    init_picture_from_file(pic, argv[1]);
    clock_gettime(CLOCK_REALTIME, &start);
    blur_pixel_by_pixel (pic);
    clock_gettime(CLOCK_REALTIME, &end);
    accum = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
    printf( "pixel time: %lf seconds\n", accum );


    save_picture_to_file(pic, argv[2]);
    free(pic);

    return 0;
  }
