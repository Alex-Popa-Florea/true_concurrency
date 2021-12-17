#ifndef BLUREXPRMT_H
#define BLUREXPRMT_H

#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"

  /*
    Struct to store the arguments needed for blurring a chunk of pixels.
    These arguments are generic to all blurring functions.
  */
  struct task_args {
    struct picture *pic;    // The picture that needs to be blurred
    struct picture tmp;     // The refrence from which to compute the blurred pixels
    int i_start;            // The i position on which to start the blurring
    int i_end;              // The i position on which to end the blurring
    int j_start;            // The j position on which to start the blurring, inclusive
    int j_end;              // The j position on which to end the blurring, inclusive
  };    

  /*
    Generic function pointer for blur functions.
  */
  typedef void (*blur_func)();

  /*
    Stack element, with pointer to the given task and the next element on the stack.
  */
  typedef struct list_elem {
    struct task_args *task; 
    struct list_elem *next;
  } list_elem_t;

  /*
    Simple implementation of a stack as a linked list.
  */
  typedef struct {
    struct list_elem *head;
    int size;
  } list_t;

#endif
