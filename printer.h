#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void pr_set_output_parseable(int x);                    /* no 'humand readable' output */
void pr_set_prefix(long long unsigned jid);                  /* pr_lhand will prefix output with this if the output was set to 'parseable' */
void pr_ts(char *lhand, time_t ts);                     /* printout timestamp     */
void pr_int(char *lhand, int value);                    /* printout an integer    */
void pr_str(char *lhand, char *str);                    /* printout string        */
void pr_stn(char *lhand, char *str, int limit);         /* printout capped string */
void pr_prct(char *lhand, double num);                  /* printout %             */
void pr_lhand(char *lhand);                             /* only print lefthand    */
void pr_fancy(char *desc);                              /* print colored string   */
