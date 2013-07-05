/* Parses the events exported by the kernel */
/* Under /sys/bus/event_source/devices      */
/* In theory the format is described in     */
/* Documentation/ABI/testing/sysfs-bus-event_source-devices-events */
/* but really the format is whatever the perf tool accepts */
/* which is described in a complex lex/yacc file and changes */
/* from time to time.                       */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "test_utils.h"

static char test_string[]="Testing format of event files under /sys/...";
static int quiet;

#define SYSFS "/sys/bus/event_source/devices/"

struct generic_event_type {
	char *name;
	char *value;
	long long config;
	long long config1;
};

struct format_type {
	char *name;
	char *value;
	int field;
	int bits;
	unsigned long long  mask;
	int shift;
};

struct pmu_type {
	char *name;
	int type;
	int num_formats;
	int num_generic_events;
	struct format_type *formats;
	struct generic_event_type *generic_events;
};

static int num_pmus=0;

static struct pmu_type *pmus=NULL;


#define FIELD_UNKNOWN	0
#define FIELD_CONFIG	1
#define FIELD_CONFIG1	2

#define MAX_FIELDS	3

char fieldnames[MAX_FIELDS][20]={
	"unknown",
	"config",
	"config1",
};

static int parse_format(char *string, int *field_type, int *shift, int *bits) {

	int i,firstnum,secondnum;
	char format_string[BUFSIZ];

	/* get format */
	i=0;
	while(1) {
		format_string[i]=string[i];
		if (string[i]==':') {
			format_string[i]=0;
			break;
		}
		if (string[i]==0) break;
		i++;
	}

	if (!strcmp(format_string,"config")) {
		*field_type=FIELD_CONFIG;
	} else if (!strcmp(format_string,"config1")) {
		*field_type=FIELD_CONFIG1;
	} else {
		*field_type=FIELD_UNKNOWN;
		if (!quiet) fprintf(stderr,"Unknown register in format\n");
		test_fail(test_string);
	}

	/* Read first number */
	i++;
	firstnum=0;
	while(1) {
		if (string[i]==0) break;
		if (string[i]=='-') break;
		if ((string[i]<'0') || (string[i]>'9')) {
			fprintf(stderr,"Unknown format char %c\n",string[i]);
			test_fail(test_string);
		}
		firstnum*=10;
		firstnum+=(string[i])-'0';
		i++;
	}
	*shift=firstnum;

	/* check if no second num */
	if (string[i]==0) {
		*bits=1;
		return 0;
	}

	/* Read second number */
	i++;
	secondnum=0;
	while(1) {
		if (string[i]==0) break;
		if (string[i]=='-') break;
		if ((string[i]<'0') || (string[i]>'9')) {
			fprintf(stderr,"Unknown format char %c\n",string[i]);
			test_fail(test_string);
		}
		secondnum*=10;
		secondnum+=(string[i])-'0';
		i++;
	}

	*bits=(secondnum-firstnum)+1;

	return 0;
}

char *field_to_name(int field) {

	if (field>=MAX_FIELDS) return fieldnames[0];

	return fieldnames[field];
}

static int update_configs(int pmu, char *field,
			long long value, long long *c, long long *c1) {

	int i;

	for(i=0;i<pmus[pmu].num_formats;i++) {
		if (!strcmp(field,pmus[pmu].formats[i].name)) {
			if (pmus[pmu].formats[i].field==FIELD_CONFIG) {
				*c|=( (value&pmus[pmu].formats[i].mask)
					<<pmus[pmu].formats[i].shift);
				return 0;
			}

			if (pmus[pmu].formats[i].field==FIELD_CONFIG1) {
				*c1|=( (value&pmus[pmu].formats[i].mask)
					<<pmus[pmu].formats[i].shift);
				return 0;
			}
		}
	}

	return 0;
}

static int parse_generic(int pmu,char *value, long long *config, long long *config1) {

	long long c=0,c1=0,temp;
	char field[BUFSIZ];
	int i,ptr=0;
	int base=10;

	while(1) {
		i=0;
		while(1) {
			field[i]=value[ptr];
			if (value[ptr]==0) break;
			if ((value[ptr]=='=') || (value[ptr]==',')) {
				field[i]=0;
				break;
			}
			i++;
			ptr++;
		}

		/* if at end, was parameter w/o value */
		/* So it is a flag with a value of 1  */
		if ((value[ptr]==',') || (value[ptr]==0)) {
			temp=0x1;
		}
		else {
			/* get number */

			base=10;

			ptr++;

			if (value[ptr]=='0') {
				if (value[ptr+1]=='x') {
					ptr++;
					ptr++;
					base=16;
				}
			}

			if (base==10) {
				fprintf(stderr,"Base 10 number used:\n");
				fprintf(stderr,"  %s\n",value);
				fprintf(stderr,"This violates the ABI\n");
				test_fail(test_string);
			}

			temp=0x0;
			while(1) {

				if (value[ptr]==0) break;
				if (value[ptr]==',') break;
				if (! ( ((value[ptr]>='0') && (value[ptr]<='9'))
                   			|| ((value[ptr]>='a') && (value[ptr]<='f'))) ) {
					fprintf(stderr,"Unexpected char %c\n",value[ptr]);
					test_fail(test_string);
				}
				temp*=base;
				if ((value[ptr]>='0') && (value[ptr]<='9')) {
					temp+=value[ptr]-'0';
				}
				else {
					temp+=(value[ptr]-'a')+10;
				}
				i++;
				ptr++;
			}
		}
		update_configs(pmu,field,temp,&c,&c1);
		if (value[ptr]==0) break;
		ptr++;
	}
	*config=c;
	*config1=c1;
	return 0;
}


static int init_pmus(void) {

	DIR *dir,*event_dir,*format_dir;
	struct dirent *entry,*event_entry,*format_entry;
	char dir_name[BUFSIZ],event_name[BUFSIZ],event_value[BUFSIZ],
		temp_name[BUFSIZ],format_name[BUFSIZ],format_value[BUFSIZ];
	int type,pmu_num=0,format_num=0,generic_num=0;
	FILE *fff;
	int result;


	/* Count number of PMUs */
	/* This may break if PMUs are ever added/removed on the fly? */

	dir=opendir(SYSFS);
	if (dir==NULL) {
		fprintf(stderr,"Unable to opendir "
			SYSFS " : %s\n",
			strerror(errno));
		test_fail(test_string);
	}

	while(1) {
		entry=readdir(dir);
		if (entry==NULL) break;
		if (!strcmp(".",entry->d_name)) continue;
		if (!strcmp("..",entry->d_name)) continue;
		num_pmus++;
	}

	if (num_pmus<1) {
		test_fail(test_string);
	}

	pmus=calloc(num_pmus,sizeof(struct pmu_type));
	if (pmus==NULL) {
		test_fail(test_string);
	}

	/****************/
	/* Add each PMU */
	/****************/

	rewinddir(dir);

	while(1) {
		entry=readdir(dir);
		if (entry==NULL) break;
		if (!strcmp(".",entry->d_name)) continue;
		if (!strcmp("..",entry->d_name)) continue;

		/* read name */
		pmus[pmu_num].name=strdup(entry->d_name);
		sprintf(dir_name,SYSFS"/%s",
			entry->d_name);

		/* read type */
		sprintf(temp_name,"%s/type",dir_name);
		fff=fopen(temp_name,"r");
		if (fff==NULL) {
		}
		else {
			result=fscanf(fff,"%d",&type);
			pmus[pmu_num].type=type;
			fclose(fff);
		}

		/***********************/
		/* Scan format strings */
		/***********************/
		sprintf(format_name,"%s/format",dir_name);
		format_dir=opendir(format_name);
		if (format_dir==NULL) {
			/* Can be normal to have no format strings */
		}
		else {
			/* Count format strings */
			while(1) {
				format_entry=readdir(format_dir);
				if (format_entry==NULL) break;
				if (!strcmp(".",format_entry->d_name)) continue;
				if (!strcmp("..",format_entry->d_name)) continue;
				pmus[pmu_num].num_formats++;
			}

			/* Allocate format structure */
			pmus[pmu_num].formats=calloc(pmus[pmu_num].num_formats,
							sizeof(struct format_type));
			if (pmus[pmu_num].formats==NULL) {
				pmus[pmu_num].num_formats=0;
				test_fail(test_string);
			}

			/* Read format string info */
			rewinddir(format_dir);
			format_num=0;
			while(1) {
				format_entry=readdir(format_dir);

				if (format_entry==NULL) break;
				if (!strcmp(".",format_entry->d_name)) continue;
				if (!strcmp("..",format_entry->d_name)) continue;

				pmus[pmu_num].formats[format_num].name=
					strdup(format_entry->d_name);
				sprintf(temp_name,"%s/format/%s",
					dir_name,format_entry->d_name);
				fff=fopen(temp_name,"r");
				if (fff!=NULL) {
					result=fscanf(fff,"%s",format_value);
					pmus[pmu_num].formats[format_num].value=
						strdup(format_value);
					fclose(fff);

					parse_format(format_value,
						&pmus[pmu_num].formats[format_num].field,
						&pmus[pmu_num].formats[format_num].shift,
						&pmus[pmu_num].formats[format_num].bits);
					if (pmus[pmu_num].formats[format_num].bits==64) {
						pmus[pmu_num].formats[format_num].mask=0xffffffffffffffffULL;
					} else {
						pmus[pmu_num].formats[format_num].mask=
							(1ULL<<pmus[pmu_num].formats[format_num].bits)-1;
					}
					format_num++;
				}
			}
			closedir(format_dir);
		}

		/***********************/
		/* Scan generic events */
		/***********************/
		sprintf(event_name,"%s/events",dir_name);
		event_dir=opendir(event_name);
		if (event_dir==NULL) {
			/* It's sometimes normal to have no generic events */
		}
		else {

			/* Count generic events */
			while(1) {
				event_entry=readdir(event_dir);
				if (event_entry==NULL) break;
				if (!strcmp(".",event_entry->d_name)) continue;
				if (!strcmp("..",event_entry->d_name)) continue;
				pmus[pmu_num].num_generic_events++;
			}

			/* Allocate generic events */
			pmus[pmu_num].generic_events=calloc(
				pmus[pmu_num].num_generic_events,
				sizeof(struct generic_event_type));
			if (pmus[pmu_num].generic_events==NULL) {
				pmus[pmu_num].num_generic_events=0;
				test_fail(test_string);
			}

			/* Read in generic events */
			rewinddir(event_dir);
			generic_num=0;
			while(1) {
				event_entry=readdir(event_dir);
				if (event_entry==NULL) break;
				if (!strcmp(".",event_entry->d_name)) continue;
				if (!strcmp("..",event_entry->d_name)) continue;

				pmus[pmu_num].generic_events[generic_num].name=
					strdup(event_entry->d_name);
				sprintf(temp_name,"%s/events/%s",
					dir_name,event_entry->d_name);
				fff=fopen(temp_name,"r");
				if (fff!=NULL) {
					result=fscanf(fff,"%s",event_value);
					pmus[pmu_num].generic_events[generic_num].value=
						strdup(event_value);
					fclose(fff);
				}
				parse_generic(pmu_num,event_value,
						&pmus[pmu_num].generic_events[generic_num].config,
						&pmus[pmu_num].generic_events[generic_num].config1);
				generic_num++;
			}
			closedir(event_dir);
		}
		pmu_num++;
	}

	closedir(dir);

	(void)result;

	return 0;

}

#if 0
void dump_pmus(void) {

	int i,j;

	printf("\nDumping PMUs\n\n");

	for(i=0;i<num_pmus;i++) {

		if (pmus[i].name!=NULL) printf("%s\n",pmus[i].name);
		printf("\tType=%d\n",pmus[i].type);

		if (pmus[i].num_formats) {
			printf("\tFormats:\n");
			for(j=0;j<pmus[i].num_formats;j++) {
				printf("\t\t%s : ",pmus[i].formats[j].name);
				printf("%s\n",pmus[i].formats[j].value);
				printf("\t\t\tfield: %s shift: %d bits: %d mask: %llx\n",
					field_to_name(pmus[i].formats[j].field),
					pmus[i].formats[j].shift,
					pmus[i].formats[j].bits,
					pmus[i].formats[j].mask);
			}
		}
		if (pmus[i].num_generic_events) {
			printf("\tGeneric Events:\n");
			for(j=0;j<pmus[i].num_generic_events;j++) {
				printf("\t\t%s : ",
					pmus[i].generic_events[j].name);
				printf("%s\n",pmus[i].generic_events[j].value);
				printf("\t\t\tconfig: %llx  config1: %llx\n",
					pmus[i].generic_events[j].config,
					pmus[i].generic_events[j].config1);
			}
		}
	}

}
#endif


int main(int argc, char **argv) {

	quiet=test_quiet();

	if (!quiet) {
		printf("\n");
		printf("Testing files under /sys/bus/event_source/devices\n");
		printf("To see if they match ABI documented in\n");
		printf("  Documentation/ABI/testing/sysfs-bus-event_source-devices-events\n");
		printf("\n");

	}

	init_pmus();

//	dump_pmus();

	test_pass(test_string);

	return 0;

}
