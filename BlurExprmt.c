#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"
#include <time.h>
#include <math.h>
#include "BlurExprmt.h"
#include "thpool/thpool.h"

#define BLUR_REGION_SIZE 9
#define BILLION  1000000000L

#define IMPLEMENTATIONS 7
#define THREADLIMIT 100

// ---------- MAIN PROGRAM ---------- \\

  static void *blur_chunk(void *);
  static void blur_chunk_thpool(void *);
  static void *do_tasks(void *);
  static void blur_column_by_column(struct picture *);
  static void blur_row_by_row(struct picture *);
  static void blur_pixel_by_pixel_with_task_stack(struct picture *);
  static void blur_pixel_by_pixel_with_thpool(struct picture *);
  static void blur_sector_by_sector(struct picture *, int);

  // Array storing file names
  char *files_names[IMPLEMENTATIONS] = {
    "experiment_images/base_blur.jpg", 
    "experiment_images/row_by_row_blur.jpg", 
    "experiment_images/column_by_column_blur.jpg", 
    "experiment_images/pixel_by_pixel_stack_blur.jpg",
    "experiment_images/pixel_by_pixel_thpool_blur.jpg", 
    "experiment_images/few_sector_by_sector_blur.jpg", 
    "experiment_images/many_sector_by_sector_blur.jpg"
  };
  // Array storing implementation names
  char *implemenation_name[IMPLEMENTATIONS] = {
    "Sequential", 
    "Row by Row", 
    "Column by Column", 
    "Pixel by Pixel using stack of tasks", 
    "Pixel by Pixel using thread pool", 
    "Sector by Sector with 4 sectors", 
    "Sector by Sector with 100 sectors" 
  };
  // Array storing the function pointers
  blur_func functions[IMPLEMENTATIONS] = { 
    &blur_picture, 
    &blur_row_by_row, 
    &blur_column_by_column, 
    &blur_pixel_by_pixel_with_task_stack, 
    &blur_pixel_by_pixel_with_thpool, 
    &blur_sector_by_sector, 
    &blur_sector_by_sector 
  };


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
    list_elem_t *new_head = (list_elem_t *) malloc(sizeof(list_elem_t));
    if (new_head == NULL) {
      printf("Ran out of memory for malloc!\n");
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
  static void *blur_chunk(void *args_ptr){
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

  // Wrapper used for thread pool, as void return type required.
  static void blur_chunk_thpool(void *args_ptr){
    blur_chunk(args_ptr);
  }

  /*
    Function to give to blur pixel which uses the stack to reuse threads
    After bluring one pixel, it pops another from the list to blur another one
    until the task stack is emptied.
  */
  static void *do_tasks(void *UNUSED) {
    pthread_mutex_lock(&task_list_lock);
    while(task_list.size != 0) {
      struct task_args *task = list_pop(&task_list);
      pthread_mutex_unlock(&task_list_lock);
      blur_chunk(task);
      free(task);
      pthread_mutex_lock(&task_list_lock);
    }
    pthread_mutex_unlock(&task_list_lock);
  }

  /*
    Bluring function that does bluring in a column by column manner.
  */
  static void blur_column_by_column(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = (pthread_t *) malloc((tmp.width - 2) * sizeof(pthread_t));
    if (threads == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
    struct task_args *args = (struct task_args *) malloc((tmp.width - 2) * sizeof(struct task_args));
    if (args == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
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
  static void blur_row_by_row(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = (pthread_t *) malloc((tmp.height - 2) * sizeof(pthread_t));
    if (threads == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
    struct task_args *args = (struct task_args *) malloc((tmp.height - 2) * sizeof(struct task_args));
    if (args == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
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
    Bluring function that does bluring in a pixel by pixel manner using a stack
    of tasks.
  */
  static void blur_pixel_by_pixel_with_task_stack(struct picture *pic){
    task_list.head = NULL;
    task_list.size = 0;
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;  
    pthread_t *threads = (pthread_t *) malloc(THREADLIMIT * sizeof(pthread_t));
    if (threads == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
    for(int i = 1; i < tmp.width - 1; i++){
      for(int j = 1; j < tmp.height - 1; j++){
        struct task_args *task = (struct task_args *) malloc(sizeof(struct task_args));
        if (task == NULL) {
          printf("Ran out of memory for malloc!\n");
          return;
        }
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

    clear_picture(&tmp);
    free(threads);
  }

  /*
    Bluring function that does bluring in a pixel by pixel manner using a thread
    pool.
  */
  static void blur_pixel_by_pixel_with_thpool(struct picture *pic){
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;
    struct task_args *args = (struct task_args *) malloc((tmp.width - 2) * (tmp.height - 2) * sizeof(struct task_args));
    if (args == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
    threadpool thread_pool = thpool_init(THREADLIMIT);
    int index = 0;
    for(int i = 1; i < tmp.width - 1; i++){
      for(int j = 1; j < tmp.height - 1; j++){
        args[index].pic = pic;
        args[index].tmp = tmp;
        args[index].i_start = i;
        args[index].i_end = i;
        args[index].j_start = j;
        args[index].j_end = j;

        thpool_add_work(thread_pool, blur_chunk_thpool, &args[index]);

        index++;
      }
    }
    thpool_wait(thread_pool);
    thpool_destroy(thread_pool);

    clear_picture(&tmp);
    free(args);
  }

  /*
    Bluring function that does bluring in a sector by sector manner.
    The number of sectors is given in the sectors parameter, which has to
    be a square number, as the image will be split into equal rectangles.
    The rectangles at the edges of the image may have a slightly smaller
    size than the other rectangles, if the image width and height are not
    divisible by the root of the number of sectors.
  */
  static void blur_sector_by_sector(struct picture *pic, int sectors){
    int split = sqrt(sectors);
    struct picture tmp;
    tmp.img = copy_image(pic->img);
    tmp.width = pic->width;
    tmp.height = pic->height;
    pthread_t *threads = (pthread_t *) malloc(split * split * sizeof(pthread_t));
    if (threads == NULL) {
      printf("Ran out of memory for malloc!\n");
      return;
    }
    struct task_args *args = (struct task_args *) malloc(split * split * sizeof(struct task_args));
    if (args == NULL) {
      printf("Ran out of memory for malloc!\n");
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
      // Edge case when image width is not divisible by the split
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
    // Edge case when image height is not divisible by the split
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
    // Edge case when image width and height are not divisible by the split
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
  static bool check_correctness(char *file) {
    char command[500];
    sprintf(command, "./picture_compare %s %s", files_names[0], file);
    if (system(command) != 0) {
      printf("\nPicture %s was not blurred correctly!\n", file);
      return false;
    } else {
      return true;
    }
  }


  int main(int argc, char **argv){

    struct picture *original = (struct picture *) malloc(sizeof(struct picture));
    if (original == NULL) {
      printf("Ran out of memory for malloc!\n");
      return EXIT_FAILURE;
    }
    init_picture_from_file(original, argv[1]);
    struct picture *pic = (struct picture *) malloc(sizeof(struct picture));
    if (pic == NULL) {
      printf("Ran out of memory for malloc!\n");
      return EXIT_FAILURE;
    }
    pic->width = original->width;
    pic->height = original->height;

    struct timespec start;
    struct timespec end;
    double diff;
    double sum = 0;
    bool correctness = true;
    double average[IMPLEMENTATIONS] = {0, 0, 0, 0, 0, 0};

    int repeats = atoi(argv[3]);

    // Creates a file to store the results of the experiment.
    FILE *f = fopen(argv[2], "w");
    if (f == NULL){
      printf("Error opening file!\n");
      return EXIT_FAILURE;
    }

    printf("\nBegining the Blur Experiment: \n");
    fprintf(f, "\nBegining the Blur Experiment: \n");

    // Loops over the number of implementations 
    for (int r = 0; r < IMPLEMENTATIONS; r++) {
      printf("\n%s Implementation: \n\n", implemenation_name[r]);
      fprintf(f, "\n%s Implementation: \n\n", implemenation_name[r]);
      for (int i = 0; i < repeats; i++){
        // Copies the image to ensure the bluring occurs on the same one each time
        pic->img = copy_image(original->img);
        // Measure time using CLOCK_REALTIME
        clock_gettime(CLOCK_REALTIME, &start);
        // Choose which function to use
        switch (r){
          case 5:
            functions[r] (pic, 4);
            break;

          case 6:
            functions[r] (pic, 100);
            break;

          default:
            functions[r] (pic);
            break;
        }
        clock_gettime(CLOCK_REALTIME, &end);

        save_picture_to_file(pic, files_names[r]);
        // Checks the correctness of the blurred files
        if (r != 0) {
          correctness &= check_correctness(files_names[r]);
        }
        // Compute the time of the iteration
        diff = ((double) (end.tv_sec - start.tv_sec)) + ((double) (end.tv_nsec - start.tv_nsec)) / BILLION;
        fprintf(f, "Time in iteration %d for %s: %lf seconds\n", i, implemenation_name[r], diff);
        sum += diff;
        clear_picture(pic);
      }
      // Computes the average time of all iterations.
      average[r] = sum / repeats;
      sum = 0;
      printf("Average time for %s: %lf seconds\n\n", implemenation_name[r], average[r]);
      fprintf(f, "Average time for %s: %lf seconds\n\n", implemenation_name[r], average[r]);
    }


    printf("\nResults after %d repeats each: \n\n", repeats);
    fprintf(f, "\nResults after %d repeats each: \n\n", repeats);
    
    for (int r = 0; r < IMPLEMENTATIONS; r++) {
      printf("Average time for %s: %lf seconds\n", implemenation_name[r], average[r]);
      fprintf(f, "Average time for %s: %lf seconds\n", implemenation_name[r], average[r]);
    }


    if (correctness) {
      fprintf (f, "\nAll images were blurred correctly.\n");
      printf ("\nAll images were blurred correctly.\n");
    } else {
      fprintf (f, "\nNot all images were blurred correctly.\n");
      printf ("\nNot all images were blurred correctly.\n");
    }

    free(pic);

    clear_picture(original);
    free(original);

    fclose(f);

    return EXIT_SUCCESS;
  }
