#include "ordered_array.h"

int8_t standard_lessthan_predicate(type_t a, type_t b) {
  return a < b;
}

ordered_array_t create_ordered_array(uint32_t max_size, lessthan_predicate_t less_than) {
  ordered_array_t to_ret;

  to_ret.array = (type_t*)kmalloc(max_size * sizeof(type_t));
  memset(to_ret.array, 0, max_size * sizeof(type_t));
  to_ret.size = 0;
  to_ret.max_size = max_size;
  to_ret.less_than = less_than;

  return to_ret;
}

ordered_array_t place_ordered_array(void *addr, uint32_t max_size, lessthan_predicate_t less_than) {
  ordered_array_t to_ret;

  to_ret.array = (type_t*)addr;
  memset(to_ret.array, 0, sizeof(type_t) * max_size);
  to_ret.size = 0;
  to_ret.max_size = max_size;
  to_ret.less_than = less_than;

  return to_ret;
}

void destroy_ordered_array(ordered_array_t *array) {
  // TODO
}

#include "monitor.h"

void insert_ordered_array(type_t item, ordered_array_t *array) {
  ASSERT(array->less_than);

  int32_t iterator = 0;
  for (; iterator < array->size &&
      array->less_than(item, array->array[iterator]); iterator++);

  if (iterator == array->size) {
    array->array[array->size++] = item;
  } else {
    int32_t end;
    for (end = array->size - 1; end >= iterator; end--) {
      array->array[end + 1] = array->array[end];
    }
    array->array[iterator] = item;

    array->size++;
  }
}

type_t lookup_ordered_array(uint32_t i, ordered_array_t *array) {
  ASSERT(i < array->size);
  return array->array[i];
}

void remove_ordered_array(uint32_t i, ordered_array_t *array) {
  for (; i < array->size; i++) {
    array->array[i] = array->array[i + 1];
  }

  array->size--;
}
