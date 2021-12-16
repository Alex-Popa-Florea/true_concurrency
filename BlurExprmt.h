#ifndef BLUREXPRMT_H
#define BLUREXPRMT_H

#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"

  struct task_args {
    struct picture *pic;
    struct picture tmp;
    int i_start;
    int i_end;
    int j_start;
    int j_end;
  };    

  typedef void (*blur_func)();

  typedef struct list_elem {
    struct task_args *task; 
    struct list_elem *next;
  } list_elem_t;

  typedef struct {
    struct list_elem *head;
    int size;
  } list_t;

#endif
