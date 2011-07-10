#include <stdio.h>
#include <limits.h>
#include <list>

#include "stats.h"
#include "thread.h"
#include "zesto-core.h"

extern struct stat_sdb_t *sim_sdb;
extern void sim_reg_stats(struct thread_t ** threads, struct stat_sdb_t *sdb);
extern void sim_print_stats(FILE *fd);

extern tick_t sim_cycle;
extern const char *sim_simout;

static unsigned long long slice_start_cycle = 0;
static unsigned long long slice_start_icount = 0;

static std::list<struct stat_sdb_t*> all_stats;

void Zesto_SliceStart(unsigned int slice_num)
{
   /* create stats database for this slice */
   struct stat_sdb_t* new_stat_db = stat_new();

   /* register new database with stat counters */
   sim_reg_stats(threads, new_stat_db);

   all_stats.push_back(new_stat_db);

   int i = 0;
   core_t* core = cores[i];
   struct thread_t *thread = core->current_thread;

   slice_start_cycle = sim_cycle;
   slice_start_icount = thread->stat.num_insn;
}

//XXX: REMOVE ME
static double total_cpi = 0.0;

void Zesto_SliceEnd(unsigned int slice_num, unsigned long long feeder_slice_length, unsigned long long slice_weight_times_1000)
{
   int i = 0;

   struct stat_sdb_t* curr_sdb = all_stats.back();

   double weight = (double)slice_weight_times_1000 / 100000.0;
   curr_sdb->slice_weight = weight;

   unsigned long long slice_length = cores[i]->current_thread->stat.num_insn - slice_start_icount;

   // Check if simulator and feeder measure instruction counts in the same way
   // (they may not count REP-s the same, f.e.)
   double slice_length_diff = 1.0 - ((double)slice_length/(double)feeder_slice_length);
   if (abs(slice_length_diff) > 0.01)
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

   double n_cycles = (double)(sim_cycle - slice_start_cycle);
   double n_insn = (double)(cores[0]->current_thread->stat.num_insn - slice_start_icount);
   double n_pin_n_insn = (double)slice_length;
   double curr_cpi = weight * n_cycles / n_insn;
   double curr_ipc = 1.0 / curr_cpi;

   total_cpi += curr_cpi;
   fprintf(stderr, "Slice %d, weigth: %.4f, IPC: %.2f, n_insn: %.0f, n_insn_pin: %.0f, n_cycles: %.0f\n", slice_num, weight, curr_ipc, n_insn, n_pin_n_insn, n_cycles);
   fprintf(stderr, "Average IPC: %.2f\n", 1.0/total_cpi);
}

void Zesto_Destroy(void)
{
   static std::list<struct stat_sdb_t*>::iterator it;

   int n_slices = all_stats.size();

   /* No active slices, just print stats */
   if (n_slices == 0) {
     sim_print_stats(stderr);
     return;
   }

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
   for (it = all_stats.begin(), i=0; it != all_stats.end(); it++, i++)
     curr_stats[i] = (*it)->stats;

   while (avg_curr_stat) 
   {
     for (it = all_stats.begin(), i=0; it != all_stats.end(); it++, i++)
     {
       /* Scale current stat by slice weight (note this is destructive) */
       stat_scale_stat(curr_stats[i], (*it)->slice_weight);

       /* And store accumulated value in average */
       stat_accum_stat(curr_stats[i], avg_curr_stat);

       curr_stats[i] = curr_stats[i]->next;
     }

     avg_curr_stat = avg_curr_stat->next;
   }

   free(curr_stats);

   sim_print_stats(stderr);
}
