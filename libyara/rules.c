/*
Copyright (c) 2013. The YARA Authors. All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <string.h>
#include <ctype.h>

#include <yara/error.h>
#include <yara/filemap.h>
#include <yara/mem.h>
#include <yara/proc.h>
#include <yara/rules.h>
#include <yara/utils.h>
#include <yara/globals.h>
#include <yara/scan.h>
#include <yara/scanner.h>
#include <yara/compiler.h>


YR_API int yr_rules_define_integer_variable(
    YR_RULES* rules,
    const char* identifier,
    int64_t value)
{
  YR_EXTERNAL_VARIABLE* external;

  if (identifier == NULL)
    return ERROR_INVALID_ARGUMENT;

  external = rules->externals_list_head;

  while (!EXTERNAL_VARIABLE_IS_NULL(external))
  {
    if (strcmp(external->identifier, identifier) == 0)
    {
      if (external->type != EXTERNAL_VARIABLE_TYPE_INTEGER)
        return ERROR_INVALID_EXTERNAL_VARIABLE_TYPE;

      external->value.i = value;
      return ERROR_SUCCESS;
    }

    external++;
  }

  return ERROR_INVALID_ARGUMENT;
}


YR_API int yr_rules_define_boolean_variable(
    YR_RULES* rules,
    const char* identifier,
    int value)
{
  YR_EXTERNAL_VARIABLE* external;

  if (identifier == NULL)
    return ERROR_INVALID_ARGUMENT;

  external = rules->externals_list_head;

  while (!EXTERNAL_VARIABLE_IS_NULL(external))
  {
    if (strcmp(external->identifier, identifier) == 0)
    {
      if (external->type != EXTERNAL_VARIABLE_TYPE_BOOLEAN)
        return ERROR_INVALID_EXTERNAL_VARIABLE_TYPE;

      external->value.i = value;
      return ERROR_SUCCESS;
    }

    external++;
  }

  return ERROR_INVALID_ARGUMENT;
}


YR_API int yr_rules_define_float_variable(
    YR_RULES* rules,
    const char* identifier,
    double value)
{
  YR_EXTERNAL_VARIABLE* external;

  if (identifier == NULL)
    return ERROR_INVALID_ARGUMENT;

  external = rules->externals_list_head;

  while (!EXTERNAL_VARIABLE_IS_NULL(external))
  {
    if (strcmp(external->identifier, identifier) == 0)
    {
      if (external->type != EXTERNAL_VARIABLE_TYPE_FLOAT)
        return ERROR_INVALID_EXTERNAL_VARIABLE_TYPE;

      external->value.f = value;
      return ERROR_SUCCESS;
    }

    external++;
  }

  return ERROR_INVALID_ARGUMENT;
}


YR_API int yr_rules_define_string_variable(
    YR_RULES* rules,
    const char* identifier,
    const char* value)
{
  YR_EXTERNAL_VARIABLE* external;

  if (identifier == NULL || value == NULL)
    return ERROR_INVALID_ARGUMENT;

  external = rules->externals_list_head;

  while (!EXTERNAL_VARIABLE_IS_NULL(external))
  {
    if (strcmp(external->identifier, identifier) == 0)
    {
      if (external->type != EXTERNAL_VARIABLE_TYPE_STRING &&
          external->type != EXTERNAL_VARIABLE_TYPE_MALLOC_STRING)
        return ERROR_INVALID_EXTERNAL_VARIABLE_TYPE;

      if (external->type == EXTERNAL_VARIABLE_TYPE_MALLOC_STRING &&
          external->value.s != NULL)
      {
        yr_free(external->value.s);
      }

      external->type = EXTERNAL_VARIABLE_TYPE_MALLOC_STRING;
      external->value.s = yr_strdup(value);

      if (external->value.s == NULL)
        return ERROR_INSUFFICIENT_MEMORY;
      else
        return ERROR_SUCCESS;
    }

    external++;
  }

  return ERROR_INVALID_ARGUMENT;
}


YR_API int yr_rules_scan_mem_blocks(
    YR_RULES* rules,
    YR_MEMORY_BLOCK_ITERATOR* iterator,
    int flags,
    YR_CALLBACK_FUNC callback,
    void* user_data,
    int timeout)
{
  YR_SCANNER* scanner;
  int result;

  FAIL_ON_ERROR(yr_scanner_create(rules, &scanner));

  yr_scanner_set_callback(scanner, callback, user_data);
  yr_scanner_set_timeout(scanner, timeout);
  yr_scanner_set_flags(scanner, flags);

  result = yr_scanner_scan_mem_blocks(scanner, iterator);

  yr_scanner_destroy(scanner);

  return result;
}


static YR_MEMORY_BLOCK* _yr_get_first_block(
    YR_MEMORY_BLOCK_ITERATOR* iterator)
{
  yr_test_count_get_block ++;
  return (YR_MEMORY_BLOCK*) iterator->context;
}


static YR_MEMORY_BLOCK* _yr_get_next_block(
    YR_MEMORY_BLOCK_ITERATOR* iterator)
{
  yr_test_count_get_block ++;
  return NULL;
}


static const uint8_t* _yr_fetch_block_data(
    YR_MEMORY_BLOCK* block)
{
  return (const uint8_t*) block->context;
}



static YR_MEMORY_BLOCK* _yr_test_mem_block_size_get_next_block(
    YR_MEMORY_BLOCK_ITERATOR* iterator)
{
  YR_PROC_ITERATOR_CTX* context = (YR_PROC_ITERATOR_CTX*) iterator->context;

  YR_MEMORY_BLOCK* result;
  uint64_t overlap;

  yr_test_count_get_block ++;

  if (0 == context->current_block.size) {
    overlap = 0;
    context->current_block.size =
      (context->buffer_size < yr_test_mem_block_size) ?
       context->buffer_size :
       yr_test_mem_block_size;
    result = &context->current_block;
  }
  else {
    overlap = yr_test_mem_block_size_overlap;
    context->current_block.base +=
      (0 == context->current_block.base) ?
       0 :
       overlap;
    context->current_block.base +=
      (context->current_block.base < context->buffer_size) ?
       yr_test_mem_block_size :
       0;
    result = (context->current_block.base < context->buffer_size) ?
             &context->current_block : NULL;
    context->current_block.size =
      ((context->buffer_size - context->current_block.base) < yr_test_mem_block_size) ?
        context->buffer_size - context->current_block.base + overlap :
        yr_test_mem_block_size + overlap;
    context->current_block.base -= overlap;
  }

  YR_TEST_FPRINTF(2, stderr, "+ %s() {} = %p // "
    ".base=(0x%lx or %'lu) of (0x%lx or %'lu) .size=%'lu, both including %'lu block overlap%s\n",
    __FUNCTION__, result,
    context->current_block.base,
    context->current_block.base,
    context->buffer_size,
    context->buffer_size,
    context->current_block.size,
    overlap, overlap ? "" : " (note: 1st block overlap always 0)");

  return result;
}


static YR_MEMORY_BLOCK* _yr_test_mem_block_size_get_first_block(
    YR_MEMORY_BLOCK_ITERATOR* iterator)
{
  YR_TEST_FPRINTF(2, stderr, "+ %s() {} // "
    "wrapping _yr_test_mem_block_size_get_next_block()\n", __FUNCTION__);

  yr_test_count_get_block ++;

  YR_PROC_ITERATOR_CTX* context = (YR_PROC_ITERATOR_CTX*) iterator->context;
  context->current_block.base = 0;
  context->current_block.size = 0;
  return _yr_test_mem_block_size_get_next_block(iterator);
}


static const uint8_t* _yr_test_mem_block_size_fetch_block_data(
    YR_MEMORY_BLOCK* block)
{
  YR_PROC_ITERATOR_CTX* context = (YR_PROC_ITERATOR_CTX*) block->context;

  #if YR_TEST_VERBOSITY > 0
  uint64_t overlap = context->current_block.base > 0 ? yr_test_mem_block_size_overlap : 0;
  #endif
  YR_TEST_FPRINTF(2, stderr, "+ %s() {} = %p = %p + 0x%lx base including %'lu overlap%s\n",
    __FUNCTION__,
    context->buffer + context->current_block.base - overlap,
    context->buffer,
    context->current_block.base,
    overlap, overlap ? "" : " (note: 1st block overlap always 0)");

  return context->buffer + context->current_block.base;
}


YR_API int yr_rules_scan_mem(
    YR_RULES* rules,
    const uint8_t* buffer,
    size_t buffer_size,
    int flags,
    YR_CALLBACK_FUNC callback,
    void* user_data,
    int timeout)
{
  YR_MEMORY_BLOCK block;
  YR_MEMORY_BLOCK_ITERATOR iterator;
  YR_PROC_ITERATOR_CTX context;

  if (yr_test_mem_block_size)
  {
    YR_TEST_FPRINTF(2, stderr, "+ %s(buffer=%p buffer_size=%'lu) {} // "
      "yr_test_mem_block_size=%'lu\n",
       __FUNCTION__,
       buffer,
       buffer_size,
       yr_test_mem_block_size);

    iterator.context = &context;
    iterator.first = _yr_test_mem_block_size_get_first_block;
    iterator.next = _yr_test_mem_block_size_get_next_block;

    context.buffer = buffer;
    context.buffer_size = buffer_size;
    context.current_block.base = 0;
    context.current_block.size = 0;
    context.current_block.context = &context;
    context.current_block.fetch_data = _yr_test_mem_block_size_fetch_block_data;
  }
  else
  {
    block.size = buffer_size;
    block.base = 0;
    block.fetch_data = _yr_fetch_block_data;
    block.context = (void*) buffer;

    iterator.context = &block;
    iterator.first = _yr_get_first_block;
    iterator.next = _yr_get_next_block;
  }

  return yr_rules_scan_mem_blocks(
      rules,
      &iterator,
      flags,
      callback,
      user_data,
      timeout);
}


YR_API int yr_rules_scan_file(
    YR_RULES* rules,
    const char* filename,
    int flags,
    YR_CALLBACK_FUNC callback,
    void* user_data,
    int timeout)
{
  YR_MAPPED_FILE mfile;

  int result = yr_filemap_map(filename, &mfile);

  if (result == ERROR_SUCCESS)
  {
    result = yr_rules_scan_mem(
        rules,
        mfile.data,
        mfile.size,
        flags,
        callback,
        user_data,
        timeout);

    yr_filemap_unmap(&mfile);
  }

  return result;
}


YR_API int yr_rules_scan_fd(
    YR_RULES* rules,
    YR_FILE_DESCRIPTOR fd,
    int flags,
    YR_CALLBACK_FUNC callback,
    void* user_data,
    int timeout)
{
  YR_MAPPED_FILE mfile;

  int result = yr_filemap_map_fd(fd, 0, 0, &mfile);

  if (result == ERROR_SUCCESS)
  {
    result = yr_rules_scan_mem(
        rules,
        mfile.data,
        mfile.size,
        flags,
        callback,
        user_data,
        timeout);

    yr_filemap_unmap_fd(&mfile);
  }

  return result;
}


YR_API int yr_rules_scan_proc(
    YR_RULES* rules,
    int pid,
    int flags,
    YR_CALLBACK_FUNC callback,
    void* user_data,
    int timeout)
{
  YR_TEST_FPRINTF(2, stderr, "+ %s() {}\n", __FUNCTION__);

  YR_MEMORY_BLOCK_ITERATOR iterator;

  int result = yr_process_open_iterator(
      pid,
      &iterator);

  if (result == ERROR_SUCCESS)
  {
    result = yr_rules_scan_mem_blocks(
        rules,
        &iterator,
        flags | SCAN_FLAGS_PROCESS_MEMORY,
        callback,
        user_data,
        timeout);

    yr_process_close_iterator(&iterator);
  }

  return result;
}


int yr_rules_from_arena(
    YR_ARENA* arena,
    YR_RULES** rules)
{
  YR_RULES* new_rules = (YR_RULES*) yr_malloc(sizeof(YR_RULES));

  if (new_rules == NULL)
    return ERROR_INSUFFICIENT_MEMORY;

  YR_SUMMARY* summary = (YR_SUMMARY*) yr_arena_get_ptr(
      arena, YR_SUMMARY_SECTION, 0);

  // Now YR_RULES relies on this arena, let's increment the arena's
  // reference count so that if the original owner of the arena calls
  // yr_arena_destroy the arena is not destroyed.
  yr_arena_acquire(arena);

  new_rules->arena = arena;
  new_rules->num_rules = summary->num_rules;
  new_rules->num_strings = summary->num_strings;
  new_rules->num_namespaces = summary->num_namespaces;

  new_rules->rules_list_head = yr_arena_get_ptr(
      arena, YR_RULES_TABLE, 0);

  new_rules->strings_list_head = yr_arena_get_ptr(
      arena, YR_STRINGS_TABLE, 0);

  new_rules->externals_list_head = yr_arena_get_ptr(
      arena, YR_EXTERNAL_VARIABLES_TABLE, 0);

  new_rules->ac_transition_table = yr_arena_get_ptr(
      arena, YR_AC_TRANSITION_TABLE, 0);

  new_rules->ac_match_table = yr_arena_get_ptr(
      arena, YR_AC_STATE_MATCHES_TABLE, 0);

  new_rules->ac_match_pool = yr_arena_get_ptr(
      arena, YR_AC_STATE_MATCHES_POOL, 0);

  new_rules->code_start = yr_arena_get_ptr(
      arena, YR_CODE_SECTION, 0);

  *rules = new_rules;

  return ERROR_SUCCESS;
}



YR_API int yr_rules_load_stream(
    YR_STREAM* stream,
    YR_RULES** rules)
{
  YR_ARENA* arena;

  // Load the arena's data the stream. We are the owners of the arena.
  FAIL_ON_ERROR(yr_arena_load_stream(stream, &arena));

  // Create the YR_RULES object from the arena, this makes YR_RULES owner
  // of the arena too.
  FAIL_ON_ERROR(yr_rules_from_arena(arena, rules));

  // Release our ownership so that YR_RULES is the single owner. This way the
  // arena is destroyed when YR_RULES is destroyed.
  yr_arena_release(arena);

  return ERROR_SUCCESS;
}


YR_API int yr_rules_load(
    const char* filename,
    YR_RULES** rules)
{
  int result;

  YR_STREAM stream;
  FILE* fh = fopen(filename, "rb");

  if (fh == NULL)
    return ERROR_COULD_NOT_OPEN_FILE;

  stream.user_data = fh;
  stream.read = (YR_STREAM_READ_FUNC) fread;

  result = yr_rules_load_stream(&stream, rules);

  fclose(fh);
  return result;
}


YR_API int yr_rules_save_stream(
    YR_RULES* rules,
    YR_STREAM* stream)
{
  return yr_arena_save_stream(rules->arena, stream);
}


YR_API int yr_rules_save(
    YR_RULES* rules,
    const char* filename)
{
  int result;

  YR_STREAM stream;
  FILE* fh = fopen(filename, "wb");

  if (fh == NULL)
    return ERROR_COULD_NOT_OPEN_FILE;

  stream.user_data = fh;
  stream.write = (YR_STREAM_WRITE_FUNC) fwrite;

  result = yr_rules_save_stream(rules, &stream);

  fclose(fh);
  return result;
}


static int _uint32_cmp (
    const void * a,
    const void * b)
{
   return (*(uint32_t*) a - *(uint32_t*) b);
}

YR_API int yr_rules_get_stats(
    YR_RULES* rules,
    YR_RULES_STATS *stats)
{
  memset(stats, 0, sizeof(YR_RULES_STATS));

  stats->ac_tables_size = yr_arena_get_current_offset(
      rules->arena, YR_AC_TRANSITION_TABLE) / sizeof(YR_AC_TRANSITION);

  uint32_t* match_list_lengths = (uint32_t*) yr_malloc(
      sizeof(uint32_t) * stats->ac_tables_size);

  if (match_list_lengths == NULL)
    return ERROR_INSUFFICIENT_MEMORY;

  stats->num_rules = rules->num_rules;
  stats->num_strings = rules->num_strings;

  float match_list_length_sum = 0;
  int c = 0;

  for (int i = 0; i < stats->ac_tables_size; i++)
  {
    int match_list_length = 0;

    if (rules->ac_match_table[i] != 0)
    {
      YR_AC_MATCH *m = &rules->ac_match_pool[rules->ac_match_table[i]-1];

      while (m != NULL)
      {
        match_list_length++;
        stats->ac_matches++;
        m = m->next;
      }
    }

    if (i == 0)
      stats->ac_root_match_list_length = match_list_length;

    match_list_length_sum += match_list_length;

    if (match_list_length > 0)
    {
      match_list_lengths[c] = match_list_length;
      c++;
    }
  }

  if (c == 0)
  {
    yr_free(match_list_lengths);
    return ERROR_SUCCESS;
  }

  // sort match_list_lengths in increasing order for computing percentiles.
  qsort(match_list_lengths, c, sizeof(match_list_lengths[0]), _uint32_cmp);

  for (int i = 0; i < 100; i++)
  {
    if (i < c)
      stats->top_ac_match_list_lengths[i] = match_list_lengths[c-i-1];
    else
      stats->top_ac_match_list_lengths[i] = 0;
  }

  stats->ac_average_match_list_length = match_list_length_sum / c;
  stats->ac_match_list_length_pctls[0] = match_list_lengths[0];
  stats->ac_match_list_length_pctls[100] = match_list_lengths[c-1];

  for (int i = 1; i < 100; i++)
    stats->ac_match_list_length_pctls[i] = match_list_lengths[(c * i) / 100];

  yr_free(match_list_lengths);

  return ERROR_SUCCESS;
}


YR_API int yr_rules_destroy(
    YR_RULES* rules)
{
  YR_EXTERNAL_VARIABLE* external = rules->externals_list_head;

  while (!EXTERNAL_VARIABLE_IS_NULL(external))
  {
    if (external->type == EXTERNAL_VARIABLE_TYPE_MALLOC_STRING)
      yr_free(external->value.s);

    external++;
  }

  yr_arena_release(rules->arena);
  yr_free(rules);

  return ERROR_SUCCESS;
}

YR_API void yr_rule_disable(
    YR_RULE* rule)
{
  YR_STRING* string;

  rule->flags |= RULE_FLAGS_DISABLED;

  yr_rule_strings_foreach(rule, string)
  {
    string->flags |= STRING_FLAGS_DISABLED;
  }
}


YR_API void yr_rule_enable(
    YR_RULE* rule)
{
  YR_STRING* string;

  rule->flags &= ~RULE_FLAGS_DISABLED;

  yr_rule_strings_foreach(rule, string)
  {
    string->flags &= ~STRING_FLAGS_DISABLED;
  }
}
