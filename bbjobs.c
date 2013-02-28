#define _GNU_SOURCE

#include <getopt.h>
#include <stdio.h>

#include "bbjobs.h"
#include "lsb_strings.h"

#define USAGE "rinfo [-u username -r -a -s -d -p -P] [ JOBID1 ... ]"

void xdie(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}


int main(int argc, char **argv) {
	LS_LONG_INT jobid;
	int cc;
	int jopts = 0;
	int jobidx;
	char *juser = NULL;
	
	if(lsb_init(argv[0]) < 0) 
		xdie("lsb_init failed");
	
	while((cc = getopt(argc, argv, "rasdpu:P")) != EOF) {
		switch(cc) {
			case 'r':
				jopts |= RUN_JOB;
				break;
			case 'a':
				jopts |= ALL_JOB;
				break;
			case 's':
				jopts |= SUSP_JOB;
				break;
			case 'd':
				jopts |= DONE_JOB;
				break;
			case 'p':
				jopts |= PEND_JOB;
				break;
			case 'u':
				if(!optarg)
					xdie(USAGE);
				juser = optarg;
				break;
			case 'P':
				pr_set_output_parseable(1);
				break;
			default:
				xdie(USAGE);
		}
	}
	
	
	if(juser == NULL) /* use $LOGNAME if user did not specify any username */
		asprintf(&juser, "%s", (getenv("LOGNAME") ? getenv("LOGNAME") : "INVALID") );
	
	if(optind < argc) {
		while(optind < argc) {
			jobid = atoi(argv[optind]);
			jobidx = parse_jobidx(argv[optind]);

			if(jobid == 0 && memcmp(argv[optind], "0", 1) != 0) {
				fprintf(stderr, "WARNING: ignoring invalid argument: %s\n", argv[optind]);
			}
			else {
				/* we have a specific jobid -> ingore 'juser' */
				if( get_jobinfo(jobid, jobidx, ALL_JOB, "all") == 0 )
					printf("Job <%llu> is not found\n", jobid);
			}
			optind++;
			if(optind != argc) printf("\n");
		}
	}
	else { /* no jobid -> filter by 'juser' */
		if( get_jobinfo(0, -1, jopts, juser) == 0 )
			printf("No jobs found for %s\n", juser);
	}
	
	exit(0);
}

/*
** Display all jobs that match given filter
*/
int get_jobinfo(LS_LONG_INT jobid, int jobidx, int jobopts, char *jobuser) {
	struct jobInfoHead *jInfoH;
	struct jobInfoEnt *job;
	int i = 0;
	
	
	jInfoH = lsb_openjobinfo_a(jobid, NULL, jobuser, NULL, NULL, jobopts);

	if(jInfoH == NULL)
		goto EARLY_RETURN;

	for(i = 0; i < jInfoH->numJobs; i++) {
		job = lsb_readjobinfo(NULL);
		if(job == NULL)
			xdie("failed to get jobinfo");

		if(jobidx >= 0 && job->counter[JGRP_COUNT_NJOBS] > 0 && LSB_ARRAY_IDX(job->jobId) != jobidx) {
			continue;
		}
		
		print_single_job(job);
		
		if(jobidx < 0 && i+1 != jInfoH->numJobs) printf("\n");
	}
	
	EARLY_RETURN:
	lsb_closejobinfo();
	return i;
}



/*
** Print information about given jobInfoEnt to STDOUT
*/
void print_single_job(struct jobInfoEnt *job) {
	time_t wc_time;   /* Wallclock time in seconds */
	double cpu_time;  /* CPU time in seconds       */
	double sys_time;  /* System time in seconds    */
	time_t wait_time; /* Time idle in queue in sec */
	time_t rq_mem;       /* Requested RAM in MB       */
	double muti;      /* Memory Utilization        */
	double peff;      /* CPU Eff.                  */
	double sysp;      /* SYS time wasted           */
	
	
	/* Calculate some handy values
	** cpu_time and co will be nonsense if the job didn't start yet
	*/
	wait_time = (job->startTime ? job->startTime : time(NULL))-job->submitTime;
	wc_time   = job->jRusageUpdateTime - job->startTime;
	cpu_time  = (double) ( (double)job->runRusage.utime + (double)job->runRusage.stime );
	sys_time  = job->runRusage.stime;
	rq_mem    = get_rr_mem(job->submit.resReq);

	/* if in parseable mode: notify printer about current jobid */
	pr_set_prefix( LSB_ARRAY_JOBID(job->jobId) );
	
	/**************************************************************
	** GENERAL INFORMATION FOR ALL JOBS                          **
	**************************************************************/
	
	pr_fancy("Job information");
	
	/* Add IDX if jobid, normal jobid otherwise */
	if(job->counter[JGRP_COUNT_NJOBS] > 0) {
		pr_lhand("Job ID"); printf("%d[%d]\n", LSB_ARRAY_JOBID(job->jobId), LSB_ARRAY_IDX(job->jobId));
	}
	else {
		pr_int("Job ID", job->jobId);
	}
	
	/* Print additional status info if job is finished */
	pr_lhand("Status"); printf("%s", get_job_status_desc(job));
	if(IS_FINISH(job->status)) {
		printf(" (exit code: %d)", (job->exitStatus >> 8) );
	}
	printf("\n");
	
	if( HAS_STATS(job) ) {
		pr_lhand("Running on node"); print_exec_hosts(job);
	}
	
	pr_str("User", job->user);  /* FIXME: Optional */
	pr_str("Queue", job->submit.queue);
	
	if( strcmp(job->submit.jobName, job->submit.command) != 0 )
		pr_stn("Job name", job->submit.jobName, 64);
	
	pr_stn("Command", job->submit.command, 64);
	
	pr_lhand("Working directory");
	if(job->cwd[0] != '/')
		printf("$HOME/");
	pr_stn(NULL, job->cwd, 256);
	
	
	
	/**************************************************************
	** REQUESTED RESOURCES                                       **
	**************************************************************/
	pr_fancy("Requested resources");
	
	pr_int("Requested cores", job->submit.numProcessors);
	
	if(job->submit.rLimits[LSF_RLIMIT_RUN] != -1) /* not started jobs have -1 in most cases - thank you lsf! */
		pr_duration("Requested runtime", job->submit.rLimits[LSF_RLIMIT_RUN]);
	
	/* print memory per core and total (if core > 1) */
	pr_lhand("Requested memory");  printf("%d MB per core", rq_mem);
	if(job->submit.numProcessors > 1)
		printf(", %d MB total", rq_mem * job->submit.numProcessors);
	printf("\n");
	
	pr_stn("Dependency", job->submit.dependCond,64);
	
	
	
	/**************************************************************
	** HISTORY: SUBMIT, START AND END TIMES                      **
	**************************************************************/
	pr_fancy("Job history");
	if(job->submitTime > 0)
		pr_ts("Submitted at", job->submitTime);
	if(job->startTime > 0)
		pr_ts("Started at", job->startTime);
	if(job->endTime > 0)
		pr_ts("Finished at", job->endTime);
	if(wait_time > 0)
		pr_duration("Queue wait time", wait_time);
	
	
	/**************************************************************
	** CURRENT (OR PAST) RESOURCE USAGE OF THIS JOB              **
	**************************************************************/
	if( HAS_STATS(job) ) {
		/* This information is only available it the job ever entered into RUN state */
		peff = (wc_time > 0 ? (cpu_time / (double)(job->submit.numProcessors * wc_time ))*100               : 0);
		sysp = (wc_time > 0 ? (sys_time / (double)(job->submit.numProcessors * wc_time ))*100               : 0);
		muti = (rq_mem  > 0 ? (job->runRusage.swap/1024 / (double)(job->submit.numProcessors * rq_mem))*100 : 0);
		pr_fancy("Resource usage");
		pr_ts("Updated at", job->jRusageUpdateTime);
		pr_duration("Wall-clock", wc_time);
		pr_int("Tasks", job->runRusage.nthreads);
		pr_duration("Total CPU time", cpu_time);
		pr_prct("CPU utilization", peff);
		pr_prct("Sys/Kernel time", sysp);
		pr_lhand("Total Memory"); printf("%d MB\n", job->runRusage.swap/1024);
		pr_prct("Memory utilization", muti);
	}

}


/*
** Printout executing hosts in bjobs-style output
*/
void print_exec_hosts(struct jobInfoEnt *job) {
	char hbuff[MAXHOSTNAMELEN];   /* hostname scratch buffer */
	int idx, last_idx;
	int ncores = 0;
	
	last_idx = job->numExHosts-1;
	strncpy(hbuff, job->exHosts[0], MAXHOSTNAMELEN);
	
	/* we loop beyond the end of the array in order to catch the last element */
	for(idx=0; idx <= last_idx+1 ; idx++) {
		if( idx > last_idx || strncmp(hbuff, job->exHosts[idx], MAXHOSTNAMELEN) != 0 ) {
			
			if(last_idx > 1) /* multicore job -> display ncores for each node */
				printf("%d*", ncores);
			printf("%s ", hbuff);
			
			if( !(idx > last_idx) ) { /* get next hbuff if we are not already at the end */
				strncpy(hbuff, job->exHosts[idx], MAXHOSTNAMELEN);
				ncores=1;
			}
		}
		else {
			ncores++;
		}
	}
	printf("\n");
}


/* Extracts the jobindex part out of a string such as 12345[333] */
int parse_jobidx(char *str) {
	char *start;
	int rv = -1;

	start = index(str, '[');
	if(start != NULL && strlen(start) > 1)
		rv = atoi(start+1);

	return rv;
}

