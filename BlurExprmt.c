#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"
#include <time.h>
#include <math.h>
#include "BlurExprmt.h"

#define BLUR_REGION_SIZE 9
#define BILLION  1000000000L
#define THREADLIMIT 4
// ---------- MAIN PROGRAM ---------- \\


  /*
    Global stack of args threads in thread pixel can pull from.
  */
  list_t task_list;

  /*
    Lock used for the global stack when popping.
  */
  pthread_mutex_t task_list_lock;
  
  /*
    Funcition to pop from the stack.
  */
  struct task_args *list_pop(list_t *list) {
    list_elem_t *old_head = list->head;
    if (old_head == NULL) {
      return NULL;
    }
    struct task_args *task = old_head->task;
    list->head = old_head->next;
    free(old_head);
    list->size--;
    return task;
  }

  /*
    Funcition to push to the stack;
  */
  void list_push(list_t *list, struct task_args *task) {
    list_elem_t *new_head = malloc(sizeof(list_elem_t));
    if (new_head == NULL) {
      return;
    }
    new_head->next = list->head;
    new_head->task = task;
    list->head = new_head;
    list->size++;
  }

  /*
    Generic bluring function to pass to the threads with the args.
  */
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

  /*
    Function to give to blur pixel which uses the stack to reuse threads
    After bluring one pixel, it pops another from the list to blur another one
    until the task stack is emptied.
  */
  void *do_tasks() {
    pthread_mutex_lock(&task_list_lock);
    while(task_list.size != 0) {
      struct task_args *task = list_pop(&task_list);
      if (task == NULL) {
        break;
      }
      pthread_mutex_unlock(&task_list_lock);
      blur_chunk(task);
      pthread_mutex_lock(&task_list_lock);
    }
    pthread_mutex_unlock(&task_list_lock);
  }

  /*
    Bluring function that does bluring in a column by column manner.
  */
  void blur_column_by_column(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = malloc((tmp.width - 2) * sizeof(pthread_t));
    struct task_args *args = malloc((tmp.width - 2) * sizeof(struct task_args));
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
    free(threads);
    free(args);
  }

  /*
    Bluring function that does bluring in a row by row manner.
  */
  void blur_row_by_row(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = malloc((tmp.height - 2) * sizeof(pthread_t));
    struct task_args *args = malloc((tmp.height - 2) * sizeof(struct task_args));
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
    free(threads);
    free(args);
  }

  /*
    Bluring function that does bluring in a pixel by pixel manner.
  */
  void blur_pixel_by_pixel(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = malloc(THREADLIMIT * sizeof(pthread_t));

    for(int i = 1; i < tmp.width - 1; i++){
      for(int j = 1; j < tmp.height - 1; j++){
        struct task_args *task = malloc(sizeof(struct task_args));
        task->pic = pic;
        task->tmp = tmp;
        task->i_start = i;
        task->i_end = i;
        task->j_start = j;
        task->j_end = j;
        list_push(&task_list, task);
      }
    }
    for (int i = 0; i < THREADLIMIT; i++){
      pthread_create(&threads[i], NULL, do_tasks, NULL);
    }
    
    for(int i = 0; i < THREADLIMIT; i++){
      pthread_join(threads[i], NULL);
    }

    free(threads);

    clear_picture(&tmp);
  }

  /*
    Bluring function that does bluring in a sector by sector manner.
    The number of sectors is given in the sectors parameter, which has to
    be a square number, as the image will be split into equal rectangles.
    The rectangles at the edges of the image will have a slightly smaller
    size than the rest if the image width and height are not divisible by the
    root of the number of sectors.
  */
  void blur_sector_by_sector(struct picture *pic, int sectors){
    int split = sqrt(sectors);
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


  /*
    Checks whether a blurred image stored in a file is the same as 
    base_blur.jpg, which is generated by running the sequential
    blur_picture. (This is done as the first blur in the experiment, 
    and is assumbed correct)
  */
  bool check_correctness(char *file) {
    char command[500];
    sprintf(command, "./picture_compare base_blur.jpg %s", file);
    if (system(command) != 0) {
      printf("\nPicture %s was not blurred correctly!\n", file);
      return false;
    } else {
      return true;
    }
  }


  int main(int argc, char **argv){

    struct picture *pic = malloc(sizeof(struct picture));
    struct timespec start;
    struct timespec end;
    double diff;
    double sum;
    double average[6];
    bool correctness = true;
    
    int repeats = atoi(argv[3]);

    // Creates a file to store the results of the experiment.
    FILE *f = fopen(argv[2], "w");
    if (f == NULL){
        printf("Error opening file!\n");
        return -1;
    }

    printf("\nBegining the Blur Experiment: \n");
    fprintf(f, "\nBegining the Blur Experiment: \n");

    printf("\nSequential Implementation: \n\n");
    fprintf(f, "\nSequential Implementation: \n\n");
    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_picture(pic);
      clock_gettime(CLOCK_REALTIME, &end);
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for non-threaded: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[0] = sum / repeats;
    sum = 0;
    printf("Average time for non-threaded: %lf seconds\n\n", average[0]);
    fprintf(f, "Average time for non-threaded: %lf seconds\n\n", average[0]);
    save_picture_to_file(pic, "base_blur.jpg");
    

    printf("\nRow by Row Implementation: \n\n");
    fprintf(f, "\nRow by Row Implementation: \n\n");

    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_row_by_row(pic);
      clock_gettime(CLOCK_REALTIME, &end);
      save_picture_to_file(pic, "row_by_row_blur.jpg");
      correctness &= check_correctness("row_by_row_blur.jpg");
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for row by row: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[1] = sum / repeats;
    sum = 0;
    printf("Average time for row by row: %lf seconds\n\n", average[1]);
    fprintf(f, "Average time for row by row: %lf seconds\n\n", average[1]);


    printf("\nColumn by Column Implementation: \n\n");
    fprintf(f, "\nColumn by Column Implementation: \n\n");

    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_column_by_column(pic);
      clock_gettime(CLOCK_REALTIME, &end);
      save_picture_to_file(pic, "column_by_column_blur.jpg");
      correctness &= check_correctness("column_by_column_blur.jpg");
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for column by column: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[2] = sum / repeats;
    sum = 0;
    printf("Average time for column by column: %lf seconds\n\n", average[2]);
    fprintf(f, "Average time for column by column: %lf seconds\n\n", average[2]);
    

    printf("\nPixel by Pixel Implementation: \n\n");
    fprintf(f, "\nPixel by Pixel Implementation: \n\n");

    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_pixel_by_pixel(pic);
      clock_gettime(CLOCK_REALTIME, &end);
      save_picture_to_file(pic, "pixel_by_pixel_blur.jpg");
      correctness &= check_correctness("pixel_by_pixel_blur.jpg");
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for pixel by pixel: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[3] = sum / repeats;
    sum = 0;
    printf("Average time for pixel by pixel: %lf seconds\n\n", average[3]);
    fprintf(f, "Average time for pixel by pixel: %lf seconds\n\n", average[3]);
    


    printf("\nSector by Sector with 4 sectors Implementation: \n\n");
    fprintf(f, "\nSector by Sector with 4 sectors Implementation: \n\n");

    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_sector_by_sector(pic, 4);
      clock_gettime(CLOCK_REALTIME, &end);
      save_picture_to_file(pic, "small_sector_by_sector_blur.jpg");
      correctness &= check_correctness("small_sector_by_sector_blur.jpg");
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for 4 sectors: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[4] = sum / repeats;
    sum = 0;
    printf("Average time for sector by sector with 4 sectors: %lf seconds\n\n", average[4]);
    fprintf(f, "Average time for sector by sector with 4 sectors: %lf seconds\n\n", average[4]);
    


    printf("\nSector by Sector with 100 sectors Implementation: \n\n");
    fprintf(f, "\nSector by Sector with 100 sectors Implementation: \n\n");

    for (int i = 0; i < repeats; i++){
      init_picture_from_file(pic, argv[1]);
      clock_gettime(CLOCK_REALTIME, &start);
      blur_sector_by_sector(pic, 100);
      clock_gettime(CLOCK_REALTIME, &end);
      save_picture_to_file(pic, "large_sector_by_sector_blur.jpg");
      correctness &= check_correctness("large_sector_by_sector_blur.jpg");
      diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
      fprintf(f, "Time in iteration %d for 100 sectors: %lf seconds\n", i, diff);
      sum += diff;
    }
    average[5] = sum / repeats;
    sum = 0;
    printf("Average time for sector by sector with 100 sectors: %lf seconds\n\n", average[5]);
    fprintf(f, "Average time for sector by sector with 100 sectors: %lf seconds\n\n", average[5]);
    


    printf("\nResults after %d repeats each: \n\n", repeats);
    fprintf(f, "\nResults after %d repeats each: \n\n", repeats);
    
    printf("Average time for non-threaded: %lf seconds\n", average[0]);
    fprintf(f, "Average time for non-threaded: %lf seconds\n", average[0]);

    printf("Average time for row by row: %lf seconds\n", average[1]);
    fprintf(f, "Average time for row by row: %lf seconds\n", average[1]);

    printf("Average time for column by column: %lf seconds\n", average[2]);
    fprintf(f, "Average time for column by column: %lf seconds\n", average[2]);

    printf("Average time for pixel by pixel: %lf seconds\n", average[3]);
    fprintf(f, "Average time for pixel by pixel: %lf seconds\n", average[3]);

    printf("Average time for sector by sector with 4 sectors: %lf seconds\n", average[4]);
    fprintf(f, "Average time for sector by sector with 4 sectors: %lf seconds\n", average[4]);

    printf("Average time for sector by sector with 100 sectors: %lf seconds\n", average[5]);
    fprintf(f, "Average time for sector by sector with 100 sectors: %lf seconds\n", average[5]);

    if (correctness) {
      fprintf (f, "\nAll images were blurred correctly.\n");
      printf ("\nAll images were blurred correctly.\n");
    } else {
      fprintf (f, "\nNot all images were blurred correctly.\n");
      printf ("\nNot all images were blurred correctly.\n");
    }

    free(pic);
    fclose(f);

    return 0;
  }
