/* prefetch-2lev-IP.cpp: IP-based (PC) prefetcher [http://download.intel.com/technology/architecture/sma.pdf] */
/*
 * __COPYRIGHT__ GT
 */

#define COMPONENT_NAME "2lev-IP"
/*
  This has been modified from the original IP prefetcher to better handle the case
  when we have an access pattern similar to:

  Strides:
  X, 0, 0, 0, 0, X, 0, 0, 0 ...

  With our changes, the prefetcher does not update on the strides of 0, since
  strides of 0 are not interesting.

  Instead, it correctly prefetches a stride of X whenever it is seen, assuming
  X does not change.

  Also, we do not prefetch if we're going to cross a page boundary.

  It implements the state machine detailed in:
  Jean-Loup Baer and Tien-Fu Chen. 1995. Effective Hardware-Based Data Prefetching for High-Performance
  Processors. IEEE Trans. Comput. 44, 5 (May 1995), 609-623.

  It is currently not really a two level prefetcher, but may be someday.
 */

#ifdef PREFETCH_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  int num_entries;
  int last_addr_bits;
  int last_stride_bits;
  if(sscanf(opt_string,"%*[^:]:%d:%d:%d",&num_entries,&last_addr_bits,&last_stride_bits) != 3)
    fatal("bad %s prefetcher options string %s (should be \"2lev-IP:num_entries:addr-bits:stride-bits\")",cp->name,opt_string);
  return std::make_unique<prefetch_2lev_IP_t>(cp, num_entries, last_addr_bits, last_stride_bits);
}
#else

class prefetch_2lev_IP_t:public prefetch_t
{
  protected:
  int num_entries;
  int mask;

  int last_addr_bits;
  int last_stride_bits;

  int last_addr_mask;
  int last_stride_mask;

  struct prefetch_IP_table_t {
    //    md_addr_t PC;
    md_paddr_t last_paddr;
    int last_stride;
    my2bc_t conf;
  } * table;

  public:
  /* CREATE */
  prefetch_2lev_IP_t(struct cache_t * arg_cp,
                int arg_num_entries,
                int arg_last_addr_bits,
                int arg_last_stride_bits)
  {
    init();

    char name[256]; /* just used for error checking macros */

    type = strdup("2lev-IP");
    if(!type)
      fatal("couldn't calloc IP-prefetch type (strdup)");

    cp = arg_cp;
    sprintf(name,"%s.%s",cp->name,type);

    CHECK_PPOW2(arg_num_entries);
    num_entries = arg_num_entries;
    mask = num_entries - 1;

    last_addr_bits = arg_last_addr_bits;
    last_stride_bits = arg_last_stride_bits;

    last_addr_mask = (1<<last_addr_bits)-1;
    last_stride_mask = (1<<last_stride_bits)-1;

    table = (struct prefetch_2lev_IP_t::prefetch_IP_table_t*) calloc(num_entries,sizeof(*table));
    if(!table)
      fatal("couldn't calloc IP-prefetch table");

    bits = num_entries * (last_addr_bits + last_stride_bits + 2);
    assert(arg_cp);
  }

  /* DESTROY */
  ~prefetch_2lev_IP_t()
  {
    free(table);
    table = NULL;
  }

  /* LOOKUP */
  PREFETCH_LOOKUP_HEADER
{
    lookups++;

    int index = PC & mask;
    md_paddr_t pf_paddr = 0;

    // If want to use tags for table
    /*    if(table[index].PC != PC) {
      table[index].last_paddr = paddr&last_addr_mask;
      table[index].last_stride = 0;
      table[index].conf = MY2BC_WEAKLY_TAKEN;
      table[index].PC = PC;
      return 0;
      }*/

    /* train entry first */
    md_paddr_t last_paddr = (paddr&~last_addr_mask) | table[index].last_paddr;
    int this_stride = (paddr - last_paddr) & last_stride_mask;

    /* zero stride is just unuseful prefetches, don't update tables */
    if(this_stride == 0) {
      return 0;
    }

    table[index].last_paddr = paddr&last_addr_mask;
    if(table[index].conf != MY2BC_STRONG_TAKEN) {
      table[index].last_stride = this_stride;
    }

    bool is_correct = (this_stride == table[index].last_stride);

    if(table[index].conf == MY2BC_WEAKLY_NT && is_correct) {
      table[index].conf = MY2BC_STRONG_TAKEN;
    }
    else {
      MY2BC_UPDATE(table[index].conf, is_correct);
    }

    if((table[index].conf != MY2BC_STRONG_NT)) {
      pf_paddr = paddr + table[index].last_stride;

      // If going to cross page boundary, don't prefetch
      if(memory::page_round_down(pf_paddr) != memory::page_round_down(paddr)) {
        return 0;
      }
    }
    return pf_paddr;
  }

  virtual md_paddr_t latest_lookup (const md_addr_t PC, const md_paddr_t paddr)
  {
    (void) paddr;

    int index = PC & mask;
    return table[index].last_stride + paddr;
  }
};


#endif /* PREFETCH_PARSE_ARGS */
#undef COMPONENT_NAME
