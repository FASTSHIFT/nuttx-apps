/****************************************************************************
 * apps/system/perf-tools/evlist.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <nuttx/list.h>

#include "evlist.h"
#include "evsel.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void perf_evlist_init(FAR struct perf_evlist_s *evlist)
{
  list_initialize(&evlist->entries);
  evlist->nr_entries = 0;
}

static void perf_evlist_add(FAR struct perf_evlist_s *evlist,
                            FAR struct perf_evsel_s *evsel)
{
  list_add_tail(&evlist->entries, &evsel->node);
  evlist->nr_entries += 1;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void evlist_init(FAR struct evlist_s *evlist)
{
  perf_evlist_init(&evlist->core);
}

FAR struct evlist_s *evlist_new(void)
{
  FAR struct evlist_s *evlist = zalloc(sizeof(struct evlist_s));

  if (evlist != NULL)
    {
      evlist_init(evlist);
    }

  return evlist;
}

int evlist_delete(FAR struct evlist_s *evlist)
{
  if (evlist == NULL)
    {
      return -EINVAL;
    }

  evlist->core.nr_entries = 0;

  free(evlist->attrs);
  free(evlist);

  return 0;
}

void evlist_add(FAR struct evlist_s *evlist, FAR struct evsel_s *entry)
{
  perf_evlist_add(&evlist->core, &entry->core);
  entry->evlist = evlist;
}

int evlist_add_attrs(FAR struct evlist_s *evlist,
                     FAR struct perf_event_attr_s *attrs,
                     size_t nr_attrs, int cpu)
{
  FAR struct evsel_s *evsel;
  FAR struct list_node *node;
  size_t i;

  for (i = 0; i < nr_attrs; i++)
    {
      evsel = evsel_new(attrs + i);
      if (evsel == NULL)
        {
          goto out_delete;
        }

      evsel->core.cpu = cpu;
      evsel->core.pid = evlist->pid;
      evlist_add(evlist, evsel);
    }

  return 0;

out_delete:
  list_for_every(&(evlist->core.entries), node)
    {
      FAR struct perf_evsel_s *psel;
      psel = container_of(node, struct perf_evsel_s, node);
      evsel = container_of(psel, struct evsel_s, core);
      evsel_delete(evsel);
    }

  return -ENOMEM;
}

int evlist_print_counters(FAR struct evlist_s *evlist)
{
  FAR struct perf_evsel_s *psel = NULL;
  FAR struct evsel_s *evsel = NULL;
  FAR struct list_node *node = NULL;
  FAR uint64_t *count = NULL;
  int i = 0;
  int j = 0;

  if (evlist->system_wide)
    {
      int num = evlist->core.nr_entries / CONFIG_SMP_NCPUS;
      count = zalloc(evlist->core.nr_entries * sizeof(uint64_t) * 2);
      if (!count)
        {
          printf("Failed to allocate temporary storage!\n");
          return -ENOMEM;
        }

      list_for_every(&(evlist->core.entries), node)
        {
          psel = container_of(node, struct perf_evsel_s, node);
          evsel = container_of(psel, struct evsel_s, core);
          count[i++] = evsel->core.count;
          count[i++] = evsel->core.attr.config;
        }

      for (i = 0; i < num * 2; i += 2)
        {
          uint64_t counter = 0;
          for (j = 0; j < CONFIG_SMP_NCPUS; j++)
            {
              counter += count[i + j * num * 2];
            }

          printf("%10ld\t %s\n", counter, evsel_get_hw_name(count[i + 1]));
        }

      free(count);
    }
  else
    {
      list_for_every(&(evlist->core.entries), node)
        {
          psel = container_of(node, struct perf_evsel_s, node);
          evsel = container_of(psel, struct evsel_s, core);
          printf("%10ld\t %s\n", evsel->core.count,
                  evsel_get_hw_name(evsel->core.attr.config));
        }
    }

  return 0;
}
