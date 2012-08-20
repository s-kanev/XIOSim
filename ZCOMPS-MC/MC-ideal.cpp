/* MC-simple.cpp: Simple memory controller */
/*
 * __COPYRIGHT__ SK
 */

#ifdef MC_PARSE_ARGS
if(!strcasecmp("ideal",type))
{
  if(sscanf(opt_string,"%*[^:]") != 0)
    fatal("bad memory controller options string %s (should be \"simple\")",opt_string);
  return new MC_ideal_t();
}
#else

/* This implements an ideal memory controller. No latency, no queues, no fun. */
class MC_ideal_t:public MC_t
{
  protected:

  public:

  MC_ideal_t()
  {
    init();
  }

  ~MC_ideal_t()
  {
  }

  MC_ENQUEUABLE_HEADER
  {
    return true;
  }

  /* Enqueue a memory command (read/write) to the memory controller. */
  MC_ENQUEUE_HEADER
  {
    /* fill previous level as appropriate, straight away
       XXX: hope that LLC won't trip with 0 latency */
    if(prev_cp)
    {
      fill_arrived(prev_cp,MSHR_bank,MSHR_index);
      bus_use(uncore->fsb,linesize>>uncore->fsb_DDR,cmd==CACHE_PREFETCH);
    }

    total_accesses++;
  }

  /* This is called each cycle to process the requests in the memory controller queue. */
  MC_STEP_HEADER
  {
  }

  MC_PRINT_HEADER
  {
  }

};


#endif /* MC_PARSE_ARGS */
