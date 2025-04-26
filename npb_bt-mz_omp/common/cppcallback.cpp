#include <inttypes.h>
#include <omp.h>
#include <omp-tools.h>
#include <stdio.h>
#include <map>
#include <deque>
#include <tuple>

#ifndef _TOOL_PREFIX
#define _TOOL_PREFIX ""
#endif

struct config {
  int best_num_threads=0;
  int cur_num_threads=0;
  double best_time=0.0;
  double cur_time=0.0;
  std::deque<int> parconf={1,2,4,8,16,24,32,40,48};  
};


static std::map<uint64_t,struct config> par_region_info[48];
//#pragma omp threadprivate (par_region_info)

extern ompt_get_thread_data_t ompt_get_thread_data;
extern ompt_get_unique_id_t ompt_get_unique_id;


extern "C" {

void on_ompt_callback_parallel_begin(
    ompt_data_t *encountering_task_data,
    const ompt_frame_t *encountering_task_frame, ompt_data_t *parallel_data,
    uint32_t requested_team_size, int flag, const void *codeptr_ra) {
  if(parallel_data->ptr)
    printf("0: parallel_data initially not null\n");
  parallel_data->value = ompt_get_unique_id();
  int iam = omp_get_thread_num();
  int invoker = flag & 0xF;
  const char *event = (flag & ompt_parallel_team) ? "parallel" : "teams";
  const char *size = (flag & ompt_parallel_team) ? "team_size" : "num_teams";
//#pragma omp master
  if(omp_get_active_level ()==1) {
     int iam = omp_get_thread_num();
     int numt = omp_get_num_threads();
     int cur_num_threads=1;
     {
        uint64_t par_region_id = ompt_get_thread_data()->value;
        if (par_region_info[iam].find(par_region_id) == par_region_info[iam].end ()) {
            par_region_info[iam][par_region_id]={1,1,1000.0,-omp_get_wtime(),{2,4,8,16,24,32,40,48}};
        } else {
            cur_num_threads=par_region_info[iam][par_region_id].best_num_threads;
            if (!par_region_info[iam][par_region_id].parconf.empty ()) {
               int my_num_threads=par_region_info[iam][par_region_id].parconf.front();
               par_region_info[iam][par_region_id].cur_time=-omp_get_wtime();
	       if (my_num_threads*numt<=96) {
                  par_region_info[iam][par_region_id].cur_num_threads=my_num_threads;
                  cur_num_threads=my_num_threads;
                  par_region_info[iam][par_region_id].parconf.pop_front();
               } else {
                  par_region_info[iam][par_region_id].parconf.clear();
               }
            }
        }
     }
     omp_set_num_threads(cur_num_threads);
  }
  /*printf("%" PRIu64 ":" _TOOL_PREFIX
         " ompt_event_%s_begin: parent_task_id=%" PRIu64
         ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, "
         "parallel_id=%" PRIu64 ", requested_%s=%" PRIu32
         ", codeptr_ra=%p, invoker=%d\n",
         ompt_get_thread_data()->value, event, encountering_task_data->value,
         encountering_task_frame->exit_frame.ptr,
         encountering_task_frame->enter_frame.ptr, parallel_data->value, size,
         requested_team_size, codeptr_ra, invoker);*/
}

void on_ompt_callback_parallel_end(ompt_data_t *parallel_data,
                                          ompt_data_t *encountering_task_data,
                                          int flag, const void *codeptr_ra) {
  int invoker = flag & 0xF;
  const char *event = (flag & ompt_parallel_team) ? "parallel" : "teams";
//#pragma omp master
  if(omp_get_active_level ()==1) {
      int iam = omp_get_thread_num();
      uint64_t par_region_id = ompt_get_thread_data()->value;
      if (par_region_info[iam].find(par_region_id) != par_region_info[iam].end ()) {
         if (!par_region_info[iam][par_region_id].parconf.empty ()) {
            double cur_time=par_region_info[iam][par_region_id].cur_time+omp_get_wtime();
            if (cur_time<par_region_info[iam][par_region_id].best_time) {
               par_region_info[iam][par_region_id].best_time=cur_time;
               par_region_info[iam][par_region_id].best_num_threads=par_region_info[iam][par_region_id].cur_num_threads;
               printf("\nDONE OMP_BEST_NUM_THREADS=%d Time In Seconds %f\n",par_region_info[iam][par_region_id].best_num_threads, cur_time);
            } else {
               //printf("\nDONE OMP_CUR_NUM_THREADS=%d Time In Seconds %f\n", par_region_info[iam][par_region_id].cur_num_threads, cur_time);
            }
         }
      } else {
        // printf("\nDONE OMP_BEST_NUM_THREADS=%d\n",par_region_info[iam][par_region_id].best_num_threads);
      }
  }
  /*printf("%" PRIu64 ":" _TOOL_PREFIX " ompt_event_%s_end: parallel_id=%" PRIu64
         ", task_id=%" PRIu64 ", invoker=%d, codeptr_ra=%p\n",
         ompt_get_thread_data()->value, event, parallel_data->value,
         encountering_task_data->value, invoker, codeptr_ra);*/
}
}