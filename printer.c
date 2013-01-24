#include "printer.h"

static int           _pr_pmode = 0;
static long long unsigned _pr_pfx = 0;
/*
** Convert seconds into 'human readable' format
*/
void pr_duration(char *lhand, time_t sec) {
	pr_lhand(lhand);
	
	if(sec <= 300 || _pr_pmode) {
		printf("%d sec", (int)sec);
	}
	else if(sec <= 60*60) {
		printf("%d min", (int)sec/60);
	}
	else {
		printf("%d h %d min", (int)sec/60/60, (int)sec/60%60);
	}
	printf("\n");
}


/*
** Printout a timestamp in localtime
*/
void pr_ts(char *lhand, time_t ts) {
	char dst[100];
	struct tm *tmp;
	
	tmp = localtime(&ts);
	
	strftime(dst, sizeof(dst), "%R %F", tmp);
	pr_str(lhand, dst);
}


/*
** Print a normal integer
*/
void pr_int(char *lhand, int value) {
	pr_lhand(lhand);
	printf("%d\n", value);
}

/*
** Print a simple string with upper cap
*/
void pr_stn(char *lhand, char *str, int limit) {
#define DOTTY "[...]"
	char dst[100];
	
	if(limit > sizeof(dst))
		limit = sizeof(dst);
	
	snprintf(dst, limit, "%s", str);
	
	if(limit > strlen(DOTTY) && strlen(str) > limit) {
		/* add dots if we truncated output (and have enough space for them) */
		memcpy(&dst[limit-1-strlen(DOTTY)], DOTTY, strlen(DOTTY));
	}
	
	pr_str(lhand, (_pr_pmode ? str : dst ) );
}



/*
** Print a simple string
*/
void pr_str(char *lhand, char *str) {
	pr_lhand(lhand);
	printf("%s\n", ( strlen(str) == 0 ? "-" : str ) );
}



/*
** Print a percent value
*/
void pr_prct(char *lhand, double num) {
	pr_lhand(lhand);
	printf("%.1f %%\n", num);
}



/*
** Printout 'description' of this line
*/
void pr_lhand(char *lhand) {
	#define C_WIDTH 20
	char xbuff[C_WIDTH];
	
	
	if(lhand != NULL) {
		/* this function won't do anything if lhand is NULL
		** This enables us to use use the pr_* functions without
		** a (forced) lefthand-side output */
		memset(xbuff, ' ', C_WIDTH);
		xbuff[C_WIDTH-1] = '\0';
		memcpy(xbuff, lhand, strlen(lhand));
		
		if(_pr_pmode) printf("%llu ", _pr_pfx);
		
		printf("  %s : ", xbuff);
	}
}



/*
** Printout fancy color info
*/
void pr_fancy(char *lhand) {
	if(isatty(fileno(stdout))) {
		printf("\x1b[1;32m%s\x1b[0m\n",lhand);
	}
	else {
		printf("%s\n", lhand);
	}
}

void pr_set_output_parseable(int x) {
	_pr_pmode = x;
}
void pr_set_prefix(long long unsigned jid) {
	_pr_pfx = jid;
}
