#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <vector>

#include "stats.h"
#include "interface.h"
#include "thread.h"
#include "zesto-core.h"

extern struct stat_sdb_t *sim_sdb;
extern void sim_reg_stats(struct thread_t ** threads, struct stat_sdb_t *sdb);
extern void sim_print_stats(FILE *fd);

extern tick_t sim_cycle;
extern const char *sim_simout;

static unsigned long long slice_start_cycle = 0;
static unsigned long long slice_end_cycle = 0;
static unsigned long long slice_start_icount = 0;

static std::vector<struct stat_sdb_t*> all_stats;

void start_slice(unsigned int slice_num)
{
   int i = 0;
   core_t* core = cores[i];
   struct thread_t *thread = core->current_thread;

   /* create stats database for this slice */
   struct stat_sdb_t* new_stat_db = stat_new();

   /* register new database with stat counters */
   sim_reg_stats(threads, new_stat_db);

   all_stats.push_back(new_stat_db);

   sim_cycle = slice_end_cycle;
   cores[i]->stat.final_sim_cycle = slice_end_cycle;
   slice_start_cycle = sim_cycle;
   slice_start_icount = thread->stat.num_insn;
}

//XXX: REMOVE ME
static double total_cpi = 0.0;

void end_slice(unsigned int slice_num, unsigned long long feeder_slice_length, unsigned long long slice_weight_times_1000)
{
   int i = 0;

   struct stat_sdb_t* curr_sdb = all_stats.back();

   slice_end_cycle = sim_cycle;

   /* Ugh, this feels very dirty. Have to make sure we don't forget a cycle stat.
      The reason for doing this is that cycle counts increasing monotonously is
      an important invariant all around and reseting the cycle counts on every
      slice causes hell to break loose with caches an you name it */
   sim_cycle -= slice_start_cycle;
   cores[i]->stat.final_sim_cycle -= slice_start_cycle;

   double weight = (double)slice_weight_times_1000 / 100000.0;
   curr_sdb->slice_weight = weight;

   unsigned long long slice_length = cores[i]->current_thread->stat.num_insn - slice_start_icount;

   // Check if simulator and feeder measure instruction counts in the same way
   // (they may not count REP-s the same, f.e.)
   double slice_length_diff = 1.0 - ((double)slice_length/(double)feeder_slice_length);
   if ((fabs(slice_length_diff) > 0.01) && (feeder_slice_length > 0))
     fprintf(stderr, "Significant slice length different between sim and feeder! Slice: %u, sim_length: %llu, feeder_length: %llu\n", slice_num, slice_length, feeder_slice_length);

   stat_save_stats(curr_sdb);

   /* Print slice stats separately */
   if (/*verbose && */ sim_simout != NULL)
   {
     char curr_filename[PATH_MAX];
     sprintf(curr_filename, "%s.slice.%d", sim_simout, slice_num);
     FILE* curr_fd = fopen(curr_filename, "w");
     if (curr_fd != NULL)
     {
       stat_print_stats(curr_sdb, curr_fd);
       fclose(curr_fd);
     }
   }

   double n_cycles = (double)sim_cycle;
   double n_insn = (double)(cores[0]->current_thread->stat.num_insn - slice_start_icount);
   double n_pin_n_insn = (double)slice_length;
   double curr_cpi = weight * n_cycles / n_insn;
   double curr_ipc = 1.0 / curr_cpi;

   total_cpi += curr_cpi;
   fprintf(stderr, "Slice %d, weight: %.4f, IPC/weight: %.2f, n_insn: %.0f, n_insn_pin: %.0f, n_cycles: %.0f\n", slice_num, weight, curr_ipc, n_insn, n_pin_n_insn, n_cycles);
   fprintf(stderr, "Average IPC: %.2f\n", 1.0/total_cpi);
}

void scale_all_slices(void)
{
   int n_slices = all_stats.size();

   /* No active slices */
   if (n_slices == 0)
     return;

   int i;
   /* create pointers to track stat in every db */
   struct stat_stat_t **curr_stats = (struct stat_stat_t**) malloc(n_slices * sizeof(struct stat_stat_t*));

   /* create stats database for averages */
   struct stat_sdb_t* avg_stat_db = stat_new();

   /* register new database with stat counters */
   sim_reg_stats(threads, avg_stat_db);

   /* ... and set it as the default that gets output */
   sim_sdb = avg_stat_db;

   /* Init pointers to all stat dbs */
   struct stat_stat_t *avg_curr_stat = avg_stat_db->stats;
   for (i=0; i < n_slices; i++)
     curr_stats[i] = all_stats[i]->stats;

   while (avg_curr_stat) 
   {
     for (i=0; i < n_slices; i++)
     {
       /* Scale current stat by slice weight (note this is destructive) */
       stat_scale_stat(curr_stats[i], all_stats[i]->slice_weight);

       /* And store accumulated value in average */
       stat_accum_stat(avg_curr_stat, curr_stats[i]);

       curr_stats[i] = curr_stats[i]->next;
     }

     avg_curr_stat = avg_curr_stat->next;
   }

   free(curr_stats);
}