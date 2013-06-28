/* reset_leader.c  */
/* by Vince Weaver   vincent.weaver _at_ maine.edu */

/* Test if calling reset on a leader resets all child events */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>


#include "perf_event.h"
#include "test_utils.h"
#include "perf_helpers.h"
#include "instructions_testcode.h"


#define EVENTS 2

#define READ_SIZE (EVENTS + 1)

int main(int argc, char** argv) {

   int fd[EVENTS],ret,quiet;
   int result;
   int read_result;
   long long count[READ_SIZE];
   int i;

   struct perf_event_attr pe[EVENTS];

   char test_string[]="Testing reset on group leader...";

   quiet=test_quiet();

   if (!quiet) {
      printf("Testing if reset on group leader resets children.\n");
   }

   /* setup instruction event, group leader */

   memset(&pe[0],0,sizeof(struct perf_event_attr));

   pe[0].type=PERF_TYPE_HARDWARE;
   pe[0].size=sizeof(struct perf_event_attr);
   pe[0].config=PERF_COUNT_HW_INSTRUCTIONS;
   pe[0].disabled=1;
   pe[0].exclude_kernel=1;
   pe[0].read_format=PERF_FORMAT_GROUP;

   fd[0]=perf_event_open(&pe[0],0,-1,-1,0);
   if (fd[0]<0) {
      fprintf(stderr,"Error opening\n");
      test_fail(test_string);
      exit(1);
   }

   /* setup cycles event, group child */

   memset(&pe[1],0,sizeof(struct perf_event_attr));

   pe[1].type=PERF_TYPE_HARDWARE;
   pe[1].size=sizeof(struct perf_event_attr);
   pe[1].config=PERF_COUNT_HW_CPU_CYCLES;
   pe[1].disabled=0;
   pe[1].exclude_kernel=1;

   fd[1]=perf_event_open(&pe[1],0,-1,fd[0],0);
   if (fd[1]<0) {
      fprintf(stderr,"Error opening\n");
      test_fail(test_string);
      exit(1);
   }

	/* Count a million */

   ioctl(fd[0], PERF_EVENT_IOC_RESET, 0);
   ioctl(fd[0], PERF_EVENT_IOC_ENABLE,0);

   result=instructions_million();
   if (result==CODE_UNIMPLEMENTED) printf("Warning, no million\n");

   ioctl(fd[0], PERF_EVENT_IOC_DISABLE,0);

	/* read results */

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (!quiet) {
      printf("Initial read\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

	/* second read */

   if (!quiet) {
      printf("Second read, to be sure\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

	/* reset leader */

   ioctl(fd[0], PERF_EVENT_IOC_RESET, 0);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (count[1]!=0) {
      fprintf(stderr,"Reset of event 0 did not work\n");
      test_fail(test_string);
   }

   if (count[2]==0) {
      fprintf(stderr,"Reset of leader cleared child\n");
      test_fail(test_string);
   }

   if (!quiet) {
      printf("After reset of group leader\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

	/* reset child */

   ioctl(fd[1], PERF_EVENT_IOC_RESET, 0);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (count[1]!=0) {
      fprintf(stderr,"Reset of event 0 did not work\n");
      test_fail(test_string);
   }

   if (count[2]!=0) {
      fprintf(stderr,"Reset of child did not work\n");
      test_fail(test_string);
   }

   if (!quiet) {
      printf("After reset of group child\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

   /* Count some more */

   ioctl(fd[0], PERF_EVENT_IOC_RESET, 0);
   ioctl(fd[0], PERF_EVENT_IOC_ENABLE,0);

   result=instructions_million();
   if (result==CODE_UNIMPLEMENTED) printf("Warning, no million\n");

   ioctl(fd[0], PERF_EVENT_IOC_DISABLE,0);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (!quiet) {
      printf("Count some more\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

	/* reset leader, PERF_IOC_FLAG_GROUP */

   ioctl(fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (count[1]!=0) {
      fprintf(stderr,"Reset of event 0 did not work\n");
      test_fail(test_string);
   }

   if (count[2]!=0) {
      fprintf(stderr,"Reset of leader didn't clear child\n");
      test_fail(test_string);
   }

   if (!quiet) {
      printf("After reset of group leader with PERF_IOC_FLAG_GROUP\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

   /* Count some more */

   ioctl(fd[0], PERF_EVENT_IOC_RESET, 0);
   ioctl(fd[0], PERF_EVENT_IOC_ENABLE,0);

   result=instructions_million();
   if (result==CODE_UNIMPLEMENTED) printf("Warning, no million\n");

   ioctl(fd[0], PERF_EVENT_IOC_DISABLE,0);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (!quiet) {
      printf("Count some more\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }


	/* reset child with PERF_IOC_FLAG_GROUP */

   ioctl(fd[1], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);

   read_result=read(fd[0],&count,sizeof(long long)*READ_SIZE);
   if (read_result!=sizeof(long long)*READ_SIZE) {
      printf("Unexpected read size\n");
   }

   if (count[1]!=0) {
      fprintf(stderr,"Reset of event 0 did not work\n");
      test_fail(test_string);
   }

   if (count[2]!=0) {
      fprintf(stderr,"Reset of child did not work\n");
      test_fail(test_string);
   }

   if (!quiet) {
      printf("After reset of group child with PERF_IOC_FLAG_GROUP\n");
      for(i=0;i<count[0];i++) {
	 printf("\t%i Counted %lld\n",i,count[1+i]);
      }
   }



   for(i=0;i<EVENTS;i++) {
      close(fd[i]);
   }

   (void) ret;

   test_pass(test_string);

   return 0;
}
