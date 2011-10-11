/* ras-multistack.cpp: 	Implementation using multiple speculative and real Return Address Stacks - very loosely folowing US patent 6374350 by Intel */
/*
 * Copyright, Svilen Kanev 2011
 */

#define COMPONENT_NAME "multistack"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  int spec_size, real_size;
  if(sscanf(opt_string,"%*[^:]:%[^:]:%d:%d",name,&spec_size,&real_size) != 3)
    fatal("bad ras options string %s (should be \"multistack:name:spec_size:real_size\")",opt_string);
  return new RAS_multistack_t(name,spec_size,real_size);
}
#else

class RAS_multistack_t:public RAS_t
{
  class RAS_stack_chkpt_t:public RAS_chkpt_t
  {
    public:
    int pos;
  };

  struct stack_entry_t{
    md_addr_t addr;
    bool valid;
  };

  protected:

  stack_entry_t *spec_stack;
  md_addr_t *real_stack;
  int spec_size;
  int real_size;
  int spec_head;
  int real_head;
  int spec_to_real_head;

  public:

  /* CREATE */
  RAS_multistack_t(char * const arg_name,
              const int arg_num_spec_entries,
              const int arg_num_real_entries
             )
  {
    init();

    spec_size = arg_num_spec_entries;
    spec_stack = (stack_entry_t*) calloc(spec_size,sizeof(stack_entry_t));
    if(!spec_stack)
      fatal("couldn't malloc stack spec_stack");

    real_size = arg_num_real_entries;
    real_stack = (md_addr_t*) calloc(spec_size,sizeof(md_addr_t));
    if(!real_stack)
      fatal("couldn't malloc stack real_stack");

    spec_head = 0;
    real_head = 0;
    spec_to_real_head = 0;

    name = strdup(arg_name);
    if(!name)
      fatal("couldn't malloc stack name (strdup)");

    bits = spec_size*8*sizeof(stack_entry_t) + real_size*8*sizeof(md_addr_t);
    type = strdup("finite RAS");
  }

  /* DESTROY */
  ~RAS_multistack_t()
  {
    free(spec_stack); spec_stack = NULL;
    free(real_stack); real_stack = NULL;
    free(name); name = NULL;
    free(type); type = NULL;
  }

  int get_size(void)
  {
     return real_size + spec_size;
  }

  /* PUSH */
  RAS_REAL_PUSH
  {
    real_head = modinc(real_head,real_size); //(head + 1) % size;
    real_stack[real_head] = ftPC;
  }

  /* POP */
  RAS_REAL_POP
  {
    md_addr_t predPC = real_stack[real_head];
    real_head = moddec(real_head,real_size); //(head - 1 + size) % size;

    return predPC;
  }


  /*SPEC_PUSH*/
  RAS_PUSH_HEADER
  {
    spec_head = modinc(spec_head,spec_size); //(head + 1) % size
    spec_to_real_head = modinc(spec_to_real_head,real_size); //(head + 1) % size
    spec_stack[spec_head].addr = ftPC;
    spec_stack[spec_head].valid = 1;
  }


  /*SPEC_POP*/
  RAS_POP_HEADER
  {
    md_addr_t predPC;
    if(spec_stack[spec_head].valid)
       predPC = spec_stack[spec_head].addr;
    else
       predPC = real_stack[spec_to_real_head];

    spec_head = moddec(spec_head,spec_size); //(head - 1 + size) % size;
    spec_to_real_head = moddec(spec_to_real_head,real_size); //(head - 1 + size) % size;
   return predPC;
  }


  /* RECOVER */
  RAS_RECOVER_HEADER
  {
    for(int i=0;i<spec_size;i++)
      spec_stack[i].valid = 0;

    spec_to_real_head = real_head;

    BPRED_STAT(num_recovers++;)
  }
};

#endif /* RAS_PARSE_ARGS */
#undef COMPONENT_NAME
