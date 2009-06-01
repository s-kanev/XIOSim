/* memdep-lwt.cpp: Load Wait Table [Alpha 21264, R.E. Kessler, IEEE Micro 1999] */
/*
 * __COPYRIGHT__ GT
 */
#define COMPONENT_NAME "lwt"

#ifdef MEMDEP_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  int num_entries;
  int reset_interval;
           
  if(sscanf(opt_string,"%*[^:]:%[^:]:%d:%d",name,&num_entries,&reset_interval) != 3)
    fatal("bad memdep options string %s (should be \"lwt:name:num_entries:reset_interval\")",opt_string);
  return new memdep_lwt_t(name,num_entries,reset_interval);
}
#else

class memdep_lwt_t:public memdep_t
{
  protected:
  int num_entries;
  char * array;
  int reset_interval;
  tick_t last_reset;
  int mask;

  public:
  memdep_lwt_t(char * arg_name,
               int arg_num_entries,
               int arg_reset_interval
              )
  {
    init();

    last_reset = 0;

    /* verify arguments are valid */
    CHECK_PPOW2(arg_num_entries);
    CHECK_POS(arg_reset_interval);

    name = strdup(arg_name);
    if(!name)
      fatal("couldn't malloc lwt-memdep name (strdup)");
    type = strdup("lwt-memdep");
    if(!type)
      fatal("couldn't malloc lwt-memdep type (strdup)");

    num_entries = arg_num_entries;
    reset_interval = arg_reset_interval;

    mask = num_entries-1;
    array = (char*) calloc(num_entries,sizeof(array));
    if(!array)
      fatal("couldn't malloc lwt-memdep lwt table");

    bits = num_entries + (int)ceil(log(reset_interval)/log(2.0)); /* plus extra counter to track cycles */
  }

  /* DESTROY */
  ~memdep_lwt_t()
  {
    free(array); array = NULL;
  }

  /* LOOKUP */
  MEMDEP_LOOKUP_HEADER
  {
    int index = PC&mask;

    MEMDEP_STAT(lookups++;)

    if((sim_cycle - last_reset) >= reset_interval)
    {
      memset(array,0,num_entries*sizeof(*array));
      last_reset = sim_cycle - (sim_cycle % reset_interval);
    }

    if(array[index]) /* predict wait */
      return !sta_unknown;
    else
      return true;
  }

  /* UPDATE */
  MEMDEP_UPDATE_HEADER
  {
    MEMDEP_STAT(updates++;)

    if((sim_cycle - last_reset) >= reset_interval)
    {
      memset(array,0,num_entries*sizeof(*array));
      last_reset = sim_cycle - (sim_cycle % reset_interval);
    }

    int index = PC&mask;
    array[index] = true;
  }
};

#endif /* MEMDEP_PARSE_ARGS */
#undef COMPONENT_NAME
